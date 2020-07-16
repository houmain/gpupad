#ifndef SYNCHRONIZELOGIC_H
#define SYNCHRONIZELOGIC_H

#include "session/Item.h"
#include "SourceType.h"
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
    void setValidateSource(bool validate);
    void setProcessSourceType(QString type);
    void setEvaluationMode(EvaluationMode mode);
    void resetEvaluation();
    void manualEvaluation();
    void updateEditor(ItemId itemId, bool activated);
    void setMousePosition(QPointF pos) { mMousePosition = pos; }
    const QPointF &mousePosition() const { return mMousePosition; }

Q_SIGNALS:
    void outputChanged(QString assembly);

private:
    void handleItemModified(const QModelIndex &index);
    void handleItemsModified(const QModelIndex &topLeft,
        const QModelIndex &bottomRight, const QVector<int> &roles);
    void handleSourceTypeChanged(SourceType sourceType);
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
    QString mProcessSourceType{ };
    QTimer *mProcessSourceTimer{ };
    ProcessSource* mProcessSource{ };

    QPointF mMousePosition{ };
};

#endif // SYNCHRONIZELOGIC_H
