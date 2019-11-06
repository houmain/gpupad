#ifndef SESSIONEDITOR_H
#define SESSIONEDITOR_H

#include "Item.h"
#include <QTreeView>

class QMenu;
struct EditActions;
class SessionModel;

class SessionEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit SessionEditor(QWidget *parent = nullptr);

    void addItemActions(QMenu* menu);
    void updateItemActions();
    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions);
    QString fileName() const { return mFileName; }
    void setFileName(QString fileName);
    void setCurrentItem(ItemId itemId);
    bool isModified() const;
    void clearUndo();
    bool clear();
    bool load();
    bool save();
    void activateFirstItem();

signals:
    void itemAdded(const QModelIndex &index);
    void itemActivated(const QModelIndex &index);
    void fileNameChanged(const QString &fileName);
    void modificationChanged(bool modified);
    void focusChanged(bool focused);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void selectionChanged(const QItemSelection &selected,
                          const QItemSelection &deselected) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void cut();
    void copy();
    void paste();
    void delete_();
    void openContextMenu(const QPoint &pos);
    void addItem(Item::Type type);
    void renameCurrentItem();

private:
    bool canPaste() const;

    SessionModel &mModel;
    QMenu *mContextMenu{ };
    QString mFileName;
    QAction *mRenameAction{ };
    QAction *mAddGroupAction{ };
    QAction *mAddBufferAction{ };
    QAction *mAddColumnAction{ };
    QAction *mAddTextureAction{ };
    QAction *mAddImageAction{ };
    QAction *mAddSamplerAction{ };
    QAction *mAddProgramAction{ };
    QAction *mAddShaderAction{ };
    QAction *mAddBindingAction{ };
    QAction *mAddStreamAction{ };
    QAction *mAddAttributeAction{ };
    QAction *mAddTargetAction{ };
    QAction *mAddAttachmentAction{ };
    QAction *mAddCallAction{ };
    QAction *mAddScriptAction{ };
};

#endif // SESSIONEDITOR_H
