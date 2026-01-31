
#include "D3DShader.h"

namespace {
    uint32_t getTypeSize(D3D_SHADER_VARIABLE_TYPE type)
    {
        switch (type) {
        case D3D_SVT_BOOL:
        case D3D_SVT_INT:
        case D3D_SVT_FLOAT:
        case D3D_SVT_UINT:    return 4;
        case D3D_SVT_UINT8:   return 1;
        case D3D_SVT_INT16:
        case D3D_SVT_UINT16:
        case D3D_SVT_FLOAT16: return 2;
        case D3D_SVT_INT64:
        case D3D_SVT_UINT64:  return 8;
        default:              Q_ASSERT(!"not handled type");
        }
        return 0;
    }

    SpvReflectTypeFlags getTypeFlags(const D3D12_SHADER_TYPE_DESC &typeDesc)
    {
        auto typeFlags = SpvReflectTypeFlags{};
        switch (typeDesc.Class) {
        case D3D_SVC_VECTOR: typeFlags |= SPV_REFLECT_TYPE_FLAG_VECTOR; break;
        case D3D_SVC_MATRIX_ROWS:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_MATRIX;
            break;
        case D3D_SVC_MATRIX_COLUMNS:
            typeFlags |= SPV_REFLECT_TYPE_FLAG_MATRIX;
            break;
        case D3D_SVC_STRUCT: typeFlags |= SPV_REFLECT_TYPE_FLAG_STRUCT; break;
        default:             break;
        }

        switch (typeDesc.Type) {
        case D3D_SVT_VOID:   typeFlags |= SPV_REFLECT_TYPE_FLAG_VOID; break;
        case D3D_SVT_BOOL:   typeFlags |= SPV_REFLECT_TYPE_FLAG_BOOL; break;
        case D3D_SVT_INT:    typeFlags |= SPV_REFLECT_TYPE_FLAG_INT; break;
        case D3D_SVT_UINT:   typeFlags |= SPV_REFLECT_TYPE_FLAG_INT; break;
        case D3D_SVT_FLOAT:  typeFlags |= SPV_REFLECT_TYPE_FLAG_FLOAT; break;
        case D3D_SVC_STRUCT:
        default:             break;
        }

        if (typeDesc.Elements > 0)
            typeFlags |= SPV_REFLECT_TYPE_FLAG_ARRAY;

        return typeFlags;
    }

    SpvReflectNumericTraits getNumericTraits(
        const D3D12_SHADER_TYPE_DESC &typeDesc)
    {
        auto numeric = SpvReflectNumericTraits{};
        switch (typeDesc.Type) {
        case D3D_SVT_VOID:   break;
        case D3D_SVT_BOOL:
        case D3D_SVT_INT:
        case D3D_SVT_FLOAT:
        case D3D_SVT_UINT:   numeric.scalar.width = 32; break;
        case D3D_SVT_INT16:
        case D3D_SVT_UINT16: numeric.scalar.width = 16; break;
        case D3D_SVT_INT64:
        case D3D_SVT_UINT64: numeric.scalar.width = 64; break;
        default:             Q_ASSERT(!"not handled type"); break;
        }

        switch (typeDesc.Type) {
        case D3D_SVT_VOID:   break;
        case D3D_SVT_UINT:
        case D3D_SVT_UINT16:
        case D3D_SVT_UINT64: numeric.scalar.signedness = 0; break;
        default:             numeric.scalar.signedness = 1; break;
        }

        switch (typeDesc.Class) {
        case D3D_SVC_MATRIX_ROWS:
        case D3D_SVC_MATRIX_COLUMNS:
            numeric.matrix.row_count = typeDesc.Rows;
            numeric.matrix.column_count = typeDesc.Columns;
            numeric.matrix.stride = 16;
            break;

        case D3D_SVC_VECTOR:
            numeric.vector.component_count = typeDesc.Columns;
            break;

        default: break;
        }
        return numeric;
    }

    SpvReflectFormat getFormat(D3D_REGISTER_COMPONENT_TYPE componentType)
    {
        switch (componentType) {
        case D3D_REGISTER_COMPONENT_UINT32:
            return SPV_REFLECT_FORMAT_R32G32B32A32_UINT;
        case D3D_REGISTER_COMPONENT_SINT32:
            return SPV_REFLECT_FORMAT_R32G32B32A32_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT32:
            return SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT;
        default: Q_ASSERT(!"unhandled component type");
        }
        return SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT;
    }

    int getBuiltIn(D3D_NAME systemValueType)
    {
        if (systemValueType == D3D_NAME_UNDEFINED)
            return -1;
        // TODO:
        return systemValueType;
    }

    SpvReflectImageTraits getImageTraits(
        const D3D12_SHADER_INPUT_BIND_DESC &bindDesc)
    {
        enum Flag : uint32_t {
            Sampled = 1 << 0,
            Arrayed = 2 << 1,
            MS = 1 << 2,
            Depth = 1 << 3,
        };
        const auto make = [](SpvDim dim, uint32_t flags) {
            return SpvReflectImageTraits{
                .dim = dim,
                .depth = (flags & Depth ? 1u : 0),
                .arrayed = (flags & Arrayed ? 1u : 0),
                .ms = (flags & MS ? 1u : 0),
                .sampled = (flags & Sampled ? 1u : 0),
                .image_format = SpvImageFormatUnknown,
            };
        };
        // TODO:
        Q_ASSERT(bindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D);
        return make(SpvDim2D, Sampled);
    }

    // https://maraneshi.github.io/HLSL-ConstantBufferLayoutVisualizer/
    uint32_t alignUp(uint32_t offset, uint32_t align)
    {
        return (offset + (align - 1)) & ~(align - 1);
    }
} // namespace

Reflection generateSpirvReflection(Shader::ShaderType shaderType,
    ID3D12ShaderReflection *reflection)
{
    using BlockVariable = Reflection::Builder::BlockVariable;
    using InterfaceVariable = Reflection::Builder::InterfaceVariable;
    using DescriptorBinding = Reflection::Builder::DescriptorBinding;

    auto builder = std::make_unique<Reflection::Builder>();
    builder->shaderStage = getShaderStage(shaderType);

    auto shaderDesc = D3D12_SHADER_DESC{};
    reflection->GetDesc(&shaderDesc);

    const auto createInterfaceVariable =
        [](const D3D12_SIGNATURE_PARAMETER_DESC &paramDesc) {
            return InterfaceVariable{
                .semantic = (paramDesc.SemanticIndex == 0
                        ? paramDesc.SemanticName
                        : std::format("{}{}", paramDesc.SemanticName,
                              paramDesc.SemanticIndex)),
                .builtIn = getBuiltIn(paramDesc.SystemValueType),
                .format = getFormat(paramDesc.ComponentType),
                .location = paramDesc.Register,
            };
        };

    for (auto i = 0u; i < shaderDesc.InputParameters; ++i) {
        auto paramDesc = D3D12_SIGNATURE_PARAMETER_DESC{};
        reflection->GetInputParameterDesc(i, &paramDesc);
        builder->inputs.push_back(createInterfaceVariable(paramDesc));
    }

    for (auto i = 0u; i < shaderDesc.OutputParameters; ++i) {
        auto paramDesc = D3D12_SIGNATURE_PARAMETER_DESC{};
        reflection->GetOutputParameterDesc(i, &paramDesc);
        builder->outputs.push_back(createInterfaceVariable(paramDesc));
    }

    for (auto i = 0u; i < shaderDesc.BoundResources; ++i) {
        auto bindDesc = D3D12_SHADER_INPUT_BIND_DESC{};
        reflection->GetResourceBindingDesc(i, &bindDesc);
        switch (bindDesc.Type) {
        case D3D_SIT_CBUFFER: {
            auto cbuffer = reflection->GetConstantBufferByName(bindDesc.Name);
            auto cbufferDesc = D3D12_SHADER_BUFFER_DESC{};
            cbuffer->GetDesc(&cbufferDesc);

            auto variables = std::vector<BlockVariable>();
            for (auto j = 0u; j < cbufferDesc.Variables; ++j) {
                auto var = cbuffer->GetVariableByIndex(j);
                auto varDesc = D3D12_SHADER_VARIABLE_DESC{};
                var->GetDesc(&varDesc);

                auto varType = var->GetType();
                auto varTypeDesc = D3D12_SHADER_TYPE_DESC{};
                varType->GetDesc(&varTypeDesc);

                const auto createTypeMembers =
                    [](const auto &createTypeMembers,
                        ID3D12ShaderReflectionType *type)
                    -> std::vector<BlockVariable> {
                    auto typeDesc = D3D12_SHADER_TYPE_DESC{};
                    type->GetDesc(&typeDesc);

                    auto members = std::vector<BlockVariable>();
                    for (auto i = 0u; i < typeDesc.Members; ++i) {
                        auto memberType = type->GetMemberTypeByIndex(i);
                        auto memberTypeDesc = D3D12_SHADER_TYPE_DESC{};
                        memberType->GetDesc(&memberTypeDesc);

                        auto subMembers =
                            createTypeMembers(createTypeMembers, memberType);
                        const auto elementSize = (!members.empty()
                                ? members.back().offset + members.back().size
                                : getTypeSize(memberTypeDesc.Type));
                        const auto arrayStride = (memberTypeDesc.Elements
                                ? alignUp(elementSize, 16)
                                : 0);

                        auto size = elementSize;
                        if (memberTypeDesc.Elements > 1)
                            size += arrayStride * (memberTypeDesc.Elements - 1);

                        auto& member = members.emplace_back(BlockVariable{
                            .name = "_" + std::to_string(i),
                            .offset = memberTypeDesc.Offset,
                            .size = size,
                            .typeName = memberTypeDesc.Name,
                            .typeFlags = getTypeFlags(memberTypeDesc),
                            .numeric = getNumericTraits(memberTypeDesc),
                            .array = {
                                .dims_count = (memberTypeDesc.Elements ? 1u : 0u),
                                .dims = { static_cast<uint32_t>(memberTypeDesc.Elements) },
                                .stride = arrayStride,
                            },
                            .members = std::move(subMembers),
                        });
                    }
                    return members;
                };

                auto members = createTypeMembers(createTypeMembers, varType);
                const auto elementSize = (!members.empty()
                        ? members.back().offset + members.back().size
                        : getTypeSize(varTypeDesc.Type));

                const auto isUserType = (varTypeDesc.Type == D3D_SVT_VOID);
                auto& variable = variables.emplace_back(BlockVariable{
                    .name = varDesc.Name,
                    .offset = varDesc.StartOffset,
                    .size = varDesc.Size,
                    .typeName = (isUserType ? varTypeDesc.Name : ""),
                    .typeFlags = getTypeFlags(varTypeDesc),
                    .numeric = getNumericTraits(varTypeDesc),
                    .array = {
                        .dims_count = (varTypeDesc.Elements ? 1u : 0u),
                        .dims = { static_cast<uint32_t>(varTypeDesc.Elements) },
                        .stride = (varTypeDesc.Elements ? alignUp(elementSize, 16) : 0),
                    },
                    .members = std::move(members)
                });
            }

            // ConstantBuffer
            if (variables.size() == 1 && !variables.front().members.empty()) {
                auto first = std::move(variables.front());
                variables = std::move(first.members);
            }

            builder->descriptorsBindings.push_back(DescriptorBinding{
                .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      .name = cbufferDesc.Name,
                      .typeName = cbufferDesc.Name,
                      .block = {
                          .size = cbufferDesc.Size,
                          .members = std::move(variables),
                      },
                      .binding = bindDesc.BindPoint,
                      .set = bindDesc.Space,
                  });
        } break;

        case D3D_SIT_UAV_RWBYTEADDRESS:
            builder->descriptorsBindings.push_back(DescriptorBinding{
                .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .name = bindDesc.Name,
                .typeName = bindDesc.Name,
                .binding = bindDesc.BindPoint,
                .set = bindDesc.Space,
            });
            break;

        case D3D_SIT_SAMPLER: {
            builder->descriptorsBindings.push_back(DescriptorBinding{
                .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER,
                .name = bindDesc.Name,
                .binding = bindDesc.BindPoint,
                .set = bindDesc.Space,
            });
        } break;

        case D3D_SIT_TEXTURE: {
            builder->descriptorsBindings.push_back(DescriptorBinding{
                .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .name = bindDesc.Name,
                .image = getImageTraits(bindDesc),
                .binding = bindDesc.BindPoint,
                .set = bindDesc.Space,
            });
        } break;

        default: Q_ASSERT(!"unhandled type");
        }
    }
    return Reflection(std::move(builder));
}
