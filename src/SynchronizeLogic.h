#ifndef SYNCHRONIZELOGIC_H
#define SYNCHRONIZELOGIC_H

#include "session/Item.h"
#include "SourceType.h"
#include <QObject>
#include <QSet>

class QTimer;
class SessionModel;
class BinaryEditor;
class RenderSession;
class ProcessSource;

class SynchronizeLogic : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = nullptr);
    ~SynchronizeLogic() override;

    void resetRenderSession();
    void setValidateSource(bool validate);
    void setProcessSourceType(QString type);
    void setEvaluationMode(bool automatic, bool steady);
    void updateBinaryEditor(const Buffer &buffer,
        BinaryEditor &editor, bool scrollToOffset = false);
    void updateFileCache();
    void setMousePosition(QPointF pos) { mMousePosition = pos; }
    const QPointF &mousePosition() const { return mMousePosition; }

signals:
    void outputChanged(QString assembly);

public slots:
    void manualEvaluation();
    void handleItemsModified(const QModelIndex &topLeft,
        const QModelIndex &bottomRight, const QVector<int> &roles);
    void handleItemModified(const QModelIndex &index);
    void handleFileItemsChanged(const QString &fileName);
    void handleFileRenamed(const QString &prevFileName,
        const QString &fileName);
    void handleSourceTypeChanged(SourceType sourceType);

private slots:
    void handleItemReordered(const QModelIndex &parent, int first);
    void handleSessionRendered();
    void evaluate(bool manualEvaluation);
    void processSource();

private:
    SessionModel &mModel;
    QSet<ItemId> mBuffersModified;
    QSet<QString> mFilesModified;

    QTimer *mEvaluationTimer{ };
    QScopedPointer<RenderSession> mRenderSession;
    bool mRenderSessionInvalidated{ };
    bool mAutomaticEvaluation{ };
    bool mSteadyEvaluation{ };

    bool mValidateSource{ };
    QString mProcessSourceType{ };
    QTimer *mProcessSourceTimer{ };
    ProcessSource* mProcessSource{ };

    QPointF mMousePosition{ };
};

#endif // SYNCHRONIZELOGIC_H
