#pragma once

#include <QObject>
#include <QHash>

class QLocalServer;
class QLocalSocket;

class ExternalScriptServer final : public QObject
{
public:
    explicit ExternalScriptServer(QObject *parent = nullptr);
    ~ExternalScriptServer() override;

    bool listen();

private:
    void acceptConnections();
    void readRequest(QLocalSocket *socket);
    void processRequest(QLocalSocket *socket, const QByteArray &request);

    QLocalServer *mServer;
    QHash<QLocalSocket *, QByteArray> mBuffers;
};

int runMcp(int argc, char *argv[]);
