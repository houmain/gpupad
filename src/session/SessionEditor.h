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
    explicit SessionEditor(QWidget *parent = 0);

    void addItemActions(QMenu* menu);
    void updateItemActions();
    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions, bool focused);
    QString fileName() const { return mFileName; }
    void setFileName(const QString &fileName);
    bool isModified() const;
    bool clear();
    bool load();
    bool save();

signals:
    void itemActivated(const QModelIndex &index, bool *handled);
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
    void addItem(ItemType type);
    void treeItemActivated(const QModelIndex &index);
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
    QAction *mAddPrimitivesAction{ };
    QAction *mAddAttributeAction{ };
    QAction *mAddFramebufferAction{ };
    QAction *mAddAttachmentAction{ };
    QAction *mAddCallAction{ };
    QAction *mAddStateAction{ };
    QAction *mAddScriptAction{ };
};

#endif // SESSIONEDITOR_H
