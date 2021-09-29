#pragma once

#include "session/Item.h"
#include "MessageList.h"

namespace glslang 
{

QString generateSpirV(const QString &source, Shader::ShaderType shaderType);
QString generateAST(const QString &source, Shader::ShaderType shaderType);
QString preprocess(const QString &source);
QString generateGLSL(const QString &source, Shader::ShaderType shaderType,
    Shader::Language language, const QString &entryPoint, 
    const QString &fileName, ItemId itemId, MessagePtrSet &messages);

} // namespace
