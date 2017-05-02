#include "MainWindow.h"
#include "SingleApplication/singleapplication.h"
#include <QApplication>

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
#include "version.h"
    );
    app.setOrganizationName("GPUpad");
    app.setApplicationName("GPUpad");

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

    return app.exec();
}
