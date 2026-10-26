#pragma once

#include "VKContext.h"
#include "VKPipeline.h"
#include "scripting/ScriptEngine.h"

class VKBuffer;
class VKTexture;

class VKCall
{
public:
    VKCall(const Call &call, const Session &session);
    VKCall(const VKCall &) = delete;
    VKCall &operator=(const VKCall &) = delete;
    ~VKCall();

    ItemId itemId() const { return mCall.id; }
    const CallKind& kind() const { return mKind; }
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
    void setAccelerationStructure(VKAccelerationStructure *accelStruct);
    void execute(VKContext &context, Bindings &&bindings,
        MessageList &messages, ScriptEngine &scriptEngine);

private:
    bool validateShaderTypes();
    int getMaxElementCount(ScriptEngine &scriptEngine);
    void executeDraw(VKContext &context, MessageList &messages,
        ScriptEngine &scriptEngine);
    void executeCompute(VKContext &context, MessageList &messages,
        ScriptEngine &scriptEngine);
    void executeTraceRays(VKContext &context, MessageList &messages,
        ScriptEngine &scriptEngine);
    void executeClearTexture(VKContext &context, MessageList &messages);
    void executeCopyTexture(VKContext &context, MessageList &messages);
    void executeClearBuffer(VKContext &context, MessageList &messages);
    void executeCopyBuffer(VKContext &context, MessageList &messages);
    void executeSwapTextures(MessageList &messages);
    void executeSwapBuffers(MessageList &messages);

    const Call mCall;
    const CallKind mKind;
    const Session mSession;
    MessageList mMessages;
    VKProgram *mProgram{ };
    VKTarget *mTarget{ };
    VKStream *mVertexStream{ };
    VKBuffer *mBuffer{ };
    VKBuffer *mFromBuffer{ };
    VKTexture *mTexture{ };
    VKTexture *mFromTexture{ };
    VKAccelerationStructure *mAccelerationStructure{ };

    VKBuffer *mIndexBuffer{ };
    QString mIndirectOffset;
    KDGpu::IndexType mIndexType{ };
    QString mIndicesOffset;
    QString mIndicesRowCount;
    int mIndicesPerRow{ };

    VKBuffer *mIndirectBuffer{ };
    int mIndirectStride{ };

    QSet<ItemId> mUsedItems;
    std::unique_ptr<VKPipeline> mPipeline;
    MessageList mPrintfMessages;
};
