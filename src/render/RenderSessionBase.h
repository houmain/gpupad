#pragma once

#include "RenderTask.h"
#include "MessageList.h"
#include "TextureData.h"
#include "session/SessionModel.h"
#include "scripting/IScriptRenderSession.h"
#include "scripting/ScriptSession.h"
#include <QMap>
#include <QMutex>

class RenderSessionBase : public RenderTask, public IScriptRenderSession
{
public:
    static std::unique_ptr<RenderSessionBase> create(RendererPtr renderer,
        const QString &basePath);

    RenderSessionBase(RendererPtr renderer, const QString &basePath,
        QObject *parent = nullptr);
    virtual ~RenderSessionBase();

    QThread *renderThread() override { return renderer().renderThread(); }
    const QString &basePath() override { return mBasePath; }
    void prepare(bool itemsChanged, EvaluationType evaluationType) override;
    void configure() override;
    void configured() override;
    void finish() override;
    void release() override;
    QSet<ItemId> usedItems() const override;
    SessionModel &sessionModelCopy() override { return mSessionModelCopy; }

    const Session &session() const;
    bool usesMouseState() const;
    bool usesKeyboardState() const;

    int getBufferSize(const Buffer &buffer);
    void evaluateBlockProperties(const Block &block, int *offset,
        int *rowCount);
    void evaluateTextureProperties(const Texture &texture, int *width,
        int *height, int *depth, int *layers);
    void evaluateTargetProperties(const Target &target, int *width, int *height,
        int *layers);

protected:
    struct GroupIteration
    {
        size_t commandQueueBeginIndex;
        int iterations;
        int iterationsLeft;
    };

    void setNextCommandQueueIndex(size_t index);
    virtual bool updatingPreviewTextures() const;

    const QString mBasePath;
    SessionModel mSessionModelCopy;
    std::unique_ptr<ScriptSession> mScriptSession;
    QSet<ItemId> mUsedItems;
    MessagePtrSet mMessages;
    bool mItemsChanged{};
    EvaluationType mEvaluationType{};
    QMap<ItemId, TextureData> mModifiedTextures;
    QMap<ItemId, QByteArray> mModifiedBuffers;
    size_t mNextCommandQueueIndex{};
    QMap<ItemId, GroupIteration> mGroupIterations;

private:
    MessagePtrSet mPrevMessages;
    mutable QMutex mUsedItemsCopyMutex;
    QSet<ItemId> mUsedItemsCopy;
};
