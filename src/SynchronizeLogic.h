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
class ValidateSource;

class SynchronizeLogic : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = nullptr);
    ~SynchronizeLogic() override;

    void resetRenderSession();
    void setSourceValidationActive(bool active);
    void setEvaluationMode(bool automatic, bool steady);
    void setEvaluationInterval(int interval);
    void updateBinaryEditor(const Buffer &buffer,
        BinaryEditor &editor, bool scrollToOffset = false);
    void updateFileCache();

public slots:
    void manualEvaluation();
    void handleItemModified(const QModelIndex &index);
    void handleFileItemsChanged(const QString &fileName);
    void handleFileRenamed(const QString &prevFileName,
        const QString &fileName);
    void handleSourceTypeChanged(SourceType sourceType);

private slots:
    void handleItemReordered(const QModelIndex &parent, int first);
    void handleSessionRendered();
    void evaluate(bool manualEvaluation = false);
    void validateSource();

private:
    SessionModel &mModel;
    QSet<ItemId> mBuffersModified;
    QSet<QString> mFilesModified;

    QTimer *mEvaluationTimer{ };
    QScopedPointer<RenderSession> mRenderSession;
    bool mRenderSessionInvalidated{ };
    bool mAutomaticEvaluation{ };
    bool mSteadyEvaluation{ };

    QTimer *mValidateSourceTimer{ };
    QScopedPointer<ValidateSource> mValidateSource;
};

#endif // SYNCHRONIZELOGIC_H
