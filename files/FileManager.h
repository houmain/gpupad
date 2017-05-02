#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "FileTypes.h"
#include "FileDialog.h"
#include "DockWindow.h"
#include <QList>
#include <QMap>

class FileManager : public DockWindow
{
    Q_OBJECT
public:
    static SourceFilePtr openSourceFile(const QString &fileName);
    static BinaryFilePtr openBinaryFile(const QString &fileName);
    static ImageFilePtr openImageFile(const QString &fileName, int index);

    explicit FileManager(QWidget *parent = 0);
    ~FileManager();

    int openNotSavedDialog(const QString& fileName);
    QString openNewSourceEditor();
    QString openNewBinaryEditor();
    QString openNewImageEditor();

    bool openEditor(const QString &fileName, bool focus = true);
    SourceEditor *openSourceEditor(const QString &fileName,
        bool raise = true, int line = -1, int column = -1);
    BinaryEditor *openBinaryEditor(const QString &fileName, bool raise = true);
    ImageEditor *openImageEditor(const QString &fileName, bool raise = true);

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

    SourceEditor *findSourceEditor(const QString &fileName);
    BinaryEditor *findBinaryEditor(const QString &fileName);
    ImageEditor *findImageEditor(const QString &fileName);
    SourceFilePtr findSourceFile(const QString &fileName);
    BinaryFilePtr findBinaryFile(const QString &fileName);
    ImageFilePtr findImageFile(const QString &firstFileName, int index = 0);

    QStringList getSourceFileNames() const;
    QStringList getBinaryFileNames() const;
    QStringList getImageFileNames() const;

signals:
    void sourceEditorChanged(const QString &fileName);
    void binaryEditorChanged(const QString &fileName);
    void imageEditorChanged(const QString &fileName);
    void editorRenamed(const QString &prevFileName, const QString &fileName);

private:
    struct EditorFunctions;

    EditorFunctions *currentEditorFunctions();
    SourceEditor *createSourceEditor(QSharedPointer<SourceFile> file);
    BinaryEditor *createBinaryEditor(QSharedPointer<BinaryFile> file);
    ImageEditor *createImageEditor(QSharedPointer<ImageFile> file);
    void addDock(QDockWidget *dock, QWidget *widget, EditorFunctions functions);
    bool saveDock(QDockWidget *dock);
    bool closeDock(QDockWidget *dock) override;

    QList<SourceEditor*> mSourceEditors;
    QList<BinaryEditor*> mBinaryEditors;
    QList<ImageEditor*> mImageEditors;
    QMap<QDockWidget*, EditorFunctions> mDocks;
    QDockWidget *mCurrentDock{ };
    const void *mPrevCurrentDock{ };
};

#endif // FILEMANAGER_H
