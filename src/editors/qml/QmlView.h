#pragma once

#include "MessageList.h"
#include "editors/IEditor.h"
#include <QFrame>
#include <QSet>

class QQuickWidget;
class QQmlNetworkAccessManagerFactory;

class QmlView : public QFrame, public IEditor
{
    Q_OBJECT

public:
    explicit QmlView(QString fileName, QWidget *parent = nullptr);
    ~QmlView();

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override;
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    void setModified() override;
    int tabifyGroup() const override { return 3; }

    void addDependency(const QString &fileName);
    bool dependsOn(const QString &fileName) const;
    void resetOnFocus();

private:
    void reset();

    const QString mFileName;
    std::unique_ptr<QQmlNetworkAccessManagerFactory>
        mNetworkAccessManagerFactory;
    MessagePtrSet mMessages;
    QQuickWidget *mQuickWidget{};
    QSet<QString> mDependencies;
    bool mResetOnFocus{};
};
