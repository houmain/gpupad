
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

    uint32_t getTypeSize(D3D12_SHADER_TYPE_DESC typeDesc)
    {
        return getTypeSize(typeDesc.Type) * typeDesc.Rows * typeDesc.Columns;
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
        case D3D_SVT_VOID:    break;
        case D3D_SVT_INT16:
        case D3D_SVT_UINT16:
        case D3D_SVT_FLOAT16: numeric.scalar.width = 16; break;
        case D3D_SVT_BOOL:
        case D3D_SVT_INT:
        case D3D_SVT_FLOAT:
        case D3D_SVT_UINT:    numeric.scalar.width = 32; break;
        case D3D_SVT_INT64:
        case D3D_SVT_UINT64:
        case D3D_SVT_DOUBLE:  numeric.scalar.width = 64; break;
        default:              Q_ASSERT(!"not handled type"); break;
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

    SpvReflectFormat getFormat(D3D_REGISTER_COMPONENT_TYPE componentType,
        int componentCount)
    {
        constexpr auto mask = [](int count, D3D_REGISTER_COMPONENT_TYPE type) {
            return static_cast<int>(type) * 1000 + count;
        };
        switch (mask(componentCount, componentType)) {
#define ADD(COUNT, TYPE, FORMAT)                     \
    case mask(COUNT, D3D_REGISTER_COMPONENT_##TYPE): \
        return SPV_REFLECT_FORMAT_##FORMAT;
            ADD(1, UINT16, R16_UINT)
            ADD(1, SINT16, R16_SINT)
            ADD(1, FLOAT16, R16_SFLOAT)
            ADD(2, UINT16, R16G16_UINT)
            ADD(2, SINT16, R16G16_SINT)
            ADD(2, FLOAT16, R16G16_SFLOAT)
            ADD(3, UINT16, R16G16B16_UINT)
            ADD(3, SINT16, R16G16B16_SINT)
            ADD(3, FLOAT16, R16G16B16_SFLOAT)
            ADD(4, UINT16, R16G16B16A16_UINT)
            ADD(4, SINT16, R16G16B16A16_SINT)
            ADD(4, FLOAT16, R16G16B16A16_SFLOAT)
            ADD(1, UINT32, R32_UINT)
            ADD(1, SINT32, R32_SINT)
            ADD(1, FLOAT32, R32_SFLOAT)
            ADD(2, UINT32, R32G32_UINT)
            ADD(2, SINT32, R32G32_SINT)
            ADD(2, FLOAT32, R32G32_SFLOAT)
            ADD(3, UINT32, R32G32B32_UINT)
            ADD(3, SINT32, R32G32B32_SINT)
            ADD(3, FLOAT32, R32G32B32_SFLOAT)
            ADD(4, UINT32, R32G32B32A32_UINT)
            ADD(4, SINT32, R32G32B32A32_SINT)
            ADD(4, FLOAT32, R32G32B32A32_SFLOAT)
            ADD(1, UINT64, R64_UINT)
            ADD(1, SINT64, R64_SINT)
            ADD(1, FLOAT64, R64_SFLOAT)
            ADD(2, UINT64, R64G64_UINT)
            ADD(2, SINT64, R64G64_SINT)
            ADD(2, FLOAT64, R64G64_SFLOAT)
            ADD(3, UINT64, R64G64B64_UINT)
            ADD(3, SINT64, R64G64B64_SINT)
            ADD(3, FLOAT64, R64G64B64_SFLOAT)
            ADD(4, UINT64, R64G64B64A64_UINT)
            ADD(4, SINT64, R64G64B64A64_SINT)
            ADD(4, FLOAT64, R64G64B64A64_SFLOAT)
#undef ADD
        }
        return SPV_REFLECT_FORMAT_UNDEFINED;
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
} // namespace

Reflection generateSpirvReflection(Shader::ShaderType shaderType,
    const Reflection &spirvReflection, ID3D12ShaderReflection *reflection)
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
                .format = getFormat(paramDesc.ComponentType,
                    std::popcount(paramDesc.Mask)),
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

                        // https://maraneshi.github.io/HLSL-ConstantBufferLayoutVisualizer/
                        auto subMembers =
                            createTypeMembers(createTypeMembers, memberType);
                        const auto elementSize = (!subMembers.empty()
                                ? subMembers.back().offset
                                    + subMembers.back().size
                                : getTypeSize(memberTypeDesc));
                        const auto arrayStride = (memberTypeDesc.Elements
                                ? alignUp(elementSize, 16)
                                : 0);

                        auto size = elementSize;
                        if (memberTypeDesc.Elements > 1)
                            size += arrayStride * (memberTypeDesc.Elements - 1);

                        auto& member = members.emplace_back(BlockVariable{
                            .name = "m" + std::to_string(i),
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
            auto array = SpvReflectArrayTraits{};
            if (variables.size() == 1 && !variables.front().members.empty()) {
                auto first = std::move(variables.front());
                array = first.array;
                variables = std::move(first.members);
            }

            // fixup member names using SPIR-V reflection
            const auto fixupMemberNames =
                [](const auto &fixupMemberNames, BlockVariable &variable,
                    const SpvReflectTypeDescription &spirvType) -> void {
                variable.name = spirvType.struct_member_name;
                for (auto i = 0u; i < variable.members.size(); ++i)
                    if (i < spirvType.member_count)
                        fixupMemberNames(fixupMemberNames, variable.members[i],
                            spirvType.members[i]);
            };
            for (const auto &binding : spirvReflection.descriptorBindings())
                if (!std::strcmp(binding.name, bindDesc.Name))
                    for (auto i = 0u; i < variables.size(); ++i)
                        if (i < binding.block.member_count)
                            fixupMemberNames(fixupMemberNames, variables[i],
                                *binding.block.members[i].type_description);

            builder->descriptorsBindings.push_back(DescriptorBinding{
                .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      .name = cbufferDesc.Name,
                      .typeName = cbufferDesc.Name,
                      .array = array,
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
