#ifndef SYNCHRONIZELOGIC_H
#define SYNCHRONIZELOGIC_H

#include "session/Item.h"
#include <QObject>
#include <QSet>

class QTimer;
class SessionModel;
class BinaryEditor;
class RenderTask;

class SynchronizeLogic : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = 0);
    ~SynchronizeLogic();

public slots:
    void manualUpdate();
    void handleItemModified(const QModelIndex &index);
    void handleItemActivated(const QModelIndex &index, bool *handled);
    void handleSourceEditorChanged(const QString &fileName);
    void handleBinaryEditorChanged(const QString &fileName);
    void handleImageEditorChanged(const QString &fileName);
    void handleFileRenamed(const QString &prevFileName,
        const QString &fileName);

private slots:
    void handleItemReordered(const QModelIndex &parent, int first);
    void handleFileItemsChanged(const QString &fileName);
    void handleSessionRendered();
    void handleRefresh();

private:
    void updateBinaryEditor(const Buffer &buffer, BinaryEditor &editor);

    SessionModel& mModel;
    QTimer *mUpdateTimer{ };
    QSet<QString> mEditorsModified;
    QSet<ItemId> mBuffersModified;
    QScopedPointer<RenderTask> mRenderSession;
    bool mRenderTaskInvalidated{ };
};

#endif // SYNCHRONIZELOGIC_H
