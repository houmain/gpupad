#pragma once

#include "GLShader.h"
#include "GLBuffer.h"
#include <map>

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

    bool validate();
    bool link(GLContext &context);
    bool bind();
    void unbind();
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
    QString tryGetProgramBinary();

private:
    bool compileShaders(PrintfBase &printf);
    bool linkProgram();
    void generateReflectionFromProgram(GLuint program,
        bool generateGlobalUniformBlockBinding);
    void enumerateSubroutines(GLuint program);

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
