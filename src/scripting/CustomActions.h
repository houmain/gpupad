#pragma once

#include "MessageList.h"
#include <QDialog>
#include <QJsonValue>
#include <QModelIndex>
#include <memory>
#include <vector>

namespace Ui {
    class CustomActions;
}

class QFileSystemModel;
class CustomAction;

class CustomActions final : public QDialog
{
    Q_OBJECT
public:
    explicit CustomActions(QWidget *parent = nullptr);
    ~CustomActions();

    void setSelection(const QModelIndexList &selection);
    QList<QAction *> getApplicableActions();

private:
    void newAction();
    void importAction();
    void editAction();
    void deleteAction();
    void actionTriggered();
    void updateWidgets();
    void updateActions();

    Ui::CustomActions *ui;
    QFileSystemModel *mModel;
    std::vector<std::unique_ptr<CustomAction>> mActions;
    MessagePtrSet mMessages;
    QModelIndexList mSelection;
};
