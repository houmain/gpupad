#pragma once

#include "MessageList.h"
#include "session/Item.h"

class Spirv final
{
public:
    struct Input
    {
        Shader::ShaderType shaderType;
        QStringList sources;
        QStringList fileNames;
        QString entryPoint;
        QString includePaths;
        ItemId itemId;
    };

    static std::map<Shader::ShaderType, Spirv> compile(const Session &session,
        const std::vector<Input> &inputs, ItemId programItemId,
        MessagePtrSet &messages);

    static QString preprocess(const Session &session,
        Shader::ShaderType shaderType, const QStringList &sources,
        const QStringList &fileNames, const QString &entryPoint, ItemId itemId,
        const QString &includePaths, MessagePtrSet &messages);

    static QString disassemble(const Spirv &spirv);
    static QString generateGLSL(const Spirv &spirv, ItemId itemId,
        MessagePtrSet &messages);
    static QString generateHLSL(const Spirv &spirv, ItemId itemId,
        MessagePtrSet &messages);
    static std::vector<uint32_t> stripReflection(const Spirv &spirv);

    static QString generateAST(const Session &session,
        Shader::ShaderType shaderType, const QStringList &sources,
        const QStringList &fileNames, const QString &entryPoint, ItemId itemId,
        const QString &includePaths, MessagePtrSet &messages);

    Spirv() = default;
    explicit Spirv(std::vector<uint32_t> spirv);

    explicit operator bool() const { return !mSpirv.empty(); }
    const std::vector<uint32_t> &spirv() const { return mSpirv; }

private:
    std::vector<uint32_t> mSpirv;
};
