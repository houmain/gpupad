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

    void resetRenderSession();
    void setEvaluationMode(bool automatic, bool steady);
    void setEvaluationInterval(int interval);

public slots:
    void manualEvaluation();
    void handleItemModified(const QModelIndex &index);
    void handleFileItemsChanged(const QString &fileName);
    void handleItemActivated(const QModelIndex &index, bool *handled);
    void handleFileRenamed(const QString &prevFileName,
        const QString &fileName);

private slots:
    void handleItemReordered(const QModelIndex &parent, int first);
    void handleSessionRendered();
    void update(bool manualEvaluation = false);

private:
    void updateBinaryEditor(const Buffer &buffer, BinaryEditor &editor);

    SessionModel& mModel;
    QTimer *mUpdateTimer{ };
    QSet<ItemId> mBuffersModified;
    QScopedPointer<RenderTask> mRenderSession;
    bool mRenderSessionInvalidated{ };
    bool mAutomaticEvaluation{ };
    bool mSteadyEvaluation{ };
};

#endif // SYNCHRONIZELOGIC_H
