#include "MainWindow.h"
#include "SingleApplication/singleapplication.h"
#include "render/CompositorSync.h"
#include "FileDialog.h"
#include <QApplication>
#include <QStyleFactory>

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

int main(int argc, char *argv[])
{
#if defined(__linux)
    // try to increase device/driver support
    // does not seem to cause any problems
    setenv("MESA_GL_VERSION_OVERRIDE", "4.5", 0);
    setenv("MESA_GLSL_VERSION_OVERRIDE", "450", 0);
#endif

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    initializeCompositorSync();

    SingleApplication app(argc, argv, true);
    auto arguments = app.arguments();
    arguments.removeFirst();

    if(app.isSecondary() && !arguments.empty()) {
        const auto openingSessionFile = (std::count_if(
            arguments.begin(), arguments.end(), [](const QString &argument) { 
                    return FileDialog::isSessionFileName(argument); 
                }) != 0);
        
        if (!openingSessionFile) {
            for (const auto &argument : qAsConst(arguments))
                app.sendMessage(argument.toUtf8());
            return 0;
        }
    }

    app.setApplicationVersion(
#include "_version.h"
    );
    app.setOrganizationName("gpupad");
    app.setApplicationName("GPUpad");

    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    QObject::connect(&app, &SingleApplication::receivedMessage,
        [&](quint32 instanceId, QByteArray argument) {
            Q_UNUSED(instanceId);
            window.openFile(QString::fromUtf8(argument));
            window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);

#if defined(_WIN32)
            SetForegroundWindowInternal(reinterpret_cast<HWND>(window.winId()));
#else
            window.raise();
            window.activateWindow();
#endif
        });

    for (const QString &argument : qAsConst(arguments))
        window.openFile(argument);

    if (!window.hasEditor())
        window.newFile();

    return app.exec();
}
