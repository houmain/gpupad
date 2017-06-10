#ifndef SYNCHRONIZELOGIC_H
#define SYNCHRONIZELOGIC_H

#include "session/Item.h"
#include <QObject>
#include <QSet>

class QTimer;
class SessionModel;
class RenderTask;

class SynchronizeLogic : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = 0);
    ~SynchronizeLogic();

public slots:
    void handleMessageActivated(ItemId item, QString fileName,
        int line, int colunn);
    void handleItemActivated(const QModelIndex &index);
    void handleSourceEditorChanged(const QString &fileName);
    void handleBinaryEditorChanged(const QString &fileName);
    void handleImageEditorChanged(const QString &fileName);
    void handleFileRenamed(const QString &prevFileName,
        const QString &fileName);
    void update();
    void deactivateCalls();

private slots:
    void handleItemModified(const QModelIndex &index);
    void handleItemReordered(const QModelIndex &parent, int first);
    void handleFileItemsChanged(const QString &fileName);
    void handleTaskRendered();

private:
    void updateBinaryEditor(const Buffer &buffer);

    SessionModel& mModel;
    QTimer *mUpdateTimer;
    QSet<QString> mEditorsModified;
    QSet<ItemId> mBuffersModified;
    QScopedPointer<RenderTask> mActiveRenderTask;
    bool mRenderTaskInvalidated{ };
};

#endif // SYNCHRONIZELOGIC_H
