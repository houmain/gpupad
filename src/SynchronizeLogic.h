#pragma once

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
    void finishEvaluation();
    void invalidateRenderSession();
    bool resetRenderSessionInvalidationState();
    void updateEditor(ItemId itemId, bool activated);

    void setValidateSource(bool validate);
    bool validatingSource() const { return mValidateSource; }
    void setProcessSourceType(QString type);
    const QString &processSourceType() const { return mProcessSourceType; }
    void setCurrentEditorFileName(QString fileName);
    void setCurrentEditorSourceType(SourceType sourceType);

    void handleSessionFileNameChanged(const QString &fileName);

    void evaluateBlockProperties(const Block &block, int *offset,
        int *rowCount);
    void evaluateTextureProperties(const Texture &texture, int *width,
        int *height, int *depth, int *layers);
    void evaluateTargetProperties(const Target &target, int *width, int *height,
        int *layers);

Q_SIGNALS:
    void waitingForSync();
    void evaluationModeChanged(EvaluationMode mode);
    void outputChanged(QVariant output);
    void currentEditorChanged(QString fileName);
    void itemAdded(const Item *item);
    void itemModified(const Item *item);
    void itemRemoved(const Item *item);

private:
    void triggerEvaluation(EvaluationType type, int delayMs = 0);
    bool initializeRenderSession();
    void handleItemRenamed(const QModelIndex &index, const QString &prevName);
    void handleItemModified(const QModelIndex &index);
    void handleItemsModified(const QModelIndex &topLeft,
        const QModelIndex &bottomRight, const QVector<int> &roles);
    void handleEditorFileRenamed(const QString &prevFileName,
        const QString &fileName);
    void handleMouseStateChanged();
    void handleKeyboardStateChanged();
    void handleViewportSizeChanged(const QString &fileName);
    void handleFileItemFileChanged(const FileItem &item);
    void handleFileItemRenamed(const FileItem &item, const QString &prevName);
    void handleFileChanged(const QString &fileName);
    void handleItemAdded(const QModelIndex &parent, int first);
    void handleItemRemoved(const QModelIndex &parent, int first);
    void updateEditors();
    void updateTextureEditor(const Texture &texture, TextureEditor &editor);
    void updateBinaryEditor(const Buffer &buffer, BinaryEditor &editor);
    void handleEvaluateTimout();
    void evaluate(EvaluationType evaluationType);
    void handlePreparingEvaluation(bool &itemsChanged, EvaluationType &type);
    void handleEvaluated();
    void processSource();

    SessionModel &mModel;

    QTimer *mUpdateEditorsTimer{};
    QSet<ItemId> mEditorItemsModified;

    QTimer *mEvaluationTimer{};
    EvaluationType mPendingEvaluationType{};
    EvaluationMode mEvaluationMode{};
    bool mRenderSessionInvalidated{};
    EvaluationType mEvaluationType{};
    bool mValidateSource{};
    QString mCurrentEditorFileName{};
    SourceType mCurrentEditorSourceType{};
    QString mProcessSourceType{};
    QTimer *mProcessSourceTimer{};
    std::unique_ptr<ProcessSource> mProcessSource;
    std::unique_ptr<RenderSessionBase> mRenderSession;
};
