#include "MainWindow.h"
#include "KDSingleApplication/kdsingleapplication.h"
#include "render/CompositorSync.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFileInfo>

#if defined(_WIN32)
// use dedicated GPUs by default
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }
extern "C" { __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001; }
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

    QApplication app(argc, argv);
    app.setApplicationVersion(
#include "_version.h"
    );
    app.setOrganizationName("gpupad");
    app.setApplicationName("GPUpad");

    auto arguments = app.arguments();
    arguments.removeFirst();

    KDSingleApplication kdsa;

    if (!kdsa.isPrimaryInstance() && !arguments.empty()) {
        for (const auto &argument : qAsConst(arguments)) {
            const auto file = QFileInfo(argument);
            if (file.exists())
                kdsa.sendMessage(file.absoluteFilePath().toUtf8());
        }
        return 0;
    }

    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    QObject::connect(&kdsa, &KDSingleApplication::messageReceived,
        [&](QByteArray argument) {
            window.openFile(QString::fromUtf8(argument));
            window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            window.raise();
            window.activateWindow();
        });

    for (const QString &argument : qAsConst(arguments))
        window.openFile(argument);

    if (!window.hasEditor())
        window.newFile();

    return app.exec();
}
