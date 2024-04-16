#pragma once

#include "session/Item.h"
#include "MessageList.h"

namespace glslang 
{
    QString generateSpirV(const QString &source, Shader::ShaderType shaderType, MessagePtrSet &messages);
    QString generateAST(const QString &source, Shader::ShaderType shaderType, MessagePtrSet &messages);
    QString preprocess(const QString &source, MessagePtrSet &messages);

    QString generateGLSL(const QString &source, Shader::ShaderType shaderType,
        Shader::Language language, const QString &entryPoint, 
        const QString &fileName, MessagePtrSet &messages);

} // namespace
