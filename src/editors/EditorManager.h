#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include "FileDialog.h"
#include "DockWindow.h"
#include "SourceEditor.h"
#include "BinaryEditor.h"
#include "TextureEditor.h"
#include <QList>
#include <QMap>

class EditorManager final : public DockWindow
{
    Q_OBJECT
public:
    explicit EditorManager(QWidget *parent = nullptr);
    ~EditorManager() override;
    QWidget *createEditorPropertiesPanel(QAction *toggleAction);
    void createEditorToolBars(QToolBar *mainToolBar);
    void setEditorToolBarPalette(const QPalette &palette);

    SourceEditor *openNewSourceEditor(const QString &fileName,
        SourceType sourceType = SourceType::PlainText);
    BinaryEditor *openNewBinaryEditor(const QString &fileName);
    TextureEditor *openNewTextureEditor(const QString &fileName);
    IEditor *openEditor(const QString &fileName, bool asBinaryFile = false);
    SourceEditor *openSourceEditor(const QString &fileName,
        int line = -1, int column = -1);
    BinaryEditor *openBinaryEditor(const QString &fileName);
    TextureEditor *openTextureEditor(const QString &fileName);
    QDockWidget *createDock(QWidget *widget, IEditor *editor);
    void setAutoRaise(bool raise) { mAutoRaise = raise; }

    IEditor *getEditor(const QString &fileName);
    SourceEditor *getSourceEditor(const QString &fileName);
    BinaryEditor *getBinaryEditor(const QString &fileName);
    TextureEditor *getTextureEditor(const QString &fileName);
    QStringList getSourceFileNames() const;
    QStringList getBinaryFileNames() const;
    QStringList getImageFileNames() const;

    bool hasEditor() const { return !mDocks.empty(); }
    bool focusNextEditor();
    bool focusPreviousEditor();
    void updateCurrentEditor();
    bool hasCurrentEditor() const { return (mCurrentDock != nullptr); }
    QString currentEditorFileName();
    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    void renameEditors(const QString &prevFileName, const QString &fileName);
    bool saveEditor();
    bool saveEditorAs();
    bool saveAllEditors();
    bool reloadEditor();
    bool closeEditor();
    bool promptSaveAllEditors();
    bool closeAllEditors(bool promptSave = true);
    bool closeAllTextureEditors();
    QString getEditorObjectName(IEditor *editor) const;
    void setEditorObjectName(IEditor *editor, const QString &name);

Q_SIGNALS:
    void editorRenamed(const QString &prevFileName, const QString &fileName);
    void currentEditorChanged(QString fileName);

private:
    int getFocusedEditorIndex() const;
    bool focusEditorByIndex(int index);
    IEditor *currentEditor();
    QDockWidget *findEditorDock(const IEditor *editor) const;
    void addSourceEditor(SourceEditor *editor);
    void addBinaryEditor(BinaryEditor *editor);
    void addTextureEditor(TextureEditor *editor);
    void handleEditorFilenameChanged(QDockWidget *dock);
    bool saveDock(QDockWidget *dock);
    bool promptSaveDock(QDockWidget *dock);
    bool closeDock(QDockWidget *dock, bool promptSave = true) override;
    void autoRaise(QWidget *editor);
    void updateEditorToolBarVisibility();
    void updateEditorPropertiesVisibility();

    QList<SourceEditor*> mSourceEditors;
    QList<BinaryEditor*> mBinaryEditors;
    QList<TextureEditor*> mTextureEditors;
    std::map<QDockWidget*, IEditor*> mDocks;
    QDockWidget *mCurrentDock{ };
    FindReplaceBar *mFindReplaceBar{ };
    TextureInfoBar *mTextureInfoBar{ };
    TextureEditorToolBar *mTextureEditorToolBar{ };
    BinaryEditorToolBar *mBinaryEditorToolBar{ };
    SourceEditorToolBar *mSourceEditorToolBar{ };
    bool mAutoRaise{ true };
};

void updateDockCurrentProperty(QDockWidget *dock, bool current);

#endif // EDITORMANAGER_H
