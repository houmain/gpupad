#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include "spirv-reflect/spirv_reflect.h"

class Spirv
{
public:
    class Interface
    {
    public:
        Interface() = default;
        explicit Interface(const std::vector<uint32_t> &spirv);
        ~Interface();

        explicit operator bool() const;
        const SpvReflectShaderModule *operator->() const;

    private:
        std::shared_ptr<SpvReflectShaderModule> mModule;
    };

    static Spirv generate(const Session &session, Shader::Language language,
        Shader::ShaderType shaderType, const QStringList &sources,
        const QStringList &fileNames, const QString &entryPoint,
        int shiftBindingsInSet0, ItemId itemId, MessagePtrSet &messages);

    Spirv() = default;
    explicit Spirv(std::vector<uint32_t> spirv);

    explicit operator bool() const { return !mSpirv.empty(); }
    const std::vector<uint32_t> &spirv() const { return mSpirv; }
    Interface getInterface() const;

private:
    std::vector<uint32_t> mSpirv;
};
