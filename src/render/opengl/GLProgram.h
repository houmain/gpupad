#pragma once

#include "GLShader.h"
#include "GLBuffer.h"
#include "scripting/ScriptEngine.h"
#include <map>

class GLProgram
{
public:
    struct Uniform
    {
        GLint location;
        GLenum dataType;
        GLint arrayElements;
    };

    struct Subroutine
    {
        QString name;
        QStringList subroutines;
    };
    using StageSubroutines =
        std::map<Shader::ShaderType, std::vector<Subroutine>>;

    GLProgram(const Program &program, const Session &session);
    bool operator==(const GLProgram &rhs) const;

    bool link(GLContext &context);
    bool bind();
    void unbind();
    ItemId itemId() const { return mItemId; }
    const Session &session() const { return mSession; }
    const Reflection &reflection() const { return mReflection; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int getUniformLocation(const SpvReflectDescriptorBinding &desc) const;
    GLBuffer &getDynamicUniformBuffer(const QString &name, int size);
    const std::vector<GLShader> &shaders() const { return mShaders; }
    const std::map<QString, Uniform> &uniforms() const { return mUniforms; }
    const StageSubroutines &stageSubroutines() const
    {
        return mStageSubroutines;
    }
    GLPrintf &printf() { return mPrintf; }

private:
    bool compileShaders();
    bool linkProgram();
    bool getReflectionFromSpirv() const;
    void generateReflectionFromProgram(GLuint program);
    void automapDescriptorBindings();

    ItemId mItemId{};
    Session mSession{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<GLShader> mShaders;
    std::vector<GLShader> mIncludableShaders;
    GLObject mProgramObject;
    Reflection mReflection;
    bool mFailed{};
    GLPrintf mPrintf;
    std::map<QString, GLBuffer> mDynamicUniformBuffers;
    std::map<QString, Uniform> mUniforms;
    std::map<Shader::ShaderType, std::vector<Subroutine>> mStageSubroutines;
};
