#pragma once

#include "Evaluation.h"
#include "MessageList.h"
#include "SourceType.h"
#include "session/Item.h"
#include <QObject>
#include <QSet>

class QTimer;
class SessionModel;
class TextureEditor;
class BinaryEditor;
class RenderSessionBase;
class ProcessSource;

class SynchronizeLogic final : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = nullptr);
    ~SynchronizeLogic() override;

    void resetRenderSession();

    void setEvaluationMode(EvaluationMode mode);
    EvaluationMode evaluationMode() const { return mEvaluationMode; }
    void resetEvaluation();
    void manualEvaluation();
    void cancelAutomaticRevalidation();
    void updateEditor(ItemId itemId, bool activated);

    void setValidateSource(bool validate);
    bool validatingSource() const { return mValidateSource; }
    void setProcessSourceType(QString type);
    const QString &processSourceType() const { return mProcessSourceType; }
    void setCurrentEditorFileName(QString fileName);
    void setCurrentEditorSourceType(SourceType sourceType);

    void handleSessionFileNameChanged(const QString& fileName);
    void handleMouseStateChanged();
    void handleKeyboardStateChanged();

    void evaluateBlockProperties(const Block &block, int *offset,
        int *rowCount);
    void evaluateTextureProperties(const Texture &texture, int *width,
        int *height, int *depth, int *layers);
    void evaluateTargetProperties(const Target &target, int *width, int *height,
        int *layers);

Q_SIGNALS:
    void outputChanged(QString assembly);

private:
    void invalidateRenderSession();
    void updateRenderSession();
    void handleItemModified(const QModelIndex &index);
    void handleItemsModified(const QModelIndex &topLeft,
        const QModelIndex &bottomRight, const QVector<int> &roles);
    void handleEditorFileRenamed(const QString &prevFileName,
        const QString &fileName);
    void handleFileItemFileChanged(const FileItem &item);
    void handleFileItemRenamed(const FileItem &item);
    void handleFileChanged(const QString &fileName);
    void handleItemReordered(const QModelIndex &parent, int first);
    void handleSessionRendered();
    void updateEditors();
    void updateTextureEditor(const Texture &texture, TextureEditor &editor);
    void updateBinaryEditor(const Buffer &buffer, BinaryEditor &editor);
    void handleEvaluateTimout();
    void evaluate(EvaluationType evaluationType);
    void processSource();

    SessionModel &mModel;

    QTimer *mUpdateEditorsTimer{};
    QSet<ItemId> mEditorItemsModified;

    QString mSessionFileName;
    QTimer *mEvaluationTimer{};
    std::unique_ptr<RenderSessionBase> mRenderSession;
    bool mRenderSessionInvalidated{};
    EvaluationMode mEvaluationMode{};

    bool mValidateSource{};
    QString mCurrentEditorFileName{};
    SourceType mCurrentEditorSourceType{};
    QString mProcessSourceType{};
    QTimer *mProcessSourceTimer{};
    std::unique_ptr<ProcessSource> mProcessSource;
};
