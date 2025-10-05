#pragma once

#if defined(_WIN32)

#include "D3DContext.h"
#include "D3DPipeline.h"
#include "scripting/ScriptEngine.h"

class D3DBuffer;
class D3DTexture;

class D3DCall
{
public:
    D3DCall(const Call &call, const Session &session);
    D3DCall(const D3DCall &) = delete;
    D3DCall &operator=(const D3DCall &) = delete;
    ~D3DCall();

    ItemId itemId() const { return mCall.id; }
    D3DProgram *program() { return mProgram; }
    D3DTarget *target() { return mTarget; }
    D3DStream *vertexStream() { return mVertexStream; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    void setProgram(D3DProgram *program);
    void setTarget(D3DTarget *target);
    void setVextexStream(D3DStream *vertexStream);
    void setIndexBuffer(D3DBuffer *indices, const Block &block);
    void setIndirectBuffer(D3DBuffer *commands, const Block &block);
    void setBuffers(D3DBuffer *buffer, D3DBuffer *fromBuffer);
    void setTextures(D3DTexture *texture, D3DTexture *fromTexture);
    void setAccelerationStructure(D3DAccelerationStructure *accelStruct);
    void execute(D3DContext &context, Bindings &&bindings,
        MessagePtrSet &messages, ScriptEngine &scriptEngine);

private:
    bool validateShaderTypes();
    int getMaxElementCount(ScriptEngine &scriptEngine);
    void executeDraw(D3DContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeCompute(D3DContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeTraceRays(D3DContext &context, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeClearTexture(D3DContext &context, MessagePtrSet &messages);
    void executeCopyTexture(D3DContext &context, MessagePtrSet &messages);
    void executeClearBuffer(D3DContext &context, MessagePtrSet &messages);
    void executeCopyBuffer(D3DContext &context, MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);

    const Call mCall;
    const CallKind mKind;
    const Session mSession;
    MessagePtrSet mMessages;
    D3DProgram *mProgram{};
    D3DTarget *mTarget{};
    D3DStream *mVertexStream{};
    D3DBuffer *mBuffer{};
    D3DBuffer *mFromBuffer{};
    D3DTexture *mTexture{};
    D3DTexture *mFromTexture{};
    D3DAccelerationStructure *mAccelerationStructure{};

    D3DBuffer *mIndexBuffer{};
    QString mIndirectOffset;
    int mIndexSize{};
    QString mIndicesOffset;
    QString mIndicesRowCount;
    int mIndicesPerRow{};

    D3DBuffer *mIndirectBuffer{};
    int mIndirectStride{};

    QSet<ItemId> mUsedItems;
    std::unique_ptr<D3DPipeline> mPipeline;
    MessagePtrSet mPrintfMessages;
};

#endif // _WIN32
