#pragma once

#include "GLProgram.h"
#include "../PipelineBase.h"

class GLTarget;
class GLStream;
class GLBuffer;
class GLTexture;
class GLAccelerationStructure;

class GLCall : public PipelineBase
{
public:
    GLCall(const Call &call, const Session &session);

    ItemId itemId() const { return mCall.id; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    GLProgram *program() { return mProgram; }

    void setProgram(GLProgram *program);
    void setTarget(GLTarget *target);
    void setVextexStream(GLStream *vertexStream);
    void setIndexBuffer(GLBuffer *indices, const Block &block);
    void setIndirectBuffer(GLBuffer *commands, const Block &block);
    void setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer);
    void setTextures(GLTexture *texture, GLTexture *fromTexture);
    void setAccelerationStructure(GLAccelerationStructure *accelStruct) { }
    bool validateShaderTypes();
    void execute(GLContext &context, Bindings &&bindings,
        MessagePtrSet &messages, ScriptEngine &scriptEngine);

private:
    void execute(GLContext &gl, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeDraw(GLContext &gl, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeCompute(GLContext &gl, MessagePtrSet &messages,
        ScriptEngine &scriptEngine);
    void executeClearTexture(GLContext &gl, MessagePtrSet &messages);
    void executeCopyTexture(GLContext &gl, MessagePtrSet &messages);
    void executeClearBuffer(GLContext &gl, MessagePtrSet &messages);
    void executeCopyBuffer(GLContext &gl, MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);
    bool updateBindings(GLContext &gl, ScriptEngine &scriptEngine);
    MessageType applyBinding(const SpvReflectDescriptorBinding &desc,
        uint32_t arrayElement, bool isVariableLengthArray, GLContext &gl,
        ScriptEngine &scriptEngine);
    void applyUniformBindings(const GLProgram::Uniform &uniform, GLContext &gl,
        ScriptEngine &scriptEngine);
    void applyUniformBinding(const GLProgram::Uniform &uniform,
        const UniformBinding &bindings, int offset, int count, GLContext &gl,
        ScriptEngine &scriptEngine);
    bool applySamplerBinding(const SpvReflectDescriptorBinding &desc,
        GLContext &gl, const SamplerBinding &binding);
    bool applyImageBinding(const SpvReflectDescriptorBinding &desc,
        GLContext &gl, const ImageBinding &binding);
    void selectSubroutines(GLContext &gl);
    bool bindVertexStream(GLContext &gl);
    void unbindVertexStream(GLContext &gl);
    GLenum getIndexType() const;
    int getMaxElementCount(ScriptEngine &scriptEngine);

    const Call mCall;
    const CallKind mKind;
    GLProgram *mProgram{};
    GLTarget *mTarget{};
    GLStream *mVertexStream{};
    GLBuffer *mBuffer{};
    GLBuffer *mFromBuffer{};
    GLTexture *mTexture{};
    GLTexture *mFromTexture{};

    GLBuffer *mIndexBuffer{};
    QString mIndirectOffset;
    int mIndexSize{};
    int mIndicesPerRow{};
    QString mIndicesOffset;
    QString mIndicesRowCount;

    GLBuffer *mIndirectBuffer{};
    GLint mIndirectStride{};
};
