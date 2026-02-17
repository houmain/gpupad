#pragma once

#include "../session/Item.h"
#include "spirv-reflect/spirv_reflect.h"
#include <memory>

class Reflection
{
public:
    struct Builder
    {
        struct BlockVariable
        {
            std::string name;
            uint32_t offset;
            uint32_t size;
            std::string typeName;
            SpvReflectTypeFlags typeFlags;
            SpvReflectDecorationFlags decorationFlags;
            SpvReflectNumericTraits numeric;
            SpvReflectImageTraits image;
            SpvReflectArrayTraits array;
            std::vector<BlockVariable> members;
        };

        struct DescriptorBinding
        {
            SpvReflectDescriptorType descriptorType;
            std::string name;
            std::string typeName;
            SpvReflectDecorationFlags decorationFlags;
            SpvReflectNumericTraits numeric;
            SpvReflectImageTraits image;
            SpvReflectArrayTraits array;
            BlockVariable block;
            uint32_t binding;
            uint32_t set;
        };

        struct InterfaceVariable
        {
            std::string name;
            std::string semantic;
            int builtIn;
            SpvReflectFormat format;
            uint32_t location;
        };

        SpvReflectShaderStageFlagBits shaderStage;
        std::vector<InterfaceVariable> inputs;
        std::vector<InterfaceVariable> outputs;
        std::vector<DescriptorBinding> descriptorsBindings;
    };

    Reflection() = default;
    explicit Reflection(const std::vector<uint32_t> &spirv);
    explicit Reflection(std::unique_ptr<Builder> builder);
    ~Reflection();

    explicit operator bool() const;
    const SpvReflectShaderModule &operator*() const;
    const SpvReflectShaderModule *operator->() const;

    std::span<const SpvReflectDescriptorBinding> descriptorBindings() const
    {
        return std::span(mModule->descriptor_bindings,
            mModule->descriptor_binding_count);
    }

    std::span<const SpvReflectBlockVariable> pushConstantBlocks() const
    {
        return std::span(mModule->push_constant_blocks,
            mModule->push_constant_block_count);
    }

    std::span<const SpvReflectInterfaceVariable *const> inputVariables() const
    {
        return std::span(mModule->input_variables,
            mModule->input_variable_count);
    }

private:
    std::shared_ptr<const Builder> mBuilder;
    std::shared_ptr<const SpvReflectShaderModule> mModule;
};

constexpr const char globalUniformBlockName[]{ "$Global" };
bool isGlobalUniformBlockName(const char *name);
bool isGlobalUniformBlockName(const QString &name);
QString removeGlobalUniformBlockName(QString string);
bool isBufferBinding(SpvReflectDescriptorType type);
bool isBuiltIn(const SpvReflectInterfaceVariable &variable);
uint32_t getBindingArraySize(const SpvReflectBindingArrayTraits &array);
SpvReflectShaderStageFlagBits getShaderStage(Shader::ShaderType shaderType);
Field::DataType getBufferMemberDataType(
    const SpvReflectBlockVariable &variable);
int getBufferMemberColumnCount(const SpvReflectBlockVariable &variable);
int getBufferMemberRowCount(const SpvReflectBlockVariable &variable);
int getBufferMemberColumnStride(const SpvReflectBlockVariable &variable);
int getBufferMemberArraySize(const SpvReflectBlockVariable &variable);
int getBufferMemberArrayStride(const SpvReflectBlockVariable &variable);
GLenum getBufferMemberGLType(const SpvReflectBlockVariable &variable);

QString getJsonString(const Reflection &reflection);

inline uint32_t alignUp(uint32_t offset, uint32_t align)
{
    return (offset + (align - 1)) & ~(align - 1);
}
