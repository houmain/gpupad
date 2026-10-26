#pragma once

#include "message/MessageList.h"
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

    Spirv compileSpirvVulkanGLSL(Shader::ShaderType shaderType,
        const QString &source);

    std::map<Shader::ShaderType, Spirv> compileSpirv(const Session &session,
        const std::vector<Input> &inputs, ItemId programItemId,
        MessageList &messages);

    QString preprocess(const Session &session, Shader::ShaderType shaderType,
        const QStringList &sources, const QStringList &fileNames,
        const QString &entryPoint, ItemId itemId, const QString &includePaths,
        MessageList &messages);

    QString disassemble(const Spirv &spirv);
    QString generateGLSL(const Spirv &spirv, ItemId itemId,
        MessageList &messages);
    QString generateHLSL(const Spirv &spirv, ItemId itemId,
        MessageList &messages);
    Spirv stripReflection(const Spirv &spirv);
    bool validateSpirv(const Session &session, const Spirv &spirv,
        QString &Errormessage);

    QString generateAST(const Session &session, Shader::ShaderType shaderType,
        const QStringList &sources, const QStringList &fileNames,
        const QString &entryPoint, ItemId itemId, const QString &includePaths,
        MessageList &messages);

}; // namespace ShaderCompiler
