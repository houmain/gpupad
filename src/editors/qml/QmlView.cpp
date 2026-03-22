
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
#  include "Settings.h"
#  include "scripting/ScriptEngine.h"
#  include <QBoxLayout>
#  include <QDir>
#  include <QNetworkAccessManager>
#  include <QNetworkReply>
#  include <QQmlAbstractUrlInterceptor>
#  include <QQmlNetworkAccessManagerFactory>
#  include <QQmlEngine>
#  include <QQuickStyle>
#  include <QApplication>
#  include <QQuickView>
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

QmlView::QmlView(QString fileName, QScriptEnginePtr enginePtr, QWidget *parent)
    : QFrame(parent)
    , mFileName(fileName)
    , mEnginePtr(std::move(enginePtr))
{
    Q_ASSERT(onMainThread());
    QQuickStyle::setStyle("Fusion");

    if (!mEnginePtr)
        mEnginePtr = ScriptEngine::make(fileName, thread(), this);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto qmlEngine = qobject_cast<QQmlEngine *>(&mEnginePtr->jsEngine());
    Q_ASSERT(qmlEngine);
    mNetworkAccessManagerFactory =
        std::make_unique<NetworkAccessManagerFactory>(this);
    qmlEngine->setNetworkAccessManagerFactory(
        mNetworkAccessManagerFactory.get());

    static UrlInterceptor sUrlInterceptor;
    qmlEngine->addUrlInterceptor(&sUrlInterceptor);
    qmlEngine->addImportPath(QFileInfo(mFileName).dir().path());

    connect(&Singletons::settings(), &Settings::windowThemeChanged, this,
        &QmlView::windowThemeChanged);
}

void QmlView::windowThemeChanged(const Theme &theme)
{
    mQuickView->setColor(backgroundColor());
}

QColor QmlView::backgroundColor() const
{
    return qApp->palette().alternateBase().color();
}

void QmlView::reset()
{
    if (mQuickWidget) {
        layout()->removeWidget(mQuickWidget);
        connect(mQuickWidget, &QWidget::destroyed, this, &QmlView::reset,
            Qt::QueuedConnection);
        mQuickWidget->deleteLater();
        mQuickWidget = nullptr;
        return;
    }

    mMessages.clear();

    const auto qmlEngine = qobject_cast<QQmlEngine *>(&mEnginePtr->jsEngine());
    Q_ASSERT(qmlEngine);
    qmlEngine->clearComponentCache();

    mQuickView = new QQuickView(qmlEngine, nullptr);
    mQuickView->setResizeMode(QQuickView::SizeRootObjectToView);
    mQuickView->setColor(backgroundColor());
    mQuickView->setFormat(QSurfaceFormat::defaultFormat());

    connect(mQuickView, &QQuickView::statusChanged,
        [this](QQuickView::Status status) {
            if (status == QQuickView::Error) {
                const auto errors = mQuickView->errors();
                for (const QQmlError &error : errors) {
                    const auto fileName = toAbsolutePath(error.url());
                    if (!QFileInfo(fileName).isFile() && error.line() < 0) {
                        mMessages += MessageList::insert(0, 0,
                            MessageType::LoadingFileFailed, fileName);
                    } else {
                        mMessages += MessageList::insert(fileName, error.line(),
                            MessageType::ScriptError, error.description());
                    }
                }
            }
        });

    connect(mQuickView, &QQuickView::sceneGraphError,
        [this](QQuickWindow::SceneGraphError error, const QString &message) {
            mMessages += MessageList::insert(mFileName, 0,
                MessageType::ScriptError, message);
        });

    connect(qmlEngine, &QQmlEngine::warnings,
        [this](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) {
                auto fileName = toAbsolutePath(warning.url());
                auto line = warning.line();
                if (fileName.isEmpty()) {
                    fileName = mFileName;
                    line = 0;
                }
                mMessages += MessageList::insert(fileName, line,
                    MessageType::ScriptWarning, warning.description());
            }
        });

    Singletons::fileCache().updateFromEditors();

    mQuickView->setSource(QUrl::fromLocalFile(mFileName));
    mQuickWidget = QWidget::createWindowContainer(mQuickView);

    layout()->addWidget(mQuickWidget);

#  if defined(_WIN32)
    // WORKAROUND: reapply current palette, to fix Fusion theme
    auto palette = qApp->palette();
    qApp->setPalette(QPalette());
    qApp->setPalette(palette);
#  else
    // on Linux some more needs to be reapplied (very slow on Windows)
    qApp->setPalette(QPalette());
    Singletons::settings().setWindowTheme(Singletons::settings().windowTheme());
#  endif
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

QString QmlView::actionId() const
{
    return mEnginePtr->actionId();
}

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
