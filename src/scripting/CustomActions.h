#ifndef CustomACTIONS_H
#define CustomACTIONS_H

#include <QDialog>

namespace Ui {
class CustomActions;
}

class QFileSystemModel;

class CustomActions : public QDialog
{
    Q_OBJECT

public:
    explicit CustomActions(QWidget *parent = 0);
    ~CustomActions();

    QList<QAction*> getApplicableActions(QString selection);
    QList<QAction*> getApplicableActions(QList<QModelIndex> selection);

private slots:
    void newAction();
    void importAction();
    void editAction();
    void deleteAction();

private:
    Ui::CustomActions *ui;
    QFileSystemModel *mModel;
};

#endif // CustomACTIONS_H
