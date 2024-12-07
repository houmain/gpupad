#pragma once

#include "GLShader.h"
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

        struct BufferElement
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
            std::map<QString, BufferElement> elements;
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
    GLObject mProgramObject;
    Interface mInterface;
    bool mFailed{};
    GLPrintf mPrintf;
    MessagePtrSet mPrintfMessages;
    Interface::BufferBindingPoint mPrintfBufferBindingPoint{};
};
