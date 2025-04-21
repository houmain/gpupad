#pragma once

#include "GLProgram.h"

class QOpenGLTimerQuery;
class GLTarget;
class GLStream;
class GLBuffer;
class GLTexture;
using TimerQueryPtr = std::shared_ptr<const QOpenGLTimerQuery>;

struct GLUniformBinding
{
    ItemId bindingItemId;
    QString name;
    Binding::BindingType type;
    QStringList values;
    bool transpose;
};

struct GLSamplerBinding
{
    ItemId bindingItemId;
    QString name;
    GLTexture *texture;
    Binding::Filter minFilter;
    Binding::Filter magFilter;
    bool anisotropic;
    Binding::WrapMode wrapModeX;
    Binding::WrapMode wrapModeY;
    Binding::WrapMode wrapModeZ;
    QColor borderColor;
    Binding::ComparisonFunc comparisonFunc;
};

struct GLImageBinding
{
    ItemId bindingItemId;
    QString name;
    GLTexture *texture;
    int level;
    int layer;
    GLenum access;
    Binding::ImageFormat format;
};

struct GLBufferBinding
{
    ItemId bindingItemId;
    QString name;
    GLBuffer *buffer;
    ItemId blockItemId;
    QString offset;
    QString rowCount;
    int stride;
};

struct GLSubroutineBinding
{
    ItemId bindingItemId;
    QString name;
    QString subroutine;
};

struct GLBindings
{
    std::map<QString, GLUniformBinding> uniforms;
    std::map<QString, GLSamplerBinding> samplers;
    std::map<QString, GLImageBinding> images;
    std::map<QString, GLBufferBinding> buffers;
    std::map<QString, GLSubroutineBinding> subroutines;
};

class GLCall
{
public:
    explicit GLCall(const Call &call);

    ItemId itemId() const { return mCall.id; }
    TimerQueryPtr timerQuery() const { return mTimerQuery; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    GLProgram *program() { return mProgram; }

    void setProgram(GLProgram *program);
    void setTarget(GLTarget *target);
    void setVextexStream(GLStream *vertexStream);
    void setIndexBuffer(GLBuffer *indices, const Block &block);
    void setIndirectBuffer(GLBuffer *commands, const Block &block);
    void setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer);
    void setTextures(GLTexture *texture, GLTexture *fromTexture);
    bool applyBindings(const GLBindings &bindings, ScriptEngine &scriptEngine);
    void execute(MessagePtrSet &messages, ScriptEngine &scriptEngine);

private:
    std::shared_ptr<void> beginTimerQuery();
    int evaluateInt(ScriptEngine &scriptEngine, const QString &expression);
    uint32_t evaluateUInt(ScriptEngine &scriptEngine,
        const QString &expression);
    void executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeClearTexture(MessagePtrSet &messages);
    void executeCopyTexture(MessagePtrSet &messages);
    void executeClearBuffer(MessagePtrSet &messages);
    void executeCopyBuffer(MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);
    bool applyUniformBindings(const QString &name,
        const GLProgram::Interface::Uniform &uniform,
        const std::map<QString, GLUniformBinding> &bindings,
        ScriptEngine &scriptEngine);
    void applyUniformBinding(const GLProgram::Interface::Uniform &uniform,
        const GLUniformBinding &bindings, int offset, int count,
        ScriptEngine &scriptEngine);
    bool applySamplerBinding(const GLProgram::Interface::Uniform &uniform,
        const GLSamplerBinding &binding, int unit);
    bool applyImageBinding(const GLProgram::Interface::Uniform &uniform,
        const GLImageBinding &binding, int unit);
    bool applyBufferBinding(
        const GLProgram::Interface::BufferBindingPoint &bufferBindingPoint,
        const GLBufferBinding &binding, ScriptEngine &scriptEngine);
    bool applyDynamicBufferBindings(const QString &bufferName,
        const GLProgram::Interface::BufferBindingPoint &bufferBindingPoint,
        const std::map<QString, GLUniformBinding> &bindings,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(GLBuffer &buffer, const QString &name,
        const GLProgram::Interface::BufferMember &member,
        const std::map<QString, GLUniformBinding> &bindings,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBinding(GLBuffer &buffer,
        const GLProgram::Interface::BufferMember &member,
        const GLUniformBinding &binding, int offset, int count,
        ScriptEngine &scriptEngine);
    void selectSubroutines(Shader::ShaderType stage,
        const std::vector<GLProgram::Interface::Subroutine> &subroutines,
        const std::map<QString, GLSubroutineBinding> &bindings);
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
    std::shared_ptr<QOpenGLTimerQuery> mTimerQuery;
};
