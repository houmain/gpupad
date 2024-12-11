#pragma once

#include "GLPrintf.h"
#include "render/ShaderBase.h"

class GLShader : public ShaderBase
{
public:
    static void parseLog(const QString &log, MessagePtrSet &messages,
        ItemId itemId, const QStringList &fileNames);

    GLShader(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);

    bool compile(ShaderPrintf &printf);
    bool specialize(const Spirv &spirv);
    GLuint shaderObject() const { return mShaderObject; }

private:
    GLObject createShader();
    bool setShaderObject(GLObject shader, const QStringList &usedFileNames);
    QStringList preprocessorDefinitions() const override;

    GLObject mShaderObject;
};

QString tryGetProgramBinary(const GLShader &shader);
void tryGetLinkerWarnings(const GLShader &shader, MessagePtrSet &messages);
