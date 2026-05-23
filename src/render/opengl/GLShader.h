#pragma once

#include "GLPrintf.h"
#include "render/ShaderBase.h"

#if defined(OPENGL_ENABLED)

class GLShader : public ShaderBase
{
public:
    GLShader(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);

    bool compile(GLContext &gl);
    bool compile(GLContext &gl, PrintfBase &printf);
    bool specialize(GLContext &gl, const Spirv &spirv);
    GLuint shaderObject() const { return mShaderObject; }

private:
    GLObject createShader(GLContext &gl);
    bool setShaderObject(GLContext &gl, GLObject shader,
        const QStringList &usedFileNames);
    QStringList preprocessorDefinitions() const override;

    GLObject mShaderObject;
};

#endif // defined(OPENGL_ENABLED)
