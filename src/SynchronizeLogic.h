#ifndef SYNCHRONIZELOGIC_H
#define SYNCHRONIZELOGIC_H

#include "session/Item.h"
#include "SourceType.h"
#include "InputState.h"
#include "Evaluation.h"
#include <QObject>
#include <QSet>

class QTimer;
class SessionModel;
class TextureEditor;
class BinaryEditor;
class RenderSession;
class ProcessSource;

class SynchronizeLogic final : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = nullptr);
    ~SynchronizeLogic() override;

    void resetRenderSession();

    void setEvaluationMode(EvaluationMode mode);
    void resetEvaluation();
    void manualEvaluation();
    void cancelAutomaticRevalidation();
    void updateEditor(ItemId itemId, bool activated);

    void setValidateSource(bool validate);
    bool validatingSource() const { return mValidateSource; }
    void setProcessSourceType(QString type);
    const QString& processSourceType() const { return mProcessSourceType; }
    void setCurrentEditorFileName(QString fileName);
    void setCurrentEditorSourceType(SourceType sourceType);

    void setMousePosition(QPoint position, QSize editorSize);
    void setMouseButtonPressed(Qt::MouseButton button, bool pressed);
    const InputState &getInputState();

Q_SIGNALS:
    void outputChanged(QString assembly);

private:
    void invalidateRenderSession();
    void handleInputStateChanged();
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
    void updateTextureEditor(const Texture &texture,
        TextureEditor &editor);
    void updateBinaryEditor(const Buffer &buffer,
        BinaryEditor &editor);
    void handleEvaluateTimout();
    void evaluate(EvaluationType evaluationType);
    void processSource();

    SessionModel &mModel;

    QTimer *mUpdateEditorsTimer{ };
    QSet<ItemId> mEditorItemsModified;

    QTimer *mEvaluationTimer{ };
    QScopedPointer<RenderSession> mRenderSession;
    bool mRenderSessionInvalidated{ };
    EvaluationMode mEvaluationMode{ };

    bool mValidateSource{ };
    QString mCurrentEditorFileName{ };
    SourceType mCurrentEditorSourceType{ };
    QString mProcessSourceType{ };
    QTimer *mProcessSourceTimer{ };
    ProcessSource* mProcessSource{ };

    InputState mInputState{ };
    bool mInputStateRead{ };
};

#endif // SYNCHRONIZELOGIC_H
