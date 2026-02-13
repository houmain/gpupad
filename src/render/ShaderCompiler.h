#pragma once

#include "MessageList.h"
#include "session/Item.h"

class Spirv final : public std::vector<uint32_t>
{
    using vector::vector;
};

namespace ShaderCompiler {

    struct Input
    {
        Shader::ShaderType shaderType;
        QStringList sources;
        QStringList fileNames;
        QString entryPoint;
        QString includePaths;
        ItemId itemId;
    };

    std::map<Shader::ShaderType, Spirv> compileSpirv(const Session &session,
        const std::vector<Input> &inputs, ItemId programItemId,
        MessagePtrSet &messages);

    QString preprocess(const Session &session, Shader::ShaderType shaderType,
        const QStringList &sources, const QStringList &fileNames,
        const QString &entryPoint, ItemId itemId, const QString &includePaths,
        MessagePtrSet &messages);

    QString disassemble(const Spirv &spirv);
    QString generateGLSL(const Spirv &spirv, ItemId itemId,
        MessagePtrSet &messages);
    QString generateHLSL(const Spirv &spirv, ItemId itemId,
        MessagePtrSet &messages);
    Spirv stripReflection(const Spirv &spirv);

    QString generateAST(const Session &session, Shader::ShaderType shaderType,
        const QStringList &sources, const QStringList &fileNames,
        const QString &entryPoint, ItemId itemId, const QString &includePaths,
        MessagePtrSet &messages);

}; // namespace ShaderCompiler
