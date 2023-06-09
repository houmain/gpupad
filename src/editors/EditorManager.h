#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include "EditActions.h"
#include "SourceType.h"
#include "FileDialog.h"
#include "DockWindow.h"
#include <QList>
#include <QMap>
#include <QStack>

class IEditor;
class SourceEditor;
class BinaryEditor;
class TextureEditor;
class FindReplaceBar;
class TextureInfoBar;
class TextureEditorToolBar;
class BinaryEditorToolBar;
class SourceEditorToolBar;
class QmlView;

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
    QmlView *openQmlView(const QString &fileName);
    void setAutoRaise(bool raise) { mAutoRaise = raise; }

    IEditor *getEditor(const QString &fileName);
    SourceEditor *getSourceEditor(const QString &fileName);
    BinaryEditor *getBinaryEditor(const QString &fileName);
    TextureEditor *getTextureEditor(const QString &fileName);
    QmlView *getQmlView(const QString &fileName);
    QStringList getSourceFileNames() const;
    QStringList getBinaryFileNames() const;
    QStringList getImageFileNames() const;

    bool hasEditor() const { return !mDocks.empty(); }
    bool canNavigateBackward() const;
    bool canNavigateForward() const;
    void navigateBackward();
    void navigateForward();
    bool focusNextEditor(bool wrap);
    bool focusPreviousEditor(bool wrap);
    void updateCurrentEditor();
    bool hasCurrentEditor() const { return (mCurrentDock != nullptr); }
    QString currentEditorFileName();
    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    void renameEditors(const QString &prevFileName, const QString &fileName);
    void resetQmlViewsDependingOn(const QString &fileName);
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
    void pasteInNewEditor();
    bool canPasteInNewEditor() const;

Q_SIGNALS:
    void editorRenamed(const QString &prevFileName, const QString &fileName);
    void currentEditorChanged(QString fileName);
    void canNavigateBackwardChanged(bool canNavigate);
    void canNavigateForwardChanged(bool canNavigate);
    void canPasteInNewEditorChanged(bool canPaste);

private:
    int getFocusedEditorIndex() const;
    bool focusEditorByIndex(int index, bool wrap);
    IEditor *currentEditor();
    QDockWidget *findEditorDock(const IEditor *editor) const;
    void closeUntitledUntouchedEditor();
    void addSourceEditor(SourceEditor *editor);
    void addBinaryEditor(BinaryEditor *editor);
    void addTextureEditor(TextureEditor *editor);
    void setDockWindowTitle(QDockWidget *dock, const QString &fileName);
    void handleEditorFilenameChanged(QDockWidget *dock);
    bool saveDock(QDockWidget *dock);
    bool promptSaveDock(QDockWidget *dock);
    void closeDock(QDockWidget *dock);
    void autoRaise(QWidget *editor);
    void updateEditorToolBarVisibility();
    void updateEditorPropertiesVisibility();
    QDockWidget *findDockToAddTab(int tabifyGroup);
    QDockWidget *createDock(QWidget *widget, IEditor *editor);
    void clearNavigationStack();
    void addNavigationPosition(const QString &position, bool update);
    bool restoreNavigationPosition(int index);

    QList<SourceEditor*> mSourceEditors;
    QList<BinaryEditor*> mBinaryEditors;
    QList<TextureEditor*> mTextureEditors;
    QList<QmlView*> mQmlViews;
    std::map<QDockWidget*, IEditor*> mDocks;
    std::map<int, QDockWidget*> mLastFocusedTabifyGroupDock;
    QDockWidget *mCurrentDock{ };
    FindReplaceBar *mFindReplaceBar{ };
    TextureInfoBar *mTextureInfoBar{ };
    TextureEditorToolBar *mTextureEditorToolBar{ };
    BinaryEditorToolBar *mBinaryEditorToolBar{ };
    SourceEditorToolBar *mSourceEditorToolBar{ };
    bool mAutoRaise{ true };
    QStack<QPair<QObject*, QString>> mNavigationStack;
    int mNavigationStackPosition{ };
};

void updateDockCurrentProperty(QDockWidget *dock, bool current);

#endif // EDITORMANAGER_H
