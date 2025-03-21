#pragma once

#include "MessageList.h"
#include "editors/IEditor.h"
#include <QFrame>
#include <QSet>

class Theme;
class QQuickWidget;
class QQmlNetworkAccessManagerFactory;
using QScriptEnginePtr = std::shared_ptr<class ScriptEngine>;

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

    const QScriptEnginePtr &enginePtr() const { return mEnginePtr; }
    void addDependency(const QString &fileName);
    bool dependsOn(const QString &fileName) const;
    void resetOnFocus();

private:
    void windowThemeChanged(const Theme &theme);
    void reset();

    const QString mFileName;
    QScriptEnginePtr mEnginePtr;
    std::unique_ptr<QQmlNetworkAccessManagerFactory>
        mNetworkAccessManagerFactory;
    MessagePtrSet mMessages;
    QQuickWidget *mQuickWidget{};
    QSet<QString> mDependencies;
    bool mResetOnFocus{};
};
