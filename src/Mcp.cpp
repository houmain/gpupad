#include "Mcp.h"

#include "FileDialog.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "scripting/ScriptEngine.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>

namespace {
    constexpr auto maxMessageSize = 4 * 1024 * 1024;
    constexpr auto connectionTimeout = 1000;
    constexpr auto responseTimeout = 30000;
    constexpr auto launchTimeout = 30000;
    constexpr auto mcpReferenceUri = "gpupad://mcp";
    constexpr auto scriptingReferenceUri = "gpupad://scripting-reference";
    constexpr auto sessionReferenceUri = "gpupad://session-reference";
    constexpr auto headlessReferenceUri = "gpupad://headless";

    QString userHash()
    {
        const auto identity = QDir::homePath().toUtf8();
        return QString::fromLatin1(
            QCryptographicHash::hash(identity, QCryptographicHash::Sha256)
                .toHex()
                .first(16));
    }

    QString serverName(const QString &instanceId)
    {
        return "gpupad-script-" + userHash() + '-' + instanceId;
    }

    QString instanceRegistryDirectory()
    {
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + "/gpupad-" + userHash() + "/mcp-instances";
    }

    QString instanceRegistryFileName(const QString &instanceId)
    {
        return instanceRegistryDirectory() + '/' + instanceId + ".json";
    }

    QJsonObject readInstanceRegistry(const QString &fileName)
    {
        auto file = QFile(fileName);
        if (!file.open(QFile::ReadOnly))
            return { };
        const auto document = QJsonDocument::fromJson(file.readAll());
        return document.isObject() ? document.object() : QJsonObject{ };
    }

    bool writeInstanceRegistry(const QString &fileName,
        const QJsonObject &record)
    {
        if (!QDir().mkpath(QFileInfo(fileName).path()))
            return false;
        auto file = QSaveFile(fileName);
        if (!file.open(QFile::WriteOnly))
            return false;
        file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
        return file.commit();
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
        auto fileName = QString();
        if (uri == mcpReferenceUri)
            fileName = "mcp.md";
        else if (uri == scriptingReferenceUri)
            fileName = "scripting-reference.md";
        else if (uri == sessionReferenceUri)
            fileName = "session-reference.md";
        else if (uri == headlessReferenceUri)
            fileName = "headless.md";
        else
            return { };
        return loadApplicationTextFile(fileName);
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

    QJsonObject callInstance(QLocalSocket &socket, QString &socketInstanceId,
        const QString &instanceId, const QString &method,
        const QJsonObject &params = { }, int connectTimeout = connectionTimeout,
        int requestTimeout = responseTimeout, bool *connected = nullptr)
    {
        if (connected)
            *connected = false;
        if (socketInstanceId != instanceId) {
            socket.abort();
            socketInstanceId.clear();
        }
        if (socket.state() != QLocalSocket::ConnectedState) {
            socket.abort();
            socket.connectToServer(serverName(instanceId));
            if (!socket.waitForConnected(connectTimeout))
                return errorResponse(QJsonValue(1), -32000,
                    "GPUpad instance '" + instanceId + "' is not available");
            socketInstanceId = instanceId;
        }
        if (connected)
            *connected = true;

        const auto request = QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 1 },
            { "method", method },
            { "params", params },
        };
        socket.write(messageBytes(request));
        if (!socket.waitForBytesWritten(connectionTimeout)) {
            socket.abort();
            socketInstanceId.clear();
            return errorResponse(QJsonValue(1), -32000,
                "Sending the request to GPUpad failed");
        }

        auto response = QByteArray();
        while (!response.contains('\n')) {
            if (!socket.waitForReadyRead(requestTimeout)) {
                socket.abort();
                socketInstanceId.clear();
                return errorResponse(QJsonValue(1), -32000,
                    "GPUpad did not respond to the request");
            }
            response += socket.readAll();
            if (response.size() > maxMessageSize) {
                socket.abort();
                socketInstanceId.clear();
                return errorResponse(QJsonValue(1), -32000,
                    "GPUpad returned an oversized response");
            }
        }

        auto parseError = QJsonParseError();
        const auto document = QJsonDocument::fromJson(
            response.first(response.indexOf('\n')), &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            socket.abort();
            socketInstanceId.clear();
            return errorResponse(QJsonValue(1), -32000,
                "GPUpad returned an invalid response");
        }
        return document.object();
    }

    QJsonObject callEval(QLocalSocket &socket, QString &socketInstanceId,
        const QString &instanceId, const QString &code)
    {
        return callInstance(socket, socketInstanceId, instanceId, "eval",
            QJsonObject{ { "code", code } });
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

    QString jsonText(const QJsonValue &value)
    {
        if (value.isArray())
            return QString::fromUtf8(
                QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        return QString::fromUtf8(
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }

    struct McpState
    {
        QLocalSocket socket;
        QString socketInstanceId;
        QString selectedInstanceId;
    };

    QJsonObject instanceStatus(const QString &instanceId,
        int connectTimeout = connectionTimeout, int requestTimeout = 1000,
        bool *connected = nullptr)
    {
        auto socket = QLocalSocket();
        auto socketInstanceId = QString();
        const auto response = callInstance(socket, socketInstanceId, instanceId,
            "status", { }, connectTimeout, requestTimeout, connected);
        return response.value("result").toObject();
    }

    QJsonArray listInstances(const QString &selectedInstanceId)
    {
        auto result = QJsonArray();
        const auto directory = QDir(instanceRegistryDirectory());
        for (const auto &entry :
            directory.entryList({ "*.json" }, QDir::Files, QDir::Name)) {
            const auto fileName = directory.filePath(entry);
            auto record = readInstanceRegistry(fileName);
            const auto instanceId = record.value("instanceId").toString();
            if (!isValidMcpInstanceId(instanceId)) {
                QFile::remove(fileName);
                continue;
            }

            auto connected = false;
            if (auto status = instanceStatus(instanceId, 100, 1000, &connected);
                !status.isEmpty()) {
                status.insert("selected", instanceId == selectedInstanceId);
                result.append(status);
            } else if (connected) {
                record.insert("status", "busy");
                record.insert("selected", instanceId == selectedInstanceId);
                result.append(record);
            } else {
                QFile::remove(fileName);
            }
        }
        return result;
    }

    QString resolveInstanceId(McpState &state, QString *error)
    {
        if (!state.selectedInstanceId.isEmpty())
            return state.selectedInstanceId;

        const auto instances = listInstances({ });
        if (instances.size() == 1) {
            state.selectedInstanceId =
                instances.first().toObject().value("instanceId").toString();
            return state.selectedInstanceId;
        }
        *error = (instances.isEmpty()
                ? "No running GPUpad instance is available"
                : "Multiple GPUpad instances are available; select one first");
        return { };
    }

    QJsonObject instanceErrorResult(const QJsonValue &id,
        const QJsonObject &response)
    {
        const auto error = response.value("error").toObject();
        auto text = error.value("message").toString();
        const auto data = error.value("data").toObject();
        if (const auto stack = data.value("stack").toString(); !stack.isEmpty())
            text += "\n" + stack;
        auto content = consoleContent(data.value("console").toArray());
        content.append(QJsonObject{
            { "type", "text" },
            { "text", text },
        });
        return toolResult(id, content, true);
    }

    QJsonObject handleMcpRequest(const QJsonObject &request, McpState &state)
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
                        "Use the instance tools to launch or select GPUpad, "
                        "then use eval to access its JavaScript scripting API. "
                        "Read gpupad://mcp for bridge behavior, "
                        "gpupad://scripting-reference and "
                        "gpupad://session-reference for the API, and "
                        "gpupad://headless for session validation." },
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
                                { "uri", mcpReferenceUri },
                                { "name", "GPUpad MCP bridge" },
                                { "description", "MCP bridge usage" },
                                { "mimeType", "text/markdown" },
                            },
                            QJsonObject{
                                { "uri", scriptingReferenceUri },
                                { "name", "GPUpad scripting reference" },
                                { "description",
                                    "The scripting API reference" },
                                { "mimeType", "text/markdown" },
                            },
                            QJsonObject{
                                { "uri", sessionReferenceUri },
                                { "name", "GPUpad session item reference" },
                                { "description", "The session item reference" },
                                { "mimeType", "text/markdown" },
                            },
                            QJsonObject{
                                { "uri", headlessReferenceUri },
                                { "name", "GPUpad headless mode" },
                                { "description",
                                    "Headless execution and session "
                                    "validation" },
                                { "mimeType", "text/markdown" },
                            },
                        } },
                });
        }

        if (method == "resources/read") {
            const auto uri = params.value("uri").toString();
            if (uri != mcpReferenceUri && uri != scriptingReferenceUri
                && uri != sessionReferenceUri && uri != headlessReferenceUri)
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
            const auto emptySchema = QJsonObject{
                { "type", "object" },
                { "properties", QJsonObject() },
                { "additionalProperties", false },
            };
            const auto instanceSchema = QJsonObject{
                { "type", "object" },
                { "properties",
                    QJsonObject{
                        { "instanceId", QJsonObject{ { "type", "string" } } },
                    } },
                { "required", QJsonArray{ "instanceId" } },
                { "additionalProperties", false },
            };
            return resultResponse(id,
                QJsonObject{
                    { "tools",
                        QJsonArray{
                            QJsonObject{
                                { "name", "list" },
                                { "description",
                                    "List running GPUpad instances and show "
                                    "which one is selected." },
                                { "inputSchema", emptySchema },
                            },
                            QJsonObject{
                                { "name", "status" },
                                { "description",
                                    "Report the status of an instance, or the "
                                    "selected instance when omitted." },
                                { "inputSchema",
                                    QJsonObject{
                                        { "type", "object" },
                                        { "properties",
                                            QJsonObject{
                                                { "instanceId",
                                                    QJsonObject{
                                                        { "type", "string" },
                                                    } },
                                            } },
                                        { "additionalProperties", false },
                                    } },
                            },
                            QJsonObject{
                                { "name", "select" },
                                { "description",
                                    "Select the GPUpad instance used by "
                                    "eval." },
                                { "inputSchema", instanceSchema },
                            },
                            QJsonObject{
                                { "name", "launch" },
                                { "description",
                                    "Launch and select a new GPUpad instance. "
                                    "Arguments are passed to GPUpad; "
                                    "--instance "
                                    "is supplied by the bridge." },
                                { "inputSchema",
                                    QJsonObject{
                                        { "type", "object" },
                                        { "properties",
                                            QJsonObject{
                                                { "arguments",
                                                    QJsonObject{
                                                        { "type", "array" },
                                                        { "items",
                                                            QJsonObject{
                                                                { "type",
                                                                    "string" },
                                                            } },
                                                    } },
                                            } },
                                        { "additionalProperties", false },
                                    } },
                            },
                            QJsonObject{
                                { "name", "close" },
                                { "description",
                                    "Close an instance, or the selected "
                                    "instance when omitted." },
                                { "inputSchema",
                                    QJsonObject{
                                        { "type", "object" },
                                        { "properties",
                                            QJsonObject{
                                                { "instanceId",
                                                    QJsonObject{
                                                        { "type", "string" },
                                                    } },
                                            } },
                                        { "additionalProperties", false },
                                    } },
                            },
                            QJsonObject{
                                { "name", "eval" },
                                { "description",
                                    "Evaluate JavaScript in the selected "
                                    "GPUpad scripting engine. Read "
                                    "gpupad://mcp, "
                                    "gpupad://scripting-reference, "
                                    "gpupad://session-reference, and "
                                    "gpupad://headless before creating or "
                                    "validating sessions." },
                                { "inputSchema",
                                    QJsonObject{
                                        { "type", "object" },
                                        { "properties",
                                            QJsonObject{
                                                { "code",
                                                    QJsonObject{
                                                        { "type", "string" },
                                                        { "description",
                                                            "JavaScript source "
                                                            "to evaluate" },
                                                    } },
                                            } },
                                        { "required", QJsonArray{ "code" } },
                                        { "additionalProperties", false },
                                    } },
                            },
                        } },
                });
        }

        if (method == "tools/call") {
            const auto name = params.value("name").toString();
            const auto arguments = params.value("arguments").toObject();

            if (name == "list")
                return toolResult(id,
                    jsonText(listInstances(state.selectedInstanceId)));

            if (name == "launch") {
                const auto argumentsValue = arguments.value("arguments");
                if (!argumentsValue.isUndefined() && !argumentsValue.isArray())
                    return toolResult(id, "launch arguments must be an array",
                        true);
                const auto values = argumentsValue.toArray();
                auto childArguments = QStringList();
                for (const auto &value : values) {
                    if (!value.isString())
                        return toolResult(id,
                            "launch arguments must be strings", true);
                    const auto argument = value.toString();
                    if (argument == "--instance"
                        || argument.startsWith("--instance=")
                        || argument == "--mcp" || argument == "--help")
                        return toolResult(id,
                            "launch does not accept argument '" + argument
                                + "'",
                            true);
                    childArguments.append(argument);
                }

                const auto instanceId =
                    QUuid::createUuid().toString(QUuid::WithoutBraces);
                childArguments.prepend(instanceId);
                childArguments.prepend("--instance");
                auto process = QProcess();
                process.setProgram(QCoreApplication::applicationFilePath());
                process.setArguments(childArguments);
                process.setWorkingDirectory(QDir::currentPath());
                process.setStandardInputFile(QProcess::nullDevice());
                process.setStandardOutputFile(QProcess::nullDevice());
                process.setStandardErrorFile(QProcess::nullDevice());
                if (!process.startDetached())
                    return toolResult(id, "Launching GPUpad failed", true);

                auto timer = QElapsedTimer();
                timer.start();
                auto status = QJsonObject();
                while (timer.elapsed() < launchTimeout) {
                    status = instanceStatus(instanceId, 100, 1000);
                    if (!status.isEmpty())
                        break;
                    QThread::msleep(50);
                }
                if (status.isEmpty())
                    return toolResult(id,
                        "The launched GPUpad instance did not become ready",
                        true);

                state.socket.abort();
                state.socketInstanceId.clear();
                state.selectedInstanceId = instanceId;
                return toolResult(id, instanceId);
            }

            auto instanceId = arguments.value("instanceId").toString();
            if ((name == "status" || name == "close")
                && instanceId.isEmpty()) {
                auto error = QString();
                instanceId = resolveInstanceId(state, &error);
                if (instanceId.isEmpty())
                    return toolResult(id, error, true);
            }

            if (name == "status" || name == "select") {
                if (!isValidMcpInstanceId(instanceId))
                    return toolResult(id, "Invalid GPUpad instance ID", true);
                auto socket = QLocalSocket();
                auto socketInstanceId = QString();
                const auto response = callInstance(socket, socketInstanceId,
                    instanceId, "status");
                if (!response.value("error").toObject().isEmpty())
                    return instanceErrorResult(id, response);
                auto status = response.value("result").toObject();
                if (name == "select") {
                    state.socket.abort();
                    state.socketInstanceId.clear();
                    state.selectedInstanceId = instanceId;
                }
                status.insert("selected",
                    instanceId == state.selectedInstanceId);
                return toolResult(id, jsonText(status));
            }

            if (name == "close") {
                if (!isValidMcpInstanceId(instanceId))
                    return toolResult(id, "Invalid GPUpad instance ID", true);
                auto socket = QLocalSocket();
                auto socketInstanceId = QString();
                const auto response = callInstance(socket, socketInstanceId,
                    instanceId, "close");
                if (!response.value("error").toObject().isEmpty())
                    return instanceErrorResult(id, response);
                if (instanceId == state.selectedInstanceId) {
                    state.socket.abort();
                    state.socketInstanceId.clear();
                    state.selectedInstanceId.clear();
                }
                return toolResult(id,
                    jsonText(response.value("result")));
            }

            if (name != "eval")
                return toolResult(id, "Unknown tool", true);

            const auto codeValue = arguments.value("code");
            if (!codeValue.isString())
                return toolResult(id,
                    "The eval tool requires a string 'code' argument", true);

            auto error = QString();
            instanceId = resolveInstanceId(state, &error);
            if (instanceId.isEmpty())
                return toolResult(id, error, true);
            const auto response = callEval(state.socket, state.socketInstanceId,
                instanceId, codeValue.toString());
            if (!response.value("error").toObject().isEmpty())
                return instanceErrorResult(id, response);
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

ExternalScriptServer::ExternalScriptServer(QString instanceId, bool headless,
    CloseHandler closeHandler, QObject *parent)
    : QObject(parent)
    , mServer(new QLocalServer(this))
    , mInstanceId(instanceId.isEmpty()
              ? QUuid::createUuid().toString(QUuid::WithoutBraces)
              : std::move(instanceId))
    , mHeadless(headless)
    , mCloseHandler(std::move(closeHandler))
{
    connect(mServer, &QLocalServer::newConnection, this,
        &ExternalScriptServer::acceptConnections);
}

ExternalScriptServer::~ExternalScriptServer()
{
    if (!mRegistryFileName.isEmpty())
        QFile::remove(mRegistryFileName);
}

bool ExternalScriptServer::listen()
{
    if (!isValidMcpInstanceId(mInstanceId)) {
        mErrorString = "Invalid MCP instance ID: " + mInstanceId;
        qWarning() << mErrorString;
        return false;
    }

    const auto name = serverName(mInstanceId);
    mServer->setSocketOptions(QLocalServer::UserAccessOption);
    if (!mServer->listen(name)) {
        auto probe = QLocalSocket();
        probe.connectToServer(name);
        if (probe.waitForConnected(100)) {
            mErrorString = "The MCP instance server already exists: " + name;
            qWarning() << mErrorString;
            return false;
        }
        QLocalServer::removeServer(name);
        if (!mServer->listen(name)) {
            mErrorString = "Listening on the MCP instance server failed: "
                + mServer->errorString();
            qWarning() << mErrorString;
            return false;
        }
    }

    mRegistryFileName = instanceRegistryFileName(mInstanceId);
    const auto record = QJsonObject{
        { "instanceId", mInstanceId },
        { "pid", QCoreApplication::applicationPid() },
        { "headless", mHeadless },
        { "status", "ready" },
        { "version", QCoreApplication::applicationVersion() },
    };
    if (!writeInstanceRegistry(mRegistryFileName, record)) {
        mErrorString = "Writing the MCP instance registry failed: "
            + mRegistryFileName;
        qWarning() << mErrorString;
        mServer->close();
        return false;
    }
    return true;
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
    if (object.value("jsonrpc") != "2.0") {
        writeMessage(socket, errorResponse(id, -32601, "Method not found"));
        return;
    }

    const auto method = object.value("method").toString();
    if (method == "status") {
        const auto &sync = Singletons::synchronizeLogic();
        writeMessage(socket,
            resultResponse(id,
                QJsonObject{
                    { "instanceId", mInstanceId },
                    { "pid", QCoreApplication::applicationPid() },
                    { "headless", mHeadless },
                    { "status", "ready" },
                    { "sessionFile", sync.sessionFileName() },
                    { "version", QCoreApplication::applicationVersion() },
                }));
        return;
    }
    if (method == "close") {
        if (!mCloseHandler) {
            writeMessage(socket,
                errorResponse(id, -32003,
                    "This GPUpad instance cannot be closed remotely"));
            return;
        }
        if (!mCloseHandler()) {
            writeMessage(socket,
                errorResponse(id, -32003, "Closing GPUpad was canceled"));
            return;
        }
        writeMessage(socket,
            resultResponse(id,
                QJsonObject{
                    { "instanceId", mInstanceId },
                    { "status", "closing" },
                }));
        return;
    }
    if (method != "eval") {
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

bool isValidMcpInstanceId(const QString &instanceId)
{
    if (instanceId.isEmpty() || instanceId.size() > 128
        || !instanceId.front().isLetterOrNumber())
        return false;
    return std::all_of(instanceId.cbegin(), instanceId.cend(),
        [](QChar c) { return c.isLetterOrNumber() || c == '-' || c == '_'; });
}

int runMcp(int argc, char *argv[])
{
    auto app = QCoreApplication(argc, argv);
    auto state = McpState();
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
        writeStdout(handleMcpRequest(request, state));
    }
    return 0;
}
