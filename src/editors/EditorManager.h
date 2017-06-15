#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include "FileDialog.h"
#include "DockWindow.h"
#include <QList>
#include <QMap>

struct EditActions;
class IEditor;
class SourceEditor;
class ImageEditor;
class BinaryEditor;

class EditorManager : public DockWindow
{
    Q_OBJECT
public:
    explicit EditorManager(QWidget *parent = 0);
    ~EditorManager();

    int openNotSavedDialog(const QString& fileName);
    QString openNewSourceEditor();
    QString openNewBinaryEditor();
    QString openNewImageEditor();
    bool openEditor(const QString &fileName, bool focus = true);
    SourceEditor *openSourceEditor(const QString &fileName,
        bool raise = true, int line = -1, int column = -1);
    BinaryEditor *openBinaryEditor(const QString &fileName, bool raise = true);
    ImageEditor *openImageEditor(const QString &fileName, bool raise = true);

    SourceEditor *getSourceEditor(const QString &fileName);
    BinaryEditor *getBinaryEditor(const QString &fileName);
    ImageEditor *getImageEditor(const QString &fileName);
    QStringList getSourceFileNames() const;
    QStringList getBinaryFileNames() const;
    QStringList getImageFileNames() const;

    bool updateCurrentEditor();
    bool hasCurrentEditor() const { return (mCurrentDock != nullptr); }
    QString currentEditorFileName();
    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    bool saveEditor();
    bool saveEditorAs();
    bool saveAllEditors();
    bool reloadEditor();
    bool closeEditor();
    bool closeAllEditors();

signals:
    void sourceEditorChanged(const QString &fileName);
    void binaryEditorChanged(const QString &fileName);
    void imageEditorChanged(const QString &fileName);
    void editorRenamed(const QString &prevFileName, const QString &fileName);

private:
    IEditor *currentEditor();
    void addSourceEditor(SourceEditor *editor);
    void addBinaryEditor(BinaryEditor *editor);
    void addImageEditor(ImageEditor *editor);
    QDockWidget *createDock(QWidget *widget, IEditor *editor);
    bool saveDock(QDockWidget *dock);
    bool closeDock(QDockWidget *dock) override;
    void raiseEditor(QWidget *editor);

    QList<SourceEditor*> mSourceEditors;
    QList<BinaryEditor*> mBinaryEditors;
    QList<ImageEditor*> mImageEditors;
    QMap<QDockWidget*, IEditor*> mDocks;
    QDockWidget *mCurrentDock{ };
    const void *mPrevCurrentDock{ };
};

#endif // EDITORMANAGER_H
