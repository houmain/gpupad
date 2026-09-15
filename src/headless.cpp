
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

void outputMessagesToStdout()
{
    for (const auto &message : MessagePtrSet::getAllMessages()) {
        const auto severity = getMessageSeverity(*message);
        const auto severityText = (severity == MessageSeverity::Error
                ? "ERROR: "
                : severity == MessageSeverity::Warning ? "WARNING: "
                : severity == MessageSeverity::Info    ? "INFO: "
                                                       : "");
        const auto format = message->fileName.isEmpty() ? "%s%s.\n"
            : message->line <= 0                        ? "%s%s%s%s\n"
                                                        : "%s%s%s%s:%i\n";
        std::fprintf(stdout, format, severityText,
            qUtf8Printable(getMessageText(*message)), "\n  in ",
            qUtf8Printable(FileDialog::getFileTitle(message->fileName)),
            message->line);
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
        sessionModel.clear();
        outputMessagesToStdout();
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
