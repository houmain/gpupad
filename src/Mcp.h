#pragma once

#include <QObject>
#include <QHash>
#include <QString>

#include <functional>

class QLocalServer;
class QLocalSocket;

class ExternalScriptServer final : public QObject
{
public:
    using CloseHandler = std::function<bool()>;

    explicit ExternalScriptServer(QString instanceId = { },
        bool headless = false, CloseHandler closeHandler = { },
        QObject *parent = nullptr);
    ~ExternalScriptServer() override;

    bool listen();
    const QString &instanceId() const { return mInstanceId; }
    const QString &errorString() const { return mErrorString; }

private:
    void acceptConnections();
    void readRequest(QLocalSocket *socket);
    void processRequest(QLocalSocket *socket, const QByteArray &request);

    QLocalServer *mServer;
    QHash<QLocalSocket *, QByteArray> mBuffers;
    QString mInstanceId;
    QString mRegistryFileName;
    QString mErrorString;
    bool mHeadless{};
    CloseHandler mCloseHandler;
};

bool isValidMcpInstanceId(const QString &instanceId);
int runMcp(int argc, char *argv[]);
