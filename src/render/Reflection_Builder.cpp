
#include "Reflection.h"

namespace {
    struct SpvReflectShaderModuleExt : SpvReflectShaderModule
    {
        std::vector<SpvReflectInterfaceVariable> interfaceVariables;
        std::vector<SpvReflectInterfaceVariable *> inputVariables;
        std::vector<SpvReflectInterfaceVariable *> outputVariables;
        std::vector<SpvReflectDescriptorBinding> descriptorBindings;
        std::vector<SpvReflectTypeDescription> typeDescriptions;
        std::vector<SpvReflectBlockVariable> blockMembers;

        virtual ~SpvReflectShaderModuleExt() = default;
    };

    size_t countBlockMembersRec(const Reflection::Builder::BlockVariable &var)
    {
        auto count = size_t{ 1 };
        for (const auto &member : var.members)
            count += countBlockMembersRec(member);
        return count;
    }

    SpvReflectTypeFlags getDescriptorTypeFlags(SpvReflectDescriptorType type)
    {
        switch (type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            return SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER;

        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE
                | SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER;

        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE;

        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE;

        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK;

        default: Q_ASSERT(!"type not handled"); break;
        }
        return {};
    }

    SpvReflectTypeDescription getTypeDescription(SpvReflectFormat format)
    {
        auto isSigned = true;
        auto typeFlags = SpvReflectTypeFlags{};
        switch (format) {
        default: break;
        case SPV_REFLECT_FORMAT_R32_UINT:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_INT;
            isSigned = false;
            break;
        case SPV_REFLECT_FORMAT_R32G32_UINT:
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_INT;
            typeFlags |= SPV_REFLECT_TYPE_FLAG_VECTOR;
            isSigned = false;
            break;

        case SPV_REFLECT_FORMAT_R32_SINT:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_INT;
            break;
        case SPV_REFLECT_FORMAT_R32G32_SINT:
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_INT;
            typeFlags |= SPV_REFLECT_TYPE_FLAG_VECTOR;
            break;

        case SPV_REFLECT_FORMAT_R32_SFLOAT:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_INT;
            break;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_FLOAT;
            typeFlags |= SPV_REFLECT_TYPE_FLAG_VECTOR;
            break;
        }

        auto componentCount = 1u;
        switch (format) {
        default:                                     break;
        case SPV_REFLECT_FORMAT_R32G32_UINT:
        case SPV_REFLECT_FORMAT_R32G32_SINT:
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:       componentCount = 2; break;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:    componentCount = 3; break;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: componentCount = 4; break;
        }

        return SpvReflectTypeDescription{
            .type_flags = typeFlags,
            .traits = { 
                .numeric = {
                    .scalar = {
                        .width = 32,
                        .signedness = (isSigned ? 1u : 0u),
                    },
                    .vector = {
                        .component_count = componentCount,
                    },
                },
            },
        };
    }

    std::unique_ptr<SpvReflectShaderModuleExt> buildSpvReflectShaderModule(
        const Reflection::Builder &builder)
    {
        using BlockVariable = Reflection::Builder::BlockVariable;

        const auto cstr_or_null = [](const std::string &str) {
            return (str.empty() ? nullptr : str.c_str());
        };

        auto module = std::make_unique<SpvReflectShaderModuleExt>();
        module->shader_stage = builder.shaderStage;

        auto totalBlockMembers = size_t{};
        for (const auto &descriptor : builder.descriptorsBindings)
            for (const auto &member : descriptor.block.members)
                totalBlockMembers += countBlockMembersRec(member);
        module->blockMembers.reserve(totalBlockMembers);

        auto totalTypeDescriptions = size_t{};
        totalTypeDescriptions += builder.descriptorsBindings.size();
        totalTypeDescriptions += builder.inputs.size();
        totalTypeDescriptions += builder.outputs.size();
        totalTypeDescriptions += totalBlockMembers;
        module->typeDescriptions.reserve(totalTypeDescriptions);

        // inputs/outputs
        module->interfaceVariables.reserve(
            builder.inputs.size() + builder.outputs.size());
        module->inputVariables.reserve(builder.inputs.size());
        module->outputVariables.reserve(builder.outputs.size());
        for (const auto inputsOutputs : { &builder.inputs, &builder.outputs })
            for (const auto &inputOutput : *inputsOutputs) {
                auto &variable = module->interfaceVariables.emplace_back();
                variable.name = inputOutput.name.c_str();
                variable.location = inputOutput.location;
                variable.semantic = cstr_or_null(inputOutput.semantic);

                auto &typeDesc = module->typeDescriptions.emplace_back();
                typeDesc = getTypeDescription(inputOutput.format);
                typeDesc.type_name = inputOutput.name.c_str();
                variable.type_description = &typeDesc;
                variable.built_in = inputOutput.builtIn;

                auto &variables = (inputsOutputs == &builder.inputs
                        ? module->inputVariables
                        : module->outputVariables);
                variables.push_back(&variable);
            }
        module->interface_variables = module->interfaceVariables.data();
        module->interface_variable_count =
            static_cast<uint32_t>(module->interfaceVariables.size());
        module->input_variables = module->inputVariables.data();
        module->input_variable_count =
            static_cast<uint32_t>(module->inputVariables.size());
        module->output_variables = module->outputVariables.data();
        module->output_variable_count =
            static_cast<uint32_t>(module->outputVariables.size());

        // descriptor bindings
        module->descriptorBindings.reserve(builder.descriptorsBindings.size());
        for (const auto &descriptor : builder.descriptorsBindings) {
            auto &typeDesc = module->typeDescriptions.emplace_back();
            typeDesc.type_name = descriptor.typeName.c_str();
            typeDesc.type_flags =
                getDescriptorTypeFlags(descriptor.descriptorType);
            typeDesc.traits.numeric = descriptor.numeric;
            typeDesc.traits.image = descriptor.image;
            typeDesc.traits.array = descriptor.array;

            auto &desc = module->descriptorBindings.emplace_back();
            desc.name = descriptor.name.c_str();
            desc.descriptor_type = descriptor.descriptorType;
            desc.binding = descriptor.binding;
            desc.image = descriptor.image;
            desc.accessed = 1;
            desc.type_description = &typeDesc;

            if (descriptor.block.size) {
                desc.block.size = descriptor.block.size;
                desc.block.type_description = desc.type_description;

                const auto addBlockMembers =
                    [&module](const auto &addBlockMembers,
                        SpvReflectBlockVariable &block,
                        const BlockVariable &descBlock) -> void {
                    if (descBlock.members.empty())
                        return;
                    const auto memberOffset = module->blockMembers.size();
                    for (const auto &member : descBlock.members) {
                        auto &typeDesc =
                            module->typeDescriptions.emplace_back();
                        typeDesc.type_flags = member.typeFlags;
                        typeDesc.type_name = member.typeName.c_str();
                        typeDesc.struct_member_name = member.name.c_str();
                        typeDesc.traits.numeric = member.numeric;
                        typeDesc.traits.image = member.image;
                        typeDesc.traits.array = member.array;

                        auto &variable = module->blockMembers.emplace_back();
                        variable.name = member.name.c_str();
                        variable.type_description = &typeDesc;
                        variable.offset = member.offset;
                        variable.size = member.size;
                        variable.decoration_flags = member.decorationFlags;
                        variable.numeric = member.numeric;
                        variable.array = member.array;

                        addBlockMembers(addBlockMembers, variable, member);
                    }
                    block.members = &module->blockMembers[memberOffset];
                    block.member_count =
                        static_cast<uint32_t>(descBlock.members.size());
                    block.type_description->members =
                        block.members[0].type_description;
                    block.type_description->member_count = block.member_count;
                };
                addBlockMembers(addBlockMembers, desc.block, descriptor.block);
            }
        }
        module->descriptor_bindings = module->descriptorBindings.data();
        module->descriptor_binding_count =
            static_cast<uint32_t>(module->descriptorBindings.size());

        Q_ASSERT(module->typeDescriptions.size() == totalTypeDescriptions);
        Q_ASSERT(module->blockMembers.size() == totalBlockMembers);
        Q_ASSERT(module->inputVariables.size() + module->outputVariables.size()
            == module->interfaceVariables.size());
        return module;
    }
} // namespace

Reflection::Reflection(std::unique_ptr<Builder> builder)
    : mBuilder(std::move(builder))
    , mModule(buildSpvReflectShaderModule(*mBuilder))
{
}
