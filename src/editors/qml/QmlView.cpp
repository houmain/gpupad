
#include "QmlView.h"
#include <QAction>

#if !defined(QtQuick_FOUND)

QmlView::QmlView(QString fileName, QWidget *parent)
    : QFrame(parent)
    , mFileName(fileName)
{
}

void QmlView::reset() { }

#else // defined(QtQuick_FOUND)

#  include "FileCache.h"
#  include "Singletons.h"
#  include "scripting/objects/AppScriptObject.h"
#  include <QBoxLayout>
#  include <QDir>
#  include <QFileInfo>
#  include <QNetworkAccessManager>
#  include <QNetworkReply>
#  include <QQmlAbstractUrlInterceptor>
#  include <QQmlNetworkAccessManagerFactory>
#  include <QQmlEngine>
#  include <QQuickWidget>
#  include <cstring>

namespace {
    QString toAbsolutePath(const QUrl &url)
    {
        auto fileName = url.toString(QUrl::RemoveScheme);
        if (fileName.startsWith("//"))
            fileName = fileName.mid(2);
#  if defined(_WIN32)
        if (fileName.startsWith("/"))
            fileName = fileName.mid(1);
#  endif
        return QDir::toNativeSeparators(fileName);
    }

    class UrlInterceptor : public QQmlAbstractUrlInterceptor
    {
        QUrl intercept(const QUrl &path, DataType type) override
        {
            // redirect file requests to custom NetworkAccessManager
            if (path.isLocalFile()
                && (type == DataType::QmlFile
                    || type == DataType::JavaScriptFile)) {
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
        qint64 mReadPosition{};

    public:
        NetworkReply(QByteArray data, QObject *parent = nullptr)
            : QNetworkReply(parent)
            , mData(data)
        {
            open(ReadOnly | Unbuffered);
            setFinished(true);
        }

        qint64 size() const override { return mData.size(); }

        qint64 readData(char *data, qint64 maxlen) override
        {
            const auto len = qMin(mData.size() - mReadPosition, maxlen);
            std::memcpy(data, mData.data() + mReadPosition, len);
            mReadPosition += len;
            return len;
        }

        void abort() override { Q_ASSERT(false); }
    };

    class NetworkAccessManager : public QNetworkAccessManager
    {
    private:
        QmlView &mView;

    public:
        NetworkAccessManager(QmlView *view, QObject *parent)
            : QNetworkAccessManager(parent)
            , mView(*view)
        {
        }

        QNetworkReply *createRequest(Operation op,
            const QNetworkRequest &request, QIODevice *outgoingData) override
        {
            if (request.url().scheme() == "cache") {
                const auto fileName = toAbsolutePath(request.url());
                mView.addDependency(fileName);
                auto source = QString();
                if (Singletons::fileCache().getSource(fileName, &source))
                    return new NetworkReply(source.toUtf8(), this);
            }
            return QNetworkAccessManager::createRequest(op, request,
                outgoingData);
        }
    };

    class NetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
    {
    private:
        QmlView &mView;

    public:
        NetworkAccessManagerFactory(QmlView *view) : mView(*view) { }

        QNetworkAccessManager *create(QObject *parent) override
        {
            return new NetworkAccessManager(&mView, parent);
        }
    };
} // namespace

QmlView::QmlView(QString fileName, QWidget *parent)
    : QFrame(parent)
    , mFileName(fileName)
{
    // WORKAROUND: tell QQuickWidget to also use OpenGL for rendering and not turn black
    // see: https://forum.qt.io/topic/148089/qopenglwidget-doesn-t-work-with-qquickwidget
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    qmlRegisterSingletonType<AppScriptObject>("gpupad", 1, 0, "app",
        [&](QQmlEngine *, QJSEngine *jsEngine) -> QObject * {
            return new AppScriptObject(jsEngine);
        });
}

void QmlView::reset()
{
    if (mQuickWidget) {
        layout()->removeWidget(mQuickWidget);
        connect(mQuickWidget, &QQuickWidget::destroyed, this, &QmlView::reset,
            Qt::QueuedConnection);
        mQuickWidget->deleteLater();
        mQuickWidget = nullptr;
        return;
    }

    mMessages.clear();

    mQuickWidget = new QQuickWidget(this);
    layout()->addWidget(mQuickWidget);

    mQuickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    connect(mQuickWidget, &QQuickWidget::statusChanged,
        [this, widget = mQuickWidget](QQuickWidget::Status status) {
            if (status == QQuickWidget::Error) {
                const auto errors = widget->errors();
                for (const QQmlError &error : errors)
                    mMessages += MessageList::insert(
                        toAbsolutePath(error.url()), error.line(),
                        MessageType::ScriptError, error.description());
            }
        });

    connect(mQuickWidget, &QQuickWidget::sceneGraphError,
        [this](QQuickWindow::SceneGraphError error, const QString &message) {
            mMessages += MessageList::insert(mFileName, 0,
                MessageType::ScriptError, message);
        });

    connect(mQuickWidget->engine(), &QQmlEngine::warnings,
        [this](const QList<QQmlError> &warnings) {
            for (auto &warning : warnings)
                mMessages += MessageList::insert(toAbsolutePath(warning.url()),
                    warning.line(), MessageType::ScriptWarning,
                    warning.description());
        });

    mNetworkAccessManagerFactory =
        std::make_unique<NetworkAccessManagerFactory>(this);
    mQuickWidget->engine()->setNetworkAccessManagerFactory(
        mNetworkAccessManagerFactory.get());

    static UrlInterceptor sUrlInterceptor;
    mQuickWidget->engine()->addUrlInterceptor(&sUrlInterceptor);
    mQuickWidget->engine()->addImportPath(QFileInfo(mFileName).dir().path());

    Singletons::fileCache().updateFromEditors();

    mQuickWidget->setSource(QUrl::fromLocalFile(mFileName));
}

#endif // defined(QtQuick_FOUND)

QmlView::~QmlView() { }

QList<QMetaObject::Connection> QmlView::connectEditActions(
    const EditActions &actions)
{
    if (std::exchange(mResetOnFocus, false))
        reset();

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

    return {};
}

QString QmlView::fileName() const
{
    return mFileName;
}

void QmlView::setFileName(QString fileName) { }

bool QmlView::load()
{
    reset();
    return true;
}

bool QmlView::save()
{
    return false;
}

void QmlView::setModified() { }

void QmlView::addDependency(const QString &fileName)
{
    mDependencies.insert(fileName);
}

bool QmlView::dependsOn(const QString &fileName) const
{
    return mDependencies.contains(fileName);
}

void QmlView::resetOnFocus()
{
    mResetOnFocus = true;
}
