
#include "FileDialog.h"
#include "MessageList.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include "scripting/ScriptEngine.h"
#include "editors/EditorManager.h"
#include "editors/IEditor.h"
#include <QApplication>

void outputMessagesToStdout(const SessionModel &sessionModel, bool verbose)
{
    for (const auto &message : MessagePtrSet::getAllMessages()) {
        const auto severity = getMessageSeverity(*message);
        if (severity == MessageSeverity::Info && !verbose)
            continue;
        const auto severityText = (severity == MessageSeverity::Error ? "ERROR"
                : severity == MessageSeverity::Warning ? "WARNING"
                                                       : "INFO");
        auto text = QString::fromLatin1(severityText);
        if (!message->fileName.isEmpty()) {
            text += " in '" + FileDialog::getFileTitle(message->fileName) + "'";
            if (message->line > 0)
                text += ":" + QString::number(message->line);
        }
        if (message->itemId)
            if (const auto item = sessionModel.findItem(message->itemId))
                text += " " + item->name;
        text += ": ";
        text += getMessageText(*message);
        text += "\n";
        std::fputs(qUtf8Printable(text), stdout);
    }
    std::fflush(stdout);
}

int runHeadless(QApplication &app)
{
    auto singletons = Singletons(nullptr);
    auto &editorManager = singletons.editorManager();
    auto &sessionModel = singletons.sessionModel();
    auto &synchronizeLogic = singletons.synchronizeLogic();
    auto editorsToSave = std::map<QString, IEditor *>();
    auto messages = MessagePtrSet{ };
    auto verbose = false;

    const auto workingDirectory = QDir::current();
    const auto toAbsoluteFileName = [workingDirectory](
                                        const QString &fileName) {
        return toNativeCanonicalFilePath(
            workingDirectory.absoluteFilePath(fileName));
    };
    const auto loadingFileFailed = [&](QString filename) {
        messages.insert(MessageType::LoadingFileFailed, filename);
        return 1;
    };
    const auto invalidArgument = [&](QString message) {
        messages.insert(MessageType::InvalidCommandlineArguments, message);
        return 1;
    };

    const auto evaluateSession = [&]() {
        synchronizeLogic.resetEvaluation();
        synchronizeLogic.finishEvaluation();
        for (const auto &message : MessagePtrSet::getAllMessages())
            if (getMessageSeverity(*message) == MessageSeverity::Error)
                return false;
        for (auto [itemIdent, editor] : std::exchange(editorsToSave, { }))
            if (!editor->save()) {
                invalidArgument("saving item '" + itemIdent + "' failed");
                return false;
            }
        return true;
    };

    const auto closeSession = [&]() {
        editorManager.closeAllEditors(false);
        outputMessagesToStdout(sessionModel, verbose);
        sessionModel.clear();
        messages.clear();
    };

    const auto cleanup = qScopeGuard([&]() {
        closeSession();
        synchronizeLogic.resetRenderSession();
    });

    auto arguments = app.arguments();
    arguments.removeFirst();
    for (auto i = 0; i < arguments.size(); ++i) {
        const auto &argument = arguments[i];

        const auto checkParameterCount = [&](int count) {
            if (i + count >= arguments.size())
                return false;
            for (auto j = 0; j < count; ++j)
                if (arguments[i + 1 + j].startsWith("--"))
                    return false;
            return true;
        };

        if (argument.startsWith("--")) {
            if (argument == "--headless") {
                continue;

            } else if (argument == "--verbose") {
                verbose = true;

            } else if (argument == "--set") {
                if (!checkParameterCount(2))
                    return invalidArgument("missing parameter to " + argument);

                const auto ident = arguments[++i];
                const auto value = arguments[++i];
                singletons.defaultScriptEngine().setGlobal(ident, value);

            } else if (argument == "--output") {
                if (!checkParameterCount(2))
                    return invalidArgument("missing parameter to " + argument);

                const auto itemIdent = arguments[++i];
                const auto fileName = toAbsoluteFileName(arguments[++i]);
                auto ok = false;
                const auto id = itemIdent.toInt(&ok);
                const auto *item = (ok
                        ? sessionModel.findItem(id)
                        : sessionModel.findItemByPath(itemIdent));
                if (!item)
                    return invalidArgument(
                        "item '" + itemIdent + "' not found");

                const auto index = sessionModel.getIndex(item,
                    SessionModel::ColumnType::FileName);
                if (sessionModel.setData(index, fileName))
                    if (const auto fileItem = castItem<FileItem>(item))
                        if (auto editor = editorManager.openEditor(*fileItem)) {
                            editorsToSave[itemIdent] = editor;
                            continue;
                        }
                return invalidArgument("invalid file item '" + itemIdent + "'");
            } else {
                return invalidArgument("unknown option " + argument);
            }
        } else {
            const auto fileName = toAbsoluteFileName(argument);
            auto source = QString();
            if (FileDialog::isSessionFileName(fileName)) {
                if (!evaluateSession())
                    return 1;
                closeSession();
                if (!sessionModel.load(fileName))
                    return loadingFileFailed(fileName);
                // Session loading changes cwd for the GUI. Keep headless output
                // paths relative to the directory from which it was invoked.
                QDir::setCurrent(workingDirectory.path());
            } else if (FileDialog::isScriptFileName(fileName)
                && singletons.fileCache().getSource(fileName, &source)) {
                singletons.defaultScriptEngine().evaluateScript(source,
                    fileName);
                messages += singletons.defaultScriptEngine().resetMessages();
                for (const auto &message : MessagePtrSet::getAllMessages())
                    if (getMessageSeverity(*message) == MessageSeverity::Error)
                        return 1;
            } else {
                if (!editorManager.openEditor(fileName))
                    return loadingFileFailed(fileName);
            }
        }
    }
    if (!evaluateSession())
        return 1;
    return 0;
}
