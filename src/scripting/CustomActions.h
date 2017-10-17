#ifndef CustomACTIONS_H
#define CustomACTIONS_H

#include "MessageList.h"
#include <memory>
#include <vector>
#include <QDialog>
#include <QJsonValue>

namespace Ui {
class CustomActions;
}

class QFileSystemModel;
class CustomAction;

class CustomActions : public QDialog
{
    Q_OBJECT

public:
    explicit CustomActions(QWidget *parent = nullptr);
    ~CustomActions();

    void setSelection(QJsonValue selection);
    QList<QAction*> getApplicableActions();

private slots:
    void newAction();
    void importAction();
    void editAction();
    void deleteAction();
    void actionTriggered();

private:
    void updateActions();

    Ui::CustomActions *ui;
    QFileSystemModel *mModel;
    std::vector<std::unique_ptr<CustomAction>> mActions;
    MessagePtrSet mMessages;
    QJsonValue mSelection;
};

#endif // CustomACTIONS_H
