#pragma once

#include "MessageList.h"
#include "editors/IEditor.h"
#include <QFrame>

class Theme;
class QQmlNetworkAccessManagerFactory;
using QScriptEnginePtr = std::shared_ptr<class ScriptEngine>;

#if defined(USE_WINDOW_CONTAINER)
class QQuickView;
#else
class QQuickWidget;
#endif

class QmlView : public QFrame, public IEditor
{
    Q_OBJECT

public:
    explicit QmlView(QString fileName, QScriptEnginePtr enginePtr = nullptr,
        QWidget *parent = nullptr);
    ~QmlView();

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override;
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    void setModified() override;
    int tabifyGroup() const override { return 3; }
    QSize minimumSizeHint() const override { return QSize(150, 150); }

    const QScriptEnginePtr &enginePtr() const { return mEnginePtr; }
    void addDependency(const QString &fileName);
    bool dependsOn(const QString &fileName) const;
    void resetOnFocus();

private:
    void windowThemeChanged(const Theme &theme);
    QColor backgroundColor() const;
    void reset();

    const QString mFileName;
    QScriptEnginePtr mEnginePtr;
    std::unique_ptr<QQmlNetworkAccessManagerFactory>
        mNetworkAccessManagerFactory;
    MessagePtrSet mMessages;
#if defined(USE_WINDOW_CONTAINER)
    QQuickView *mQuickView{};
    QWidget *mQuickWidget{};
#else
    QQuickWidget *mQuickWidget{};
#endif
    QSet<QString> mDependencies;
    bool mResetOnFocus{};
};
