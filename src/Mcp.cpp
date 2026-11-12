#include "Mcp.h"

#include "FileDialog.h"
#include "Singletons.h"
#include "scripting/ScriptEngine.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>

namespace {
    constexpr auto maxMessageSize = 4 * 1024 * 1024;
    constexpr auto connectionTimeout = 1000;
    constexpr auto responseTimeout = 30000;
    constexpr auto scriptingReferenceUri = "gpupad://scripting-reference";
    constexpr auto sessionReferenceUri = "gpupad://session-reference";

    QString serverName()
    {
        const auto identity = QDir::homePath().toUtf8();
        const auto hash =
            QCryptographicHash::hash(identity, QCryptographicHash::Sha256)
                .toHex()
                .first(16);
        return "gpupad-script-" + QString::fromLatin1(hash);
    }

    QJsonObject errorResponse(const QJsonValue &id, int code,
        const QString &message, const QJsonValue &data = { })
    {
        auto error = QJsonObject{
            { "code", code },
            { "message", message },
        };
        if (!data.isUndefined())
            error.insert("data", data);
        return {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "error", error },
        };
    }

    QJsonObject resultResponse(const QJsonValue &id, const QJsonValue &result)
    {
        return {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "result", result },
        };
    }

    QString referenceContent(const QString &uri)
    {
        auto resourceName = QString();
        if (uri == scriptingReferenceUri)
            resourceName = ":/docs/scripting-reference.md";
        else if (uri == sessionReferenceUri)
            resourceName = ":/docs/session-reference.md";
        else
            return { };

        auto file = QFile(resourceName);
        if (!file.open(QFile::ReadOnly | QFile::Text))
            return { };
        return QString::fromUtf8(file.readAll());
    }

    std::optional<QJsonValue> toJson(ScriptEngine &engine,
        const QJSValue &value, QString *error)
    {
        if (value.isUndefined())
            return QJsonValue(QJsonValue::Undefined);
        if (value.isNull())
            return QJsonValue(QJsonValue::Null);
        if (value.isBool())
            return QJsonValue(value.toBool());
        if (value.isNumber()) {
            const auto number = value.toNumber();
            if (!std::isfinite(number)) {
                *error = "Evaluation result contains a non-finite number";
                return std::nullopt;
            }
            return QJsonValue(number);
        }
        if (value.isString())
            return QJsonValue(value.toString());
        if (value.isCallable()) {
            *error = "Evaluation result contains a function";
            return std::nullopt;
        }
        if (!value.isObject()) {
            *error = "Evaluation result is not JSON-compatible";
            return std::nullopt;
        }

        auto wrapper = engine.jsEngine().newArray(1);
        wrapper.setProperty(0, value);
        auto stringify = engine.getGlobal("JSON").property("stringify");
        const auto serialized = engine.call(stringify, { wrapper }, 0);
        if (serialized.isError()) {
            *error = serialized.property("message").toString();
            if (error->isEmpty())
                *error = serialized.toString();
            return std::nullopt;
        }
        if (!serialized.isString()) {
            *error = "Evaluation result is not JSON-compatible";
            return std::nullopt;
        }

        const auto bytes = serialized.toString().toUtf8();
        if (bytes.size() > maxMessageSize) {
            *error = "Evaluation result is too large";
            return std::nullopt;
        }
        auto parseError = QJsonParseError();
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()
            || document.array().size() != 1) {
            *error = "Serializing the evaluation result failed";
            return std::nullopt;
        }
        return document.array().first();
    }

    QJsonArray messagesSince(const MessageList &messages, int first)
    {
        auto result = QJsonArray();
        auto index = 0;
        for (const auto &message : messages) {
            if (index++ < first)
                continue;
            result.append(QJsonObject{
                { "severity", getMessageSeverityText(*message) },
                { "text", getMessageText(*message) },
                { "fileName", message->fileName },
                { "line", message->line },
                { "itemId", message->itemId },
            });
        }
        return result;
    }

    QByteArray messageBytes(const QJsonObject &message)
    {
        auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        return bytes;
    }

    void writeMessage(QLocalSocket *socket, const QJsonObject &message)
    {
        socket->write(messageBytes(message));
        socket->flush();
    }

    void writeStdout(const QJsonObject &message)
    {
        const auto bytes = messageBytes(message);
        std::fwrite(bytes.constData(), 1,
            static_cast<std::size_t>(bytes.size()), stdout);
        std::fflush(stdout);
    }

    QJsonObject callEval(QLocalSocket &socket, const QString &code)
    {
        if (socket.state() != QLocalSocket::ConnectedState) {
            socket.abort();
            socket.connectToServer(serverName());
            if (!socket.waitForConnected(connectionTimeout))
                return errorResponse(QJsonValue(1), -32000,
                    "No running GPUpad instance is available");
        }

        const auto request = QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 1 },
            { "method", "eval" },
            { "params", QJsonObject{ { "code", code } } },
        };
        socket.write(messageBytes(request));
        if (!socket.waitForBytesWritten(connectionTimeout))
            return errorResponse(QJsonValue(1), -32000,
                "Sending the request to GPUpad failed");

        auto response = QByteArray();
        while (!response.contains('\n')) {
            if (!socket.waitForReadyRead(responseTimeout))
                return errorResponse(QJsonValue(1), -32000,
                    "GPUpad did not respond to the request");
            response += socket.readAll();
            if (response.size() > maxMessageSize)
                return errorResponse(QJsonValue(1), -32000,
                    "GPUpad returned an oversized response");
        }

        auto parseError = QJsonParseError();
        const auto document = QJsonDocument::fromJson(
            response.first(response.indexOf('\n')), &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject())
            return errorResponse(QJsonValue(1), -32000,
                "GPUpad returned an invalid response");
        return document.object();
    }

    QString valueText(const QJsonObject &result)
    {
        if (result.value("valueType") == "undefined")
            return "undefined";

        const auto value = result.value("value");
        if (value.isString())
            return value.toString();
        if (value.isNull())
            return "null";
        if (value.isBool())
            return value.toBool() ? "true" : "false";
        if (value.isDouble())
            return QString::number(value.toDouble(), 'g', 17);
        if (value.isArray())
            return QString::fromUtf8(
                QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        if (value.isObject())
            return QString::fromUtf8(
                QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        return { };
    }

    QJsonArray consoleContent(const QJsonArray &messages)
    {
        auto content = QJsonArray();
        for (const auto &value : messages) {
            const auto message = value.toObject();
            auto text = message.value("text").toString();
            const auto severity = message.value("severity").toString();
            if (severity != "INFO")
                text = '[' + severity + "] " + text;
            content.append(QJsonObject{
                { "type", "text" },
                { "text", text },
            });
        }
        return content;
    }

    QJsonObject toolResult(const QJsonValue &id, QJsonArray content,
        bool isError = false)
    {
        auto result = QJsonObject{ { "content", content } };
        if (isError)
            result.insert("isError", true);
        return resultResponse(id, result);
    }

    QJsonObject toolResult(const QJsonValue &id, const QString &text,
        bool isError = false)
    {
        return toolResult(id,
            QJsonArray{ QJsonObject{
                { "type", "text" },
                { "text", text },
            } },
            isError);
    }

    QJsonObject handleMcpRequest(const QJsonObject &request,
        QLocalSocket &socket)
    {
        const auto id = request.value("id");
        const auto method = request.value("method").toString();
        const auto params = request.value("params").toObject();

        if (method == "initialize") {
            const auto requestedVersion =
                params.value("protocolVersion").toString();
            const auto protocolVersion =
                requestedVersion.isEmpty() ? "2025-03-26" : requestedVersion;
            return resultResponse(id,
                QJsonObject{
                    { "protocolVersion", protocolVersion },
                    { "capabilities",
                        QJsonObject{
                            { "tools", QJsonObject() },
                            { "resources", QJsonObject() },
                        } },
                    { "serverInfo",
                        QJsonObject{
                            { "name", "gpupad" },
                            { "version",
                                QCoreApplication::applicationVersion() },
                        } },
                    { "instructions",
                        "Use the eval tool to access GPUpad's JavaScript "
                        "scripting API. Read gpupad://scripting-reference and "
                        "gpupad://session-reference for the API and session "
                        "item references." },
                });
        }

        if (method == "ping")
            return resultResponse(id, QJsonObject());

        if (method == "resources/list") {
            return resultResponse(id,
                QJsonObject{
                    { "resources",
                        QJsonArray{
                            QJsonObject{
                                { "uri", scriptingReferenceUri },
                                { "name", "GPUpad scripting reference" },
                                { "description",
                                    "The scripting API reference from "
                                    "docs/scripting-reference.md" },
                                { "mimeType", "text/markdown" },
                            },
                            QJsonObject{
                                { "uri", sessionReferenceUri },
                                { "name", "GPUpad session item reference" },
                                { "description",
                                    "The session item reference from "
                                    "docs/session-reference.md" },
                                { "mimeType", "text/markdown" },
                            },
                        } },
                });
        }

        if (method == "resources/read") {
            const auto uri = params.value("uri").toString();
            if (uri != scriptingReferenceUri && uri != sessionReferenceUri)
                return errorResponse(id, -32602, "Unknown resource URI");
            const auto reference = referenceContent(uri);
            if (reference.isEmpty())
                return errorResponse(id, -32000,
                    "Loading the reference failed");
            return resultResponse(id,
                QJsonObject{
                    { "contents",
                        QJsonArray{ QJsonObject{
                            { "uri", uri },
                            { "mimeType", "text/markdown" },
                            { "text", reference },
                        } } },
                });
        }

        if (method == "tools/list") {
            const auto inputSchema = QJsonObject{
                { "type", "object" },
                { "properties",
                    QJsonObject{
                        { "code",
                            QJsonObject{
                                { "type", "string" },
                                { "description",
                                    "JavaScript source to evaluate" },
                            } },
                    } },
                { "required", QJsonArray{ "code" } },
                { "additionalProperties", false },
            };
            return resultResponse(id,
                QJsonObject{
                    { "tools",
                        QJsonArray{ QJsonObject{
                            { "name", "eval" },
                            { "description",
                                "Evaluate JavaScript in the running GPUpad "
                                "scripting engine. See the "
                                "gpupad://scripting-reference and "
                                "gpupad://session-reference resources." },
                            { "inputSchema", inputSchema },
                        } } },
                });
        }

        if (method == "tools/call") {
            if (params.value("name") != "eval")
                return toolResult(id, "Unknown tool", true);
            const auto arguments = params.value("arguments").toObject();
            const auto codeValue = arguments.value("code");
            if (!codeValue.isString())
                return toolResult(id,
                    "The eval tool requires a string 'code' argument", true);

            const auto response = callEval(socket, codeValue.toString());
            if (const auto error = response.value("error").toObject();
                !error.isEmpty()) {
                auto text = error.value("message").toString();
                const auto data = error.value("data").toObject();
                if (const auto stack = data.value("stack").toString();
                    !stack.isEmpty())
                    text += "\n" + stack;
                auto content = consoleContent(data.value("console").toArray());
                content.append(QJsonObject{
                    { "type", "text" },
                    { "text", text },
                });
                return toolResult(id, content, true);
            }
            const auto result = response.value("result").toObject();
            auto content = consoleContent(result.value("console").toArray());
            if (content.isEmpty() || result.value("valueType") != "undefined")
                content.append(QJsonObject{
                    { "type", "text" },
                    { "text", valueText(result) },
                });
            return toolResult(id, content);
        }

        return errorResponse(id, -32601, "Method not found");
    }
} // namespace

ExternalScriptServer::ExternalScriptServer(QObject *parent)
    : QObject(parent)
    , mServer(new QLocalServer(this))
{
    connect(mServer, &QLocalServer::newConnection, this,
        &ExternalScriptServer::acceptConnections);
}

ExternalScriptServer::~ExternalScriptServer() = default;

bool ExternalScriptServer::listen()
{
    mServer->setSocketOptions(QLocalServer::UserAccessOption);
    if (mServer->listen(serverName()))
        return true;

    auto probe = QLocalSocket();
    probe.connectToServer(serverName());
    if (probe.waitForConnected(100))
        return false;

    QLocalServer::removeServer(serverName());
    return mServer->listen(serverName());
}

void ExternalScriptServer::acceptConnections()
{
    while (auto socket = mServer->nextPendingConnection()) {
        mBuffers.insert(socket, { });
        connect(socket, &QLocalSocket::readyRead, this,
            [this, socket]() { readRequest(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            mBuffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void ExternalScriptServer::readRequest(QLocalSocket *socket)
{
    auto &buffer = mBuffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > maxMessageSize) {
        writeMessage(socket,
            errorResponse(QJsonValue::Null, -32600, "Request is too large"));
        socket->disconnectFromServer();
        return;
    }

    for (;;) {
        const auto newline = buffer.indexOf('\n');
        if (newline < 0)
            break;
        const auto request = buffer.first(newline);
        buffer.remove(0, newline + 1);
        if (!request.trimmed().isEmpty())
            processRequest(socket, request);
    }
}

void ExternalScriptServer::processRequest(QLocalSocket *socket,
    const QByteArray &request)
{
    auto parseError = QJsonParseError();
    const auto document = QJsonDocument::fromJson(request, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        writeMessage(socket,
            errorResponse(QJsonValue::Null, -32700, "Parse error"));
        return;
    }

    const auto object = document.object();
    const auto id = object.value("id");
    if (object.value("jsonrpc") != "2.0" || object.value("method") != "eval") {
        writeMessage(socket, errorResponse(id, -32601, "Method not found"));
        return;
    }

    const auto params = object.value("params").toObject();
    const auto codeValue = params.value("code");
    if (!codeValue.isString()) {
        writeMessage(socket,
            errorResponse(id, -32602, "A string 'code' parameter is required"));
        return;
    }

    const auto fileName =
        toNativeCanonicalAbsoluteFilePath(".gpupad-mcp-eval.js");
    auto &engine = Singletons::defaultScriptEngine();
    const auto firstMessage = engine.messages().size();
    const auto value = engine.evaluateScript(codeValue.toString(), fileName);
    if (value.isError()) {
        auto data = QJsonObject();
        data.insert("fileName", value.property("fileName").toString());
        data.insert("lineNumber", value.property("lineNumber").toInt());
        data.insert("stack", value.property("stack").toString());
        data.insert("console", messagesSince(engine.messages(), firstMessage));
        writeMessage(socket,
            errorResponse(id, -32001, value.property("message").toString(),
                data));
        return;
    }

    auto conversionError = QString();
    const auto converted = toJson(engine, value, &conversionError);
    if (!converted) {
        auto data = QJsonObject{
            { "console", messagesSince(engine.messages(), firstMessage) },
        };
        writeMessage(socket, errorResponse(id, -32002, conversionError, data));
        return;
    }

    auto result = QJsonObject();
    result.insert("console", messagesSince(engine.messages(), firstMessage));
    if (converted->isUndefined()) {
        result.insert("valueType", "undefined");
    } else {
        result.insert("value", *converted);
    }
    const auto response = resultResponse(id, result);
    if (messageBytes(response).size() > maxMessageSize) {
        writeMessage(socket,
            errorResponse(id, -32002, "Evaluation result is too large"));
        return;
    }
    writeMessage(socket, response);
}

int runMcp(int argc, char *argv[])
{
    auto app = QCoreApplication(argc, argv);
    auto socket = QLocalSocket();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.size() > maxMessageSize) {
            writeStdout(errorResponse(QJsonValue::Null, -32600,
                "Request is too large"));
            continue;
        }

        auto parseError = QJsonParseError();
        const auto document = QJsonDocument::fromJson(
            QByteArray::fromStdString(line), &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            writeStdout(errorResponse(QJsonValue::Null, -32700, "Parse error"));
            continue;
        }

        const auto request = document.object();
        if (!request.contains("id"))
            continue;
        writeStdout(handleMcpRequest(request, socket));
    }
    return 0;
}
