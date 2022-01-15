
#include "QmlView.h"
#include <QAction>

#if !defined(QtQuick_FOUND)

QmlView::QmlView(QString fileName, QWidget *parent)
    : QFrame(parent)
    , mFileName(fileName)
{
}

bool QmlView::load()
{
    return false;
}

#else // defined(QtQuick_FOUND)

#include "FileCache.h"
#include "Singletons.h"
#include "scripting/SessionScriptObject.h"
#include <QQuickWidget>
#include <QBoxLayout>
#include <QQmlEngine>
#include <QQmlAbstractUrlInterceptor>
#include <QNetworkAccessManager>
#include <QQmlNetworkAccessManagerFactory>
#include <QNetworkReply>
#include <QDir>
#include <cstring>

namespace
{
    QString toAbsolutePath(const QUrl &url)
    {
        auto fileName = url.toString(QUrl::RemoveScheme);
        if (fileName.startsWith("//"))
            fileName = fileName.mid(2);
#if defined(_WIN32)
        if (fileName.startsWith("/"))
            fileName = fileName.mid(1);
#endif
        return QDir::toNativeSeparators(fileName);
    }

    class UrlInterceptor : public QQmlAbstractUrlInterceptor
    {
        QUrl intercept(const QUrl &path, DataType type) override
        {
            // redirect file requests to custom NetworkAccessManager
            if (type == DataType::QmlFile && path.scheme() == "file") {
                auto cachePath = path;
                cachePath.setScheme("cache");
                return cachePath;
            }
            return path;
        }
    };

    class NetworkReply : public QNetworkReply
    {
        const QByteArray mData;
        qint64 mReadPosition{ };

    public:
        NetworkReply(QByteArray data, QObject *parent = nullptr)
            : QNetworkReply(parent)
            , mData(data)
        {
            open(ReadOnly | Unbuffered);
            setFinished(true);
        }

        qint64 size() const override
        {
            return mData.size();
        }

        qint64 readData(char *data, qint64 maxlen) override
        {
            const auto len = qMin(mData.size() - mReadPosition, maxlen);
            std::memcpy(data, mData.data() + mReadPosition, len);
            mReadPosition += len;
            return len;
        }

        void abort() override
        {
            Q_ASSERT(false);
        }
    };

    class NetworkAccessManager : public QNetworkAccessManager
    {
    public:
        using QNetworkAccessManager::QNetworkAccessManager;

        QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                         QIODevice *outgoingData) override
        {
            if (request.url().scheme() == "cache") {
                auto source = QString();
                if (Singletons::fileCache().getSource(toAbsolutePath(request.url()), &source))
                    return new NetworkReply(source.toUtf8(), this);
            }
            return QNetworkAccessManager::createRequest(op, request, outgoingData);
        }
    };

    class NetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
    {
    public:
        QNetworkAccessManager *create(QObject *parent) override
        {
            return new NetworkAccessManager(parent);
        }
    };
} // namespace

QmlView::QmlView(QString fileName, QWidget *parent)
    : QFrame(parent)
    , mFileName(fileName)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    qmlRegisterSingletonType<SessionScriptObject>("gpupad", 1, 0, "Session",
        [&](QQmlEngine *, QJSEngine *jsEngine) -> QObject * {
            return new SessionScriptObject(jsEngine);
        });
}

bool QmlView::load()
{
    mMessages.clear();

    delete mQuickWidget;
    mQuickWidget = new QQuickWidget(this);
    layout()->addWidget(mQuickWidget);

    mQuickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    connect(mQuickWidget, &QQuickWidget::statusChanged,
        [this](QQuickWidget::Status status) {
            if (status == QQuickWidget::Error) {
                const auto errors = mQuickWidget->errors();
                for (const QQmlError &error : errors)
                    mMessages += MessageList::insert(error.url().toString(),
                        error.line(), MessageType::ScriptError, error.toString());
            }
        });

    connect(mQuickWidget, &QQuickWidget::sceneGraphError,
        [this](QQuickWindow::SceneGraphError error, const QString &message) {
            mMessages += MessageList::insert(mFileName,
                0, MessageType::ScriptError, message);
        });

    static NetworkAccessManagerFactory sNetworkAccessManagerFactory;
    mQuickWidget->engine()->setNetworkAccessManagerFactory(&sNetworkAccessManagerFactory);

    static UrlInterceptor sUrlInterceptor;
    mQuickWidget->engine()->setUrlInterceptor(&sUrlInterceptor);

    Singletons::fileCache().updateEditorFiles();

    mQuickWidget->setSource(QUrl::fromLocalFile(mFileName));

    return true;
}

#endif // defined(QtQuick_FOUND)

QList<QMetaObject::Connection> QmlView::connectEditActions(
    const EditActions &actions)
{
    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(false);
    actions.undo->setEnabled(false);
    actions.redo->setEnabled(false);
    actions.cut->setEnabled(false);
    actions.copy->setEnabled(false);
    actions.paste->setEnabled(false);
    actions.delete_->setEnabled(false);
    actions.selectAll->setEnabled(false);
    actions.rename->setEnabled(false);
    actions.findReplace->setEnabled(false);
    return { };
}

QString QmlView::fileName() const
{
    return mFileName;
}

void QmlView::setFileName(QString fileName)
{
}

bool QmlView::save()
{
    return false;
}

int QmlView::tabifyGroup()
{
    return 3;
}
