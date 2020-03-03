#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include "FileDialog.h"
#include "DockWindow.h"
#include "SourceEditor.h"
#include "BinaryEditor.h"
#include "TextureEditor.h"
#include <QList>
#include <QMap>

class EditorManager : public DockWindow
{
    Q_OBJECT
public:
    explicit EditorManager(QWidget *parent = nullptr);
    ~EditorManager() override;

    FindReplaceBar &findReplaceBar() { return *mFindReplaceBar; }
    int openNotSavedDialog(const QString &fileName);
    QString openNewSourceEditor(const QString &baseName,
        SourceType sourceType = SourceType::PlainText);
    QString openNewBinaryEditor(const QString &baseName);
    QString openNewTextureEditor(const QString &baseName);
    bool openEditor(const QString &fileName);
    SourceEditor *openSourceEditor(const QString &fileName,
        int line = -1, int column = -1);
    BinaryEditor *openBinaryEditor(const QString &fileName);
    TextureEditor *openTextureEditor(const QString &fileName);
    void setAutoRaise(bool raise) { mAutoRaise = raise; }

    SourceEditor *getSourceEditor(const QString &fileName);
    BinaryEditor *getBinaryEditor(const QString &fileName);
    TextureEditor *getTextureEditor(const QString &fileName);
    QStringList getSourceFileNames() const;
    QStringList getBinaryFileNames() const;
    QStringList getImageFileNames() const;

    bool hasEditor() const { return !mDocks.isEmpty(); }
    void updateCurrentEditor();
    bool hasCurrentEditor() const { return (mCurrentDock != nullptr); }
    QString currentEditorFileName();
    SourceType currentSourceType();
    void setCurrentSourceType(SourceType sourceType);
    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    bool saveEditor();
    bool saveEditorAs();
    bool saveAllEditors();
    bool reloadEditor();
    bool closeEditor();
    bool closeAllEditors();
    bool closeAllTextureEditors();

signals:
    void editorChanged(const QString &fileName);
    void editorRenamed(const QString &prevFileName, const QString &fileName);
    void sourceTypeChanged(SourceType sourceType);

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    IEditor *currentEditor();
    void addSourceEditor(SourceEditor *editor);
    void addBinaryEditor(BinaryEditor *editor);
    void addTextureEditor(TextureEditor *editor);
    QDockWidget *createDock(QWidget *widget, IEditor *editor);
    bool saveDock(QDockWidget *dock);
    bool closeDock(QDockWidget *dock) override;
    void autoRaise(QWidget *editor);

    QList<SourceEditor*> mSourceEditors;
    QList<BinaryEditor*> mBinaryEditors;
    QList<TextureEditor*> mTextureEditors;
    QMap<QDockWidget*, IEditor*> mDocks;
    QDockWidget *mCurrentDock{ };
    FindReplaceBar *mFindReplaceBar{ };
    bool mAutoRaise{ true };
};

#endif // EDITORMANAGER_H
