#pragma once

#include "GLProgram.h"

class GLTarget;
class GLStream;
class GLBuffer;
class GLTexture;

class GLAccelerationStructure
{
public:
    explicit GLAccelerationStructure(const AccelerationStructure &accelStruct)
    {
    }

    bool operator==(const GLAccelerationStructure &rhs) const { return true; }

    void setVertexBuffer(int instanceIndex, int geometryIndex, GLBuffer *buffer,
        const Block &block, GLRenderSession &renderSession)
    {
    }

    void setIndexBuffer(int instanceIndex, int geometryIndex, GLBuffer *buffer,
        const Block &block, GLRenderSession &renderSession)
    {
    }

    void setTransformBuffer(int instanceIndex, int geometryIndex,
        GLBuffer *buffer, const Block &block, GLRenderSession &renderSession)
    {
    }
};

class GLCall
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
    std::shared_ptr<void> beginTimerQuery(GLContext &context);
    void execute(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeClearTexture(MessagePtrSet &messages);
    void executeCopyTexture(MessagePtrSet &messages);
    void executeClearBuffer(MessagePtrSet &messages);
    void executeCopyBuffer(MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);
    bool applyBindings(const Bindings &bindings, ScriptEngine &scriptEngine);
    bool applyUniformBindings(const QString &name,
        const GLProgram::Interface::Uniform &uniform,
        const std::map<QString, UniformBinding> &bindings,
        ScriptEngine &scriptEngine);
    void applyUniformBinding(const GLProgram::Interface::Uniform &uniform,
        const UniformBinding &bindings, int offset, int count,
        ScriptEngine &scriptEngine);
    bool applySamplerBinding(const GLProgram::Interface::Uniform &uniform,
        const SamplerBinding &binding);
    bool applyImageBinding(const GLProgram::Interface::Uniform &uniform,
        const ImageBinding &binding);
    bool applyBufferBinding(
        const GLProgram::Interface::BufferBindingPoint &bufferBindingPoint,
        const BufferBinding &binding, ScriptEngine &scriptEngine);
    bool applyDynamicBufferBindings(const QString &bufferName,
        const GLProgram::Interface::BufferBindingPoint &bufferBindingPoint,
        const std::map<QString, UniformBinding> &bindings,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(GLBuffer &buffer, const QString &name,
        const GLProgram::Interface::BufferMember &member,
        const std::map<QString, UniformBinding> &bindings,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBinding(GLBuffer &buffer,
        const GLProgram::Interface::BufferMember &member,
        const UniformBinding &binding, int offset, int count,
        ScriptEngine &scriptEngine);
    void selectSubroutines(Shader::ShaderType stage,
        const std::vector<GLProgram::Interface::Subroutine> &subroutines,
        const std::map<QString, SubroutineBinding> &bindings);
    bool bindVertexStream();
    void unbindVertexStream();
    GLenum getIndexType() const;
    int getMaxElementCount(ScriptEngine &scriptEngine);

    const Call mCall;
    const CallKind mKind;
    MessagePtrSet mMessages;
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

    QSet<ItemId> mUsedItems;
};
