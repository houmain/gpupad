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

class SynchronizeLogic : public QObject
{
    Q_OBJECT
public:
    explicit SynchronizeLogic(QObject *parent = nullptr);
    ~SynchronizeLogic() override;

    void resetRenderSession();
    void setValidateSource(bool validate);
    void setProcessSourceType(QString type);
    void setEvaluationMode(EvaluationMode mode);
    void updateFileCache();
    void updateEditor(ItemId itemId, bool activated);
    void setMousePosition(QPointF pos) { mMousePosition = pos; }
    const QPointF &mousePosition() const { return mMousePosition; }

signals:
    void outputChanged(QString assembly);

public slots:
    void resetEvaluation();
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
    void updateEditors();
    void updateTextureEditor(const Texture &texture,
        TextureEditor &editor);
    void updateBinaryEditor(const Buffer &buffer,
        BinaryEditor &editor);
    void evaluate(EvaluationType evaluationType);
    void processSource();

private:
    SessionModel &mModel;

    QTimer *mUpdateEditorsTimer{ };
    QSet<ItemId> mEditorItemsModified;

    QTimer *mEvaluationTimer{ };
    QSet<QString> mFilesModified;
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
