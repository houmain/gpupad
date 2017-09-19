#ifndef CustomACTIONS_H
#define CustomACTIONS_H

#include "MessageList.h"
#include <memory>
#include <vector>
#include <QDialog>

namespace Ui {
class CustomActions;
}

class QFileSystemModel;
class CustomAction;

class CustomActions : public QDialog
{
    Q_OBJECT

public:
    explicit CustomActions(QWidget *parent = 0);
    ~CustomActions();

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
};

#endif // CustomACTIONS_H
