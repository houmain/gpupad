#pragma once

#include "MessageList.h"
#include <QModelIndex>

class CustomAction;
class QAction;
using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;

class CustomActions final : public QObject
{
    Q_OBJECT
public:
    explicit CustomActions(QObject *parent = nullptr);
    ~CustomActions();

    void setSelection(const QModelIndexList &selection);
    QList<QAction *> getApplicableActions();

private:
    void actionTriggered();
    void updateActions();

    std::map<QString, std::unique_ptr<CustomAction>> mActions;
    MessagePtrSet mMessages;
    QModelIndexList mSelection;
};
