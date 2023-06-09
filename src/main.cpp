#include "windows/MainWindow.h"
#include "SingleApplication/singleapplication.h"
#include "FileDialog.h"
#include "Style.h"
#include <QApplication>
#include <QSurfaceFormat>

#if defined(_WIN32)
// use dedicated GPUs by default
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }
extern "C" { __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001; }

// https://www.codeproject.com/Tips/76427/How-to-bring-window-to-top-with-SetForegroundWindo
void SetForegroundWindowInternal(HWND hWnd) {
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
#endif

const auto singleApplicationMode = 
    SingleApplication::Mode::User |
    SingleApplication::Mode::ExcludeAppPath |
    SingleApplication::Mode::ExcludeAppVersion;

bool forwardToInstance(int argc, char *argv[]) 
{
    auto app = QCoreApplication(argc, argv);
    auto instance = SingleApplication(true, singleApplicationMode);
    if (!instance.isSecondary())
        return false;

    auto arguments = app.arguments();
    arguments.removeFirst();
    if (arguments.empty() || std::find_if(arguments.begin(), arguments.end(), 
            &FileDialog::isSessionFileName) != arguments.end())
        return false;

    for (const auto &argument : qAsConst(arguments))
        instance.sendMessage(argument.toUtf8());
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("gpupad");
    QCoreApplication::setApplicationName("GPUpad");
#if __has_include("_version.h")
    QCoreApplication::setApplicationVersion(
# include "_version.h"
    );
#endif

    if (forwardToInstance(argc, argv))
        return 0;

#if defined(__linux)
    // try to increase device/driver support
    // does not seem to cause any problems
    setenv("MESA_GL_VERSION_OVERRIDE", "4.5", 0);
    setenv("MESA_GLSL_VERSION_OVERRIDE", "450", 0);
#endif
    // format floats independent of locale
    QLocale::setDefault(QLocale::c());

    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    auto format = QSurfaceFormat();
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    auto app = QApplication(argc, argv);

    QApplication::setStyle(new Style());
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, true);

#if defined(_WIN32)
    // workaround for consistent font size on Windows
    auto ratio = QWidget().devicePixelRatio();
    auto font = QApplication::font("QMenu");
    font.setPointSizeF(font.pointSizeF() / ratio);
    app.setFont(font);
#endif

    auto instance = SingleApplication(true, singleApplicationMode);
    auto window = MainWindow();

    QObject::connect(&instance, &SingleApplication::receivedMessage,
        [&](quint32 instanceId, QByteArray argument) {
            Q_UNUSED(instanceId);
            window.openFile(QString::fromUtf8(argument));
            window.setWindowState(
                (window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
#if defined(_WIN32)
            window.ignoreNextAlt();
            SetForegroundWindowInternal(reinterpret_cast<HWND>(window.winId()));
#else
            window.raise();
            window.activateWindow();
#endif
        });

    window.show();

    auto arguments = app.arguments();
    arguments.removeFirst();
    for (const QString &argument : qAsConst(arguments))
        window.openFile(argument);

    if (!window.hasEditor())
        window.newFile();

    return app.exec();
}
