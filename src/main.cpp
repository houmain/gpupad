#include "MainWindow.h"
#include "SingleApplication/singleapplication.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    SingleApplication app(argc, argv, true);
    auto arguments = app.arguments();
    arguments.removeFirst();

    if(app.isSecondary() && !arguments.empty()) {
        foreach (QString argument, arguments)
            app.sendMessage(argument.toUtf8());
        return 0;
    }

    app.setApplicationVersion(
#include "_version.h"
    );
    app.setOrganizationName("GPUpad");
    app.setApplicationName("GPUpad");

    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    QObject::connect(&app, &SingleApplication::receivedMessage,
        [&](quint32 instanceId, QByteArray argument) {
            Q_UNUSED(instanceId);
            window.openFile(QString::fromUtf8(argument));
            window.raise();
        });

    foreach (QString argument, arguments)
        window.openFile(argument);

    if (!window.hasEditor())
        window.newFile();

    return app.exec();
}
