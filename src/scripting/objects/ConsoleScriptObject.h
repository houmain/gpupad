#pragma once

#include "MessageList.h"
#include <QObject>

class ConsoleScriptObject : public QObject
{
    Q_OBJECT
public:
    explicit ConsoleScriptObject(MessagePtrSet *messages, QObject *parent = 0);

    [[nodiscard]] std::shared_ptr<void> setFileName(const QString &fileName);
    [[nodiscard]] std::shared_ptr<void> setItemId(ItemId itemId);

    Q_INVOKABLE void output(QString message, int level);

private:
    std::shared_ptr<void> pushState();

    MessagePtrSet &mMessages;
    QString mFileName;
    ItemId mItemId{};
};
