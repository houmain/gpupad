#pragma once

#include "Evaluation.h"
#include "MessageList.h"
#include "Renderer.h"
#include "TextureData.h"
#include "session/SessionModel.h"
#include <QMap>
#include <QMutex>

class ScriptSession;

class RenderSessionBase
{
public:
    static std::unique_ptr<RenderSessionBase> create(Renderer &renderer);

    RenderSessionBase();
    virtual ~RenderSessionBase();
    RenderSessionBase(const RenderSessionBase &) = delete;
    RenderSessionBase &operator=(const RenderSessionBase &) = delete;

    void prepare(bool itemsChanged, EvaluationType evaluationType);
    void configure();
    void configured();
    virtual void render() = 0;
    virtual void finish();
    virtual void release() = 0;

    QSet<ItemId> usedItems() const;
    bool usesMouseState() const;
    bool usesKeyboardState() const;

protected:
    struct GroupIteration
    {
        size_t commandQueueBeginIndex;
        int iterations;
        int iterationsLeft;
    };

    void setNextCommandQueueIndex(size_t index);
    virtual bool updatingPreviewTextures() const;

    SessionModel mSessionCopy;
    QScopedPointer<ScriptSession> mScriptSession;
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
