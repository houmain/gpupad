#pragma once

#include "GLShader.h"

#if defined(OPENGL_ENABLED)

#  include "GLBuffer.h"
#  include <map>

class GLProgram
{
public:
    struct Uniform
    {
        QString name;
        GLint location;
        GLenum dataType;
        GLint arraySize;
    };

    struct Subroutine
    {
        QString name;
        QStringList subroutines;
    };
    using StageSubroutines =
        std::map<Shader::ShaderType, std::vector<Subroutine>>;

    struct BindingPoint
    {
        GLenum target;
        GLuint index;
    };

    GLProgram(const Program &program, const Session &session);
    bool operator==(const GLProgram &rhs) const;

    bool validate(GLContext &gl);
    bool link(GLContext &context);
    bool bind(GLContext &gl);
    void unbind(GLContext &gl);
    ItemId itemId() const { return mItemId; }
    const Session &session() const { return mSession; }
    const Reflection &reflection() const { return mReflection; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    BindingPoint getDescriptorBindingPoint(
        const SpvReflectDescriptorBinding &desc, int arrayIndex = 0) const;
    GLBuffer &getDynamicUniformBuffer(const QString &name, int size);
    const std::vector<GLShader> &shaders() const { return mShaders; }
    const std::vector<Uniform> &uniforms() const { return mUniforms; }
    const StageSubroutines &stageSubroutines() const
    {
        return mStageSubroutines;
    }
    GLPrintf &printf() { return mPrintf; }
    MessagePtrSet resetMessages();
    QString tryGetProgramBinary(GLContext &gl);

private:
    bool compileShaders(GLContext &gl, PrintfBase &printf);
    bool linkProgram(GLContext &gl);
    void generateReflectionFromProgram(GLContext &gl, GLuint program,
        bool generateGlobalUniformBlockBinding);
    void enumerateSubroutines(GLContext &gl, GLuint program);

    ItemId mItemId{};
    Session mSession{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mMessages;
    std::vector<GLShader> mShaders;
    std::vector<GLShader> mIncludableShaders;
    GLObject mProgramObject;
    Reflection mReflection;
    bool mFailed{};
    GLPrintf mPrintf;
    std::map<QString, GLBuffer> mDynamicUniformBuffers;
    std::vector<Uniform> mUniforms;
    std::map<QString, BindingPoint> mDescriptorBindingPoints;
    std::map<Shader::ShaderType, Spirv> mStageSpirv;
    std::map<Shader::ShaderType, std::vector<Subroutine>> mStageSubroutines;
};

#else // !defined(OPENGL_ENABLED)

class GLProgram
{
public:
    bool validate(GLContext &) { return false; }
    const Reflection &reflection() const { return mReflection; }
    MessagePtrSet resetMessages() { return {}; }
    QString tryGetProgramBinary(GLContext &) { return {}; }

private:
    Reflection mReflection;
};

#endif // !defined(OPENGL_ENABLED)
