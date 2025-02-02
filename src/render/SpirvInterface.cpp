
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

    const char *getImageDims(const SpvReflectTypeDescription &type)
    {
        if (type.traits.image.ms) {
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
            json["type"] = getTypeName(type);
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
        return json;
    }

    QJsonObject getJson(const SpvReflectDescriptorBinding &binding)
    {
        auto json = getJson(*binding.type_description);
        if (*binding.name)
            json["name"] = binding.name;
        json["accessed"] = static_cast<bool>(binding.accessed);
        return json;
    }

    QJsonObject getJson(const SpvReflectShaderModule &module)
    {
        auto json = QJsonObject();

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
                default: Q_ASSERT(!"not handled descriptor type");
                }
            }
        json["uniforms"] = jsonUniforms;
        json["buffers"] = jsonBuffers;
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
    Q_ASSERT(count > 0);
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
