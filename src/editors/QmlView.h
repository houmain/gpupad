#pragma once

#include "IEditor.h"
#include "MessageList.h"
#include <QFrame>

class QQuickWidget;

class QmlView : public QFrame, public IEditor
{
    Q_OBJECT

public:
    explicit QmlView(QString fileName, QWidget *parent = nullptr);

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override;
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    int tabifyGroup() override;

private:
    const QString mFileName;
    MessagePtrSet mMessages;
    QQuickWidget *mQuickWidget{ };
};
