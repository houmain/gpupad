#pragma once

#include "MessageList.h"
#include "session/Item.h"

namespace glslang {
    QString generateSpirV(const QString &source, Shader::ShaderType shaderType,
        MessagePtrSet &messages);
    QString generateAST(const QString &source, Shader::ShaderType shaderType,
        MessagePtrSet &messages);
    QString preprocess(const QString &source, MessagePtrSet &messages);

} // namespace glslang
