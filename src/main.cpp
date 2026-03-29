#include "FileDialog.h"
#include "SingleApplication/singleapplication.h"
#include "Style.h"
#include "windows/MainWindow.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "session/SessionModel.h"
#include "scripting/ScriptEngine.h"
#include "editors/EditorManager.h"
#include "FileCache.h"
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
    auto ip = INPUT{};
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

bool attachToConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return false;
    FILE *in, *out, *err;
    freopen_s(&in, "CONIN$", "r", stdin);
    freopen_s(&out, "CONOUT$", "w", stdout);
    freopen_s(&err, "CONOUT$", "w", stderr);
    return true;
}

#else // !_WIN32

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

void outputMessagesToStdout()
{
#if defined(_WIN32)
    attachToConsole();
#endif
    for (const auto &message : MessageList::messages()) {
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
    auto arguments = app.arguments();
    arguments.removeFirst();

    auto singletons = Singletons(nullptr);
    for (const auto &argument : std::as_const(arguments)) {
        auto messages = MessagePtrSet{};
        const auto fileName =
            toNativeCanonicalFilePath(QFileInfo(argument).absoluteFilePath());
        if (FileDialog::isSessionFileName(argument)) {
            if (!singletons.sessionModel().load(fileName))
                messages += MessageList::insert(0,
                    MessageType::LoadingFileFailed, fileName);
        } else if (FileDialog::isScriptFileName(argument)) {
            auto source = QString();
            if (Singletons::fileCache().getSource(fileName, &source)) {
                singletons.defaultScriptEngine().evaluateScript(source,
                    fileName);
            } else {
                messages += MessageList::insert(0,
                    MessageType::LoadingFileFailed, fileName);
            }
        }
        singletons.synchronizeLogic().manualEvaluation();
        singletons.synchronizeLogic().finishEvaluation();
        outputMessagesToStdout();
        singletons.editorManager().saveAllEditors();
        singletons.editorManager().closeAllEditors(false);
        singletons.sessionModel().clear();
        singletons.synchronizeLogic().resetRenderSession();
    }
    return 0;
}

int run(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents);

    auto format = QSurfaceFormat();
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

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

    app.processEvents();
    window.show();
    app.processEvents();

    auto arguments = QApplication::arguments();
    arguments.removeFirst();
    for (const QString &argument : std::as_const(arguments))
        window.openFile(argument);

    if (!window.hasEditor())
        window.newFile();

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
#endif

    if (std::string_view(argv[0]).ends_with("gpupad-headless"))
        return runHeadless(argc, argv);

    if (argc > 1 && std::string_view(argv[1]) == "--headless") {
        argc = std::distance(argv, std::remove(argv, argv + argc, argv[1]));
        return runHeadless(argc, argv);
    }

    if (forwardToInstance(argc, argv))
        return 0;

    return run(argc, argv);
}
