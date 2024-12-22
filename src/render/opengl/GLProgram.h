#pragma once

#include "GLShader.h"
#include "GLBuffer.h"
#include "scripting/ScriptEngine.h"
#include <map>

class GLProgram
{
public:
    struct Interface
    {
        struct Uniform
        {
            GLint location;
            GLenum dataType;
            GLint size;
        };

        struct BufferMember
        {
            GLenum dataType;
            GLint size;
            GLint offset;
            GLint arrayStride;
            GLint matrixStride;
            bool isRowMajor;
        };

        struct BufferBindingPoint
        {
            GLenum target;
            GLuint index;
            std::map<QString, BufferMember> members;
            int minimumSize;
            bool readonly;
        };

        struct Subroutine
        {
            QString name;
            QStringList subroutines;
        };

        std::map<QString, GLuint> attributeLocations;
        std::map<QString, Uniform> uniforms;
        std::map<QString, BufferBindingPoint> bufferBindingPoints;
        std::map<Shader::ShaderType, std::vector<Subroutine>> stageSubroutines;
    };

    GLProgram(const Program &program, const Session &session);
    bool operator==(const GLProgram &rhs) const;

    bool link();
    bool bind();
    void unbind();
    const Session &session() const { return mSession; }
    const Interface &interface() const { return mInterface; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    GLBuffer &getDynamicUniformBuffer(const QString &name, int size);

private:
    bool compileShaders();
    bool linkProgram();
    void fillInterface(GLuint program, Interface &interface);
    void applyPrintfBindings();

    ItemId mItemId{};
    Session mSession{};
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<GLShader> mShaders;
    std::vector<GLShader> mIncludableShaders;
    GLObject mProgramObject;
    Interface mInterface;
    bool mFailed{};
    GLPrintf mPrintf;
    MessagePtrSet mPrintfMessages;
    Interface::BufferBindingPoint mPrintfBufferBindingPoint{};
    std::map<QString, GLBuffer> mDynamicUniformBuffers;
};
