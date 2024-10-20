#pragma once

#include "GLPrintf.h"
#include "render/ShaderBase.h"

class GLShader : public ShaderBase
{
public:
    static void parseLog(const QString &log, MessagePtrSet &messages,
        ItemId itemId, const QStringList &fileNames);

    GLShader(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const QString &preamble, const QString &includePaths);

    bool compile(GLPrintf *printf = nullptr, bool failSilently = false);
    GLuint shaderObject() const { return mShaderObject; }
    QString getAssembly();

private:
    QStringList preprocessorDefinitions() const override;

    GLObject mShaderObject;
};
