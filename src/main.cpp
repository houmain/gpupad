#include "FileDialog.h"
#include "SingleApplication/singleapplication.h"
#include "Style.h"
#include "MessageList.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "FileCache.h"
#include "windows/MainWindow.h"
#include "windows/AboutDialog.h"
#include "session/SessionModel.h"
#include "scripting/ScriptEngine.h"
#include "editors/EditorManager.h"
#include "editors/IEditor.h"
#include <QApplication>
#include <QSettings>
#include <QSurfaceFormat>

#if defined(_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

// use dedicated GPUs by default
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}

// https://www.codeproject.com/Tips/76427/How-to-bring-window-to-top-with-SetForegroundWindo
void SetForegroundWindowInternal(HWND hWnd)
{
    // Press the "Alt" key
    auto ip = INPUT{ };
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = VK_MENU;
    SendInput(1, &ip, sizeof(INPUT));

    ::Sleep(100);
    ::SetForegroundWindow(hWnd);

    // Release the "Alt" key
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}

void raiseProcessPriority()
{
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

void restoreProcessPriority()
{
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
}

void attachToConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE *in, *out, *err;
    freopen_s(&in, "CONIN$", "r", stdin);
    freopen_s(&out, "CONOUT$", "w", stdout);
    freopen_s(&err, "CONOUT$", "w", stderr);
    std::fprintf(stdout, "\n");
    std::fflush(stdout);
}

#else // !_WIN32

void attachToConsole() { }

void raiseProcessPriority() { }

void restoreProcessPriority() { }

#endif

const auto singleApplicationMode = SingleApplication::Mode::User
    | SingleApplication::Mode::ExcludeAppPath
    | SingleApplication::Mode::ExcludeAppVersion;

bool forwardToInstance(int argc, char *argv[])
{
    auto app = QCoreApplication(argc, argv);
    auto instance = SingleApplication(true, singleApplicationMode);
    if (!instance.isSecondary())
        return false;

    auto arguments = app.arguments();
    arguments.removeFirst();
    if (arguments.empty()
        || std::count_if(arguments.begin(), arguments.end(),
            &FileDialog::isSessionFileName))
        return false;

    for (const auto &argument : std::as_const(arguments))
        if (!instance.sendMessage(argument.toUtf8(), 1000))
            return false;
    return true;
}

QtMessageHandler defaultMessageHandler;

void filteringMessageHandler(QtMsgType type, const QMessageLogContext &context,
    const QString &msg)
{
    if (msg
        == "QMetaObject::indexOfSignal: signal textChanged(QString) from "
           "QLineEdit redefined in ExpressionLineEdit")
        return;

    // Variable ... is used before its declaration at
    if (msg.contains("gl-matrix"))
        return;

    defaultMessageHandler(type, context, msg);
}

void outputHelpToStdout()
{
    std::fprintf(stdout,
        "%s %s (c) %s-%s by %s\n"
        "\n"
        "Usage: gpupad [--options] <filenames>\n"
        "  --headless                        run in headless mode.\n"
        "  --help                            print this help.\n"
        "\n"
        "In headless mode the following parameters are available:\n"
        "  --output <item-ident> <filename>  output an item's data to a file.\n"
        "\n"
        "All Rights Reserved.\n"
        "This program comes with absolutely no warranty.\n"
        "See the GNU General Public License, version 3 for details.\n"
        "\n",
        qUtf8Printable(QCoreApplication::applicationName()),
        qUtf8Printable(QCoreApplication::applicationVersion()),
        copyrightRangeBegin, copyrightRangeEnd, copyrightAuthor);

    std::fflush(stdout);
}

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

int runHeadless(int argc, char *argv[])
{
    auto app = QApplication(argc, argv);
    defaultMessageHandler = qInstallMessageHandler(filteringMessageHandler);
    attachToConsole();

    auto singletons = Singletons(nullptr);
    auto &editorManager = singletons.editorManager();
    auto &sessionModel = singletons.sessionModel();
    auto &synchronizeLogic = singletons.synchronizeLogic();
    auto editorsToSave = std::map<QString, IEditor *>();
    auto messages = MessagePtrSet{ };

    const auto toAbsoluteFileName = [workingDirectory = QDir::current()](
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
        synchronizeLogic.manualEvaluation();
        synchronizeLogic.finishEvaluation();
        for (auto [itemIdent, editor] : std::exchange(editorsToSave, { }))
            if (!editor->save()) {
                invalidArgument("saving item '" + itemIdent + "' failed");
                return false;
            }
        return true;
    };

    const auto closeSession = [&]() {
        editorManager.closeAllEditors(false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
            } else if (argument == "--help") {
                outputHelpToStdout();
                return 0;
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
                    loadingFileFailed(fileName);
            } else if (FileDialog::isScriptFileName(fileName)
                && singletons.fileCache().getSource(fileName, &source)) {
                singletons.defaultScriptEngine().evaluateScript(source,
                    fileName);
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

int run(int argc, char *argv[])
{
    auto app = QApplication(argc, argv);
    defaultMessageHandler = qInstallMessageHandler(filteringMessageHandler);

    QApplication::setStyle(new Style());
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, true);

    auto instance = SingleApplication(true, singleApplicationMode);
    auto window = MainWindow();

    QObject::connect(&instance, &SingleApplication::receivedMessage,
        [&](quint32 instanceId, QByteArray argument) {
            Q_UNUSED(instanceId);
            raiseProcessPriority();

            window.openFile(QString::fromUtf8(argument));
            window.setWindowState((window.windowState() & ~Qt::WindowMinimized)
                | Qt::WindowActive);
#if defined(_WIN32)
            window.ignoreNextAlt();
            SetForegroundWindowInternal(reinterpret_cast<HWND>(window.winId()));
#else
            window.raise();
            window.activateWindow();
#endif
            restoreProcessPriority();
        });

    auto arguments = QApplication::arguments();
    arguments.removeFirst();

    if (!arguments.isEmpty()) {
        window.closeAllFiles();
        for (const QString &argument : std::as_const(arguments))
            window.openFile(argument);
    }

    app.processEvents();
    restoreProcessPriority();

    return app.exec();
}

int main(int argc, char *argv[])
{
    raiseProcessPriority();

    QCoreApplication::setOrganizationName("gpupad");
    QCoreApplication::setApplicationName("GPUpad");
#if __has_include("_version.h")
    QCoreApplication::setApplicationVersion(
#  include "_version.h"
    );
#endif
#if defined(_WIN32)
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif
    // format floats independent of locale
    QLocale::setDefault(QLocale::c());

#if defined(__linux)
    // try to increase device/driver support
    // does not seem to cause any problems
    setenv("MESA_GL_VERSION_OVERRIDE", "4.5", 0);
    setenv("MESA_GLSL_VERSION_OVERRIDE", "450", 0);

    // disable video acceleration, since seeking crashes with:
    // "Failed to sync surface 0x6: 5 (invalid VAContextID)."
    // even when calling QVideoFrame::toImage in QVideoSink::videoFrameChanged
    setenv("LIBVA_DRIVER_NAME", "", 0);

    // prefer xwayland over wayland, since dragging editors can deadlock
    // pass "-platform wayland" to force wayland platform
    setenv("QT_QPA_PLATFORM", "xcb;wayland", 1);
#endif

    QApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents);

#if defined(OPENGL_ENABLED)
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    auto format = QSurfaceFormat();
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);
#endif

    if (std::string_view(argv[0]).ends_with("gpupad-headless"))
        return runHeadless(argc, argv);

    if (const auto arg1 = std::string_view(argc > 1 ? argv[1] : "");
        arg1.starts_with("--")) {

        if (arg1 == "--headless")
            return runHeadless(argc, argv);

        attachToConsole();
        outputHelpToStdout();
        return (arg1 == "--help" ? 0 : 1);
    }

    if (forwardToInstance(argc, argv))
        return 0;

    return run(argc, argv);
}
