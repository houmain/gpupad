#include "GpupadScriptObject.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "editors/EditorManager.h"

#if defined(QtQuick_FOUND)
#  include <QQuickWidget>
#  include <QQmlEngine>
#  include <QQmlContext>
#endif

#if defined(QtWebEngineWidgets_FOUND)
#  include <QWebEngineView>
#  include <QWebChannel>
#endif

GpupadScriptObject::GpupadScriptObject(QObject *parent)
    : QObject(parent)
{
}

QString GpupadScriptObject::openFileDialog()
{
    auto options = FileDialog::Options();
    if (Singletons::fileDialog().exec(options))
        return Singletons::fileDialog().fileName();
    return { };
}

QString GpupadScriptObject::readTextFile(const QString &fileName)
{
    auto file = QFile(fileName);
    file.open(QFile::ReadOnly | QFile::Text);
    return QTextStream(&file).readAll();
}

bool GpupadScriptObject::openQmlDock()
{
#if defined(QtQuick_FOUND)
    class CustomEditor : public IEditor
    {
    public:
        QList<QMetaObject::Connection> connectEditActions(const EditActions &) override { return { }; }
        QString fileName() const override { return FileDialog::generateNextUntitledFileName("QML"); }
        void setFileName(QString) override { }
        bool load() override { return false; }
        bool save() override { return false; }
        int tabifyGroup() override { return 1; }
    };

    auto fileName = QDir::current().filePath("index.qml");
    if (!QFileInfo::exists(fileName))
        return false;

    auto editor = new CustomEditor();
    auto widget = new QQuickWidget();

    connect(widget, &QQuickWidget::statusChanged,
        [widget](QQuickWidget::Status status) {
            if (status == QQuickWidget::Error) {
                QStringList errors;
                const auto widgetErrors = widget->errors();
                for (const QQmlError &error : widgetErrors)
                    errors.append(error.toString());
                int here = 0;
            }
        });
    connect(widget, &QQuickWidget::sceneGraphError,
        [](QQuickWindow::SceneGraphError error, const QString &message) {
            int here = 0;
        });

    widget->setResizeMode(QQuickWidget::SizeRootObjectToView );
    Singletons::editorManager().createDock(widget, editor);

    widget->setSource(QUrl::fromLocalFile(fileName));

    widget->rootContext()->setContextProperty("gpupad", this);
    //widget->engine()->globalObject().setProperty("gpupad",
    //    widget->engine()->newQObject(this));

    return true;
#else
    return false;
#endif
}

bool GpupadScriptObject::openWebDock()
{
#if defined(QtWebEngineWidgets_FOUND)
    class CustomEditor : public IEditor
    {
    public:
        QList<QMetaObject::Connection> connectEditActions(const EditActions &) override { return { }; }
        QString fileName() const override { return FileDialog::generateNextUntitledFileName("Web"); }
        void setFileName(QString) override { }
        bool load() override { return false; }
        bool save() override { return false; }
        int tabifyGroup() override { return 1; }
    };

    class CustomPage : public QWebEnginePage {
    private:
        GpupadScriptObject &mParent;

    public:
        CustomPage(GpupadScriptObject *parent)
            : QWebEnginePage(parent)
            , mParent(*parent)
        {
        }

        void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
            const QString &message, int lineNumber, const QString &sourceID) override
        {
            const auto messageType = (
                level == ErrorMessageLevel ? MessageType::ScriptError :
                level == WarningMessageLevel ? MessageType::ScriptWarning :
                MessageType::ScriptMessage);
            mParent.mMessages += MessageList::insert(url().fileName(), 0, messageType, message);
        }
    };

    auto fileName = QDir::current().filePath("index.html");
    if (!QFileInfo::exists(fileName))
        return false;

    auto editor = new CustomEditor();
    auto view = new QWebEngineView();
    Singletons::editorManager().createDock(view, editor);

    auto page = new CustomPage(this);
    page->load(QUrl::fromLocalFile(fileName));

    connect(view, &QWebEngineView::loadFinished, [this, view, page]() {
        auto webChannel = new QWebChannel();
        webChannel->registerObject("gpupad", this);
        page->setWebChannel(webChannel);

        page->runJavaScript(
            readTextFile(":/qtwebchannel/qwebchannel.js") +
            "window.webChannel = new QWebChannel(qt.webChannelTransport, function(channel) {"
            "  window.gpupad = channel.objects.gpupad;"
            "})");
    });
    view->setPage(page);

    return true;
#else
    return false;
#endif
}
