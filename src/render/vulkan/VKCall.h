#pragma once

#include "VKItem.h"
#include "VKPipeline.h"
#include "scripting/ScriptEngine.h"

class VKCall
{
public:
    VKCall(const Call &call, const Session &session);
    VKCall(const VKCall &) = delete;
    VKCall &operator=(const VKCall &) = delete;
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
    VKPipeline *getPipeline(VKContext &context);
    void execute(VKContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);

private:
    int evaluateInt(ScriptEngine &scriptEngine, const QString &expression);
    uint32_t evaluateUInt(ScriptEngine &scriptEngine,
        const QString &expression);
    std::optional<KDGpu::IndexType> getIndexType() const;
    void executeDraw(VKContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeCompute(VKContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeTraceRays(VKContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeClearTexture(VKContext &context, MessagePtrSet &messages);
    void executeCopyTexture(VKContext &context, MessagePtrSet &messages);
    void executeClearBuffer(VKContext &context, MessagePtrSet &messages);
    void executeCopyBuffer(VKContext &context, MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);

    const Call mCall;
    const CallKind mKind;
    const Session mSession;
    MessagePtrSet mMessages;
    VKProgram *mProgram{};
    VKTarget *mTarget{};
    VKStream *mVertexStream{};
    VKBuffer *mBuffer{};
    VKBuffer *mFromBuffer{};
    VKTexture *mTexture{};
    VKTexture *mFromTexture{};

    VKBuffer *mIndexBuffer{};
    QString mIndirectOffset;
    int mIndexSize{};
    QString mIndicesOffset;

    VKBuffer *mIndirectBuffer{};
    int mIndirectStride{};

    QSet<ItemId> mUsedItems;
    std::unique_ptr<VKPipeline> mPipeline;
    MessagePtrSet mPrintfMessages;
};
