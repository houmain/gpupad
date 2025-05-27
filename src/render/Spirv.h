#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include "spirv-reflect/spirv_reflect.h"

class Spirv final
{
public:
    struct Input
    {
        Shader::Language language;
        Shader::ShaderType shaderType;
        QStringList sources;
        QStringList fileNames;
        QString entryPoint;
        QString includePaths;
        ItemId itemId;
    };

    class Interface
    {
    public:
        Interface() = default;
        explicit Interface(const std::vector<uint32_t> &spirv);
        ~Interface();

        explicit operator bool() const;
        const SpvReflectShaderModule &operator*() const;
        const SpvReflectShaderModule *operator->() const;

    private:
        std::shared_ptr<SpvReflectShaderModule> mModule;
    };

    static std::map<Shader::ShaderType, Spirv> compile(const Session &session,
        const std::vector<Input> &inputs, ItemId programItemId,
        MessagePtrSet &messages);

    static QString preprocess(const Session &session, Shader::Language language,
        Shader::ShaderType shaderType, const QStringList &sources,
        const QStringList &fileNames, const QString &entryPoint, ItemId itemId,
        const QString &includePaths, MessagePtrSet &messages);

    static QString disassemble(const Spirv &spirv);

    static QString generateAST(const Session &session,
        Shader::Language language, Shader::ShaderType shaderType,
        const QStringList &sources, const QStringList &fileNames,
        const QString &entryPoint, ItemId itemId, const QString &includePaths,
        MessagePtrSet &messages);

    Spirv() = default;
    explicit Spirv(std::vector<uint32_t> spirv);

    explicit operator bool() const { return !mSpirv.empty(); }
    const std::vector<uint32_t> &spirv() const { return mSpirv; }
    Interface getInterface() const;

private:
    std::vector<uint32_t> mSpirv;
};

constexpr const char globalUniformBlockName[]{ "$Global" };
bool isGlobalUniformBlockName(const char *name);
bool isGlobalUniformBlockName(QStringView name);
QString removeGlobalUniformBlockName(QString string);

QString getJsonString(const SpvReflectShaderModule &module);
uint32_t getBindingArraySize(const SpvReflectBindingArrayTraits &array);
Field::DataType getBufferMemberDataType(
    const SpvReflectBlockVariable &variable);
int getBufferMemberColumnCount(const SpvReflectBlockVariable &variable);
int getBufferMemberRowCount(const SpvReflectBlockVariable &variable);
int getBufferMemberColumnStride(const SpvReflectBlockVariable &variable);
int getBufferMemberArraySize(const SpvReflectBlockVariable &variable);
int getBufferMemberArrayStride(const SpvReflectBlockVariable &variable);
