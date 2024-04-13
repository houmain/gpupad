#pragma once

#include "VKItem.h"
#include "VKPipeline.h"
#include "scripting/ScriptEngine.h"

class VKCall
{
public:
    explicit VKCall(const Call &call);
    VKCall(const VKCall&) = delete;
    VKCall& operator=(const VKCall&) = delete;
    ~VKCall();

    ItemId itemId() const { return mCall.id; }
    VKProgram *program() { return mProgram; }
    VKTarget *target() { return mTarget; }
    VKStream *vertexStream() { return mVertexStream; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    void setProgram(VKProgram *program);
    void setTarget(VKTarget *target);
    void setVextexStream(VKStream *vertexStream);
    void setIndexBuffer(VKBuffer *indices, const Block &block);
    void setIndirectBuffer(VKBuffer *commands, const Block &block);
    void setBuffers(VKBuffer *buffer, VKBuffer *fromBuffer);
    void setTextures(VKTexture *texture, VKTexture *fromTexture);
    VKPipeline *createPipeline(VKContext &context);
    void execute(VKContext &context, MessagePtrSet &messages, ScriptEngine &scriptEngine);

private:
    int evaluateInt(ScriptEngine &scriptEngine, const QString &expression);
    uint32_t evaluateUInt(ScriptEngine &scriptEngine, const QString &expression);
    void executeDraw(VKContext &context, MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeCompute(VKContext &context, MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeClearTexture(VKContext &context, MessagePtrSet &messages);
    void executeCopyTexture(MessagePtrSet &messages);
    void executeClearBuffer(MessagePtrSet &messages);
    void executeCopyBuffer(MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);

    MessagePtrSet mMessages;
    Call mCall{ };
    VKProgram *mProgram{ };
    VKTarget *mTarget{ };
    VKStream *mVertexStream{ };
    VKBuffer *mBuffer{ };
    VKBuffer *mFromBuffer{ };
    VKTexture *mTexture{ };
    VKTexture *mFromTexture{ };

    VKBuffer *mIndexBuffer{ };
    QString mIndirectOffset;
    KDGpu::IndexType mIndexType{ };
    QString mIndicesOffset;

    VKBuffer *mIndirectBuffer{ };
    int mIndirectStride{ };

    QSet<ItemId> mUsedItems;
    std::unique_ptr<VKPipeline> mPipeline;
};
