
#include "Spirv.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {

    struct TypeDesc
    {
        const char *prefix;
        const char *single;
        const char *dims;
    };

    const char *getStageName(SpvReflectShaderStageFlagBits stage)
    {
        switch (stage) {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: return "Vertex";
        case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return "TessControl";
        case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return "TessEvaluation";
        case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:    return "Geometry";
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:    return "Fragment";
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:     return "Compute";
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:    return "Task";
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:    return "Mesh";
        case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:  return "RayGeneration";
        case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR: return "RayAnyHit";
        case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            return "RayClosestHit";
        case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR: return "RayMiss";
        case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
            return "RayIntersection";
        case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR: return "RayCallable";
        }
        return nullptr;
    }

    const char *getImageDims(const SpvReflectTypeDescription &type)
    {
        if (type.traits.image.ms) {
            if (type.traits.image.arrayed) {
                switch (type.traits.image.dim) {
                case SpvDim2D: return "2DMSArray";
                default:       break;
                }
            } else {
                switch (type.traits.image.dim) {
                case SpvDim2D: return "2DMS";
                default:       break;
                }
            }
        }
        if (type.traits.image.arrayed) {
            switch (type.traits.image.dim) {
            case SpvDim1D:   return "1DArray";
            case SpvDim2D:   return "2DArray";
            case SpvDimCube: return "CubeArray";
            default:         break;
            }
        } else {
            switch (type.traits.image.dim) {
            case SpvDim1D:     return "1D";
            case SpvDim2D:     return "2D";
            case SpvDim3D:     return "3D";
            case SpvDimCube:   return "Cube";
            case SpvDimRect:   return "Rect";
            case SpvDimBuffer: return "Buffer";
            default:           break;
            }
        }
        Q_ASSERT(!"unhandled dimensions");
        return "";
    }

    const char *getImageFormat(const SpvReflectTypeDescription &type)
    {
        switch (type.traits.image.image_format) {
        case SpvImageFormatUnknown:
        case SpvImageFormatMax:          break;
        case SpvImageFormatRgba32f:      return "rgba32f";
        case SpvImageFormatRgba16f:      return "rgba16f";
        case SpvImageFormatR32f:         return "r32f";
        case SpvImageFormatRgba8:        return "rgba8";
        case SpvImageFormatRgba8Snorm:   return "rgba8snorm";
        case SpvImageFormatRg32f:        return "rg32f";
        case SpvImageFormatRg16f:        return "rg16f";
        case SpvImageFormatR11fG11fB10f: return "r11fg11fb10f";
        case SpvImageFormatR16f:         return "r16f";
        case SpvImageFormatRgba16:       return "rgba16";
        case SpvImageFormatRgb10A2:      return "rgb10a2";
        case SpvImageFormatRg16:         return "rg16";
        case SpvImageFormatRg8:          return "rg8";
        case SpvImageFormatR16:          return "r16";
        case SpvImageFormatR8:           return "r8";
        case SpvImageFormatRgba16Snorm:  return "rgba16snorm";
        case SpvImageFormatRg16Snorm:    return "rg16snorm";
        case SpvImageFormatRg8Snorm:     return "rg8snorm";
        case SpvImageFormatR16Snorm:     return "r16snorm";
        case SpvImageFormatR8Snorm:      return "r8snorm";
        case SpvImageFormatRgba32i:      return "rgba32i";
        case SpvImageFormatRgba16i:      return "rgba16i";
        case SpvImageFormatRgba8i:       return "rgba8i";
        case SpvImageFormatR32i:         return "r32i";
        case SpvImageFormatRg32i:        return "rg32i";
        case SpvImageFormatRg16i:        return "rg16i";
        case SpvImageFormatRg8i:         return "rg8i";
        case SpvImageFormatR16i:         return "r16i";
        case SpvImageFormatR8i:          return "r8i";
        case SpvImageFormatRgba32ui:     return "rgba32ui";
        case SpvImageFormatRgba16ui:     return "rgba16ui";
        case SpvImageFormatRgba8ui:      return "rgba8ui";
        case SpvImageFormatR32ui:        return "r32ui";
        case SpvImageFormatRgb10a2ui:    return "rgb10a2ui";
        case SpvImageFormatRg32ui:       return "rg32ui";
        case SpvImageFormatRg16ui:       return "rg16ui";
        case SpvImageFormatRg8ui:        return "rg8ui";
        case SpvImageFormatR16ui:        return "r16ui";
        case SpvImageFormatR8ui:         return "r8ui";
        case SpvImageFormatR64ui:        return "r64ui";
        case SpvImageFormatR64i:         return "r64i";
        }
        return nullptr;
    }

    const char *getVectorMatrixDims(const SpvReflectTypeDescription &type)
    {
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
            switch (type.traits.numeric.matrix.column_count) {
            case 2:
                switch (type.traits.numeric.matrix.row_count) {
                case 2: return "2";
                case 3: return "2x3";
                case 4: return "2x4";
                }
            case 3:
                switch (type.traits.numeric.matrix.row_count) {
                case 2: return "3x2";
                case 3: return "3";
                case 4: return "3x4";
                }
            case 4:
                switch (type.traits.numeric.matrix.row_count) {
                case 2: return "4x2";
                case 3: return "4x3";
                case 4: return "4";
                }
            }
        }
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
            switch (type.traits.numeric.vector.component_count) {
            case 1: return "1";
            case 2: return "2";
            case 3: return "3";
            case 4: return "4";
            }
        return "";
    }

    TypeDesc getTypeDesc(const SpvReflectTypeDescription &type)
    {
        if (type.type_flags
            & (SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE
                | SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE
                | SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER)) {
            const auto dims = getImageDims(type);
            if (type.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
                return { "", "", dims };
            if (type.traits.numeric.scalar.signedness == 0)
                return { "u", "", dims };
            return { "i", "", dims };
        }

        const auto dims = getVectorMatrixDims(type);
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_BOOL)
            return { "b", "bool", dims };

        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_INT) {
            if (type.traits.numeric.scalar.signedness == 0)
                return { "u", "uint", dims };
            return { "i", "int", dims };
        }
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
            if (type.traits.numeric.scalar.width == 64)
                return { "d", "double", dims };
            return { "", "float", dims };
        }
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK)
            return { "", "", "" };

        Q_ASSERT(!"unhandled type");
        return { "", "", "" };
    }

    QString getTypeName(const SpvReflectTypeDescription &type)
    {
        const auto [prefix, single, dims] = getTypeDesc(type);
        auto kind = "";
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE) {
            kind = "sampler";
        } else if (type.type_flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE) {
            kind = "image";
        } else if (type.type_flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER) {
            kind = "sampler";
        } else if (type.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
            kind = "mat";
        } else if (type.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) {
            kind = "vec";
        } else {
            return single;
        }
        return QStringLiteral("%1%2%3").arg(prefix).arg(kind).arg(dims);
    }

    QJsonObject getJson(const SpvReflectTypeDescription &type);

    QJsonArray getTypeMembers(const SpvReflectTypeDescription &type)
    {
        auto json = QJsonArray();
        for (auto i = 0u; i < type.member_count; ++i) {
            const auto &member = type.members[i];
            auto jsonMember = getJson(member);
            jsonMember["name"] = member.struct_member_name;
            json.append(jsonMember);
        }
        return json;
    }

    QJsonObject getJson(const SpvReflectTypeDescription &type)
    {
        auto json = QJsonObject();
        if (!type.member_count) {
            if (auto typeName = getTypeName(type); !typeName.isEmpty())
                json["type"] = typeName;
            if (auto imageFormat = getImageFormat(type))
                json["imageFormat"] = imageFormat;
        } else {
            json["type"] = type.type_name;
            json["members"] = getTypeMembers(type);
        }

        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY) {
            auto array = QJsonArray();
            for (auto i = 0u; i < type.traits.array.dims_count; ++i)
                array.append(static_cast<int>(type.traits.array.dims[i]));
            json["array"] = array;
        }
        return json;
    }

    bool isBuiltIn(const SpvReflectInterfaceVariable &variable)
    {
        return (variable.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
            != 0;
    }

    QJsonObject getJson(const SpvReflectInterfaceVariable &variable)
    {
        auto json = getJson(*variable.type_description);
        json["name"] = variable.name;
        if (auto location = static_cast<int>(variable.location); location >= 0)
            json["location"] = location;
        return json;
    }

    QJsonObject getJson(const SpvReflectDescriptorBinding &binding)
    {
        auto json = getJson(*binding.type_description);
        if (*binding.name)
            json["name"] = binding.name;
        json["accessed"] = static_cast<bool>(binding.accessed);
        json["binding"] = static_cast<int>(binding.binding);
        json["set"] = static_cast<int>(binding.set);
        return json;
    }

    QJsonObject getJson(const SpvReflectShaderModule &module)
    {
        auto json = QJsonObject();
        json["stage"] = getStageName(module.shader_stage);

        auto jsonInputs = QJsonArray();
        for (auto i = 0u; i < module.input_variable_count; ++i)
            if (const auto input = module.input_variables[i])
                if (!isBuiltIn(*input))
                    jsonInputs.append(getJson(*input));
        json["inputs"] = jsonInputs;

        auto jsonOutputs = QJsonArray();
        for (auto i = 0u; i < module.output_variable_count; ++i)
            if (const auto output = module.output_variables[i])
                if (!isBuiltIn(*output))
                    jsonOutputs.append(getJson(*output));
        json["outputs"] = jsonOutputs;

        auto jsonUniforms = QJsonArray();
        auto jsonBuffers = QJsonArray();
        auto jsonAccelerationStructures = QJsonArray();
        for (auto i = 0u; i < module.descriptor_binding_count; ++i)
            if (const auto &binding = module.descriptor_bindings[i]; true) {
                const auto &type = *binding.type_description;
                switch (binding.descriptor_type) {
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    jsonUniforms.push_back(getJson(binding));
                    break;

                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    if (isGlobalUniformBlockName(type.type_name)) {
                        for (const auto &member : getTypeMembers(type))
                            jsonUniforms.push_back(member);
                    } else {
                        jsonBuffers.push_back(getJson(binding));
                    }
                    break;

                case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    jsonAccelerationStructures.push_back(getJson(binding));
                    break;

                default: Q_ASSERT(!"not handled descriptor type");
                }
            }
        json["uniforms"] = jsonUniforms;
        json["buffers"] = jsonBuffers;
        json["accelerationStructures"] = jsonAccelerationStructures;
        return json;
    }
} // namespace

Spirv::Interface::Interface(const std::vector<uint32_t> &spirv)
{
    auto module = SpvReflectShaderModule{};
    const auto result = spvReflectCreateShaderModule(
        spirv.size() * sizeof(uint32_t), spirv.data(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
        module = {};
    mModule = std::shared_ptr<SpvReflectShaderModule>(
        new SpvReflectShaderModule(module), spvReflectDestroyShaderModule);
}

Spirv::Interface::~Interface() = default;

Spirv::Interface::operator bool() const
{
    return static_cast<bool>(mModule);
}

const SpvReflectShaderModule &Spirv::Interface::operator*() const
{
    Q_ASSERT(mModule);
    return *mModule;
}

const SpvReflectShaderModule *Spirv::Interface::operator->() const
{
    Q_ASSERT(mModule);
    return mModule.get();
}

//-------------------------------------------------------------------------

QString getJsonString(const SpvReflectShaderModule &module)
{
    return QJsonDocument(getJson(module)).toJson();
}

uint32_t getBindingArraySize(const SpvReflectBindingArrayTraits &array)
{
    auto count = 1u;
    for (auto i = 0u; i < array.dims_count; ++i)
        count *= array.dims[i];
    return count;
}

Field::DataType getBufferMemberDataType(const SpvReflectBlockVariable &variable)
{
    const auto &type_desc = *variable.type_description;
    if (type_desc.traits.numeric.scalar.width == 32) {
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
            return Field::DataType::Float;
        return (type_desc.traits.numeric.scalar.signedness
                ? Field::DataType::Int32
                : Field::DataType::Uint32);
    } else if (type_desc.traits.numeric.scalar.width == 64) {
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
            return Field::DataType::Double;
    }
    Q_ASSERT(!"variable type not handled");
    return Field::DataType::Float;
}

int getBufferMemberColumnCount(const SpvReflectBlockVariable &variable)
{
    const auto &type_desc = *variable.type_description;
    if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
        return variable.numeric.matrix.column_count;
    return 1;
}

int getBufferMemberRowCount(const SpvReflectBlockVariable &variable)
{
    const auto &type_desc = *variable.type_description;
    if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
        return variable.numeric.matrix.row_count;
    if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
        return variable.numeric.vector.component_count;
    return 1;
}

int getBufferMemberColumnStride(const SpvReflectBlockVariable &variable)
{
    const auto &type_desc = *variable.type_description;
    if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
        return variable.numeric.matrix.stride;
    return 0;
}

int getBufferMemberArraySize(const SpvReflectBlockVariable &variable)
{
    const auto &type_desc = *variable.type_description;
    auto count = 1u;
    if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
        for (auto i = 0u; i < variable.array.dims_count; ++i)
            count *= variable.array.dims[i];
    return static_cast<int>(count);
}

int getBufferMemberArrayStride(const SpvReflectBlockVariable &variable)
{
    const auto &type_desc = *variable.type_description;
    if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
        return variable.array.stride;
    return 0;
}
