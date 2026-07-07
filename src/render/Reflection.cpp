
#include "Reflection.h"

Reflection::Reflection(const std::vector<uint32_t> &spirv)
{
    auto module = std::make_unique<SpvReflectShaderModule>();
    if (!spirv.empty()) {
        const auto result = spvReflectCreateShaderModule(
            spirv.size() * sizeof(uint32_t), spirv.data(), module.get());
        if (result != SPV_REFLECT_RESULT_SUCCESS)
            *module = {};
    }

    mModule = std::shared_ptr<SpvReflectShaderModule>(module.release(),
        [](SpvReflectShaderModule *module) {
            if (module)
                spvReflectDestroyShaderModule(module);
            delete module;
        });
}

Reflection::~Reflection() = default;

Reflection::operator bool() const
{
    return static_cast<bool>(mModule);
}

const SpvReflectShaderModule &Reflection::operator*() const
{
    Q_ASSERT(mModule);
    return *mModule;
}

const SpvReflectShaderModule *Reflection::operator->() const
{
    Q_ASSERT(mModule);
    return mModule.get();
}

//-------------------------------------------------------------------------

bool isGlobalUniformBlockName(const char *name_)
{
    if (!name_)
        return false;
    const auto name = std::string_view(name_);
    return (name == "$Globals" || name == "$Global" || name == "_Global"
        || name == "type.$Globals");
}

bool isGlobalUniformBlockName(const QString &name)
{
    return isGlobalUniformBlockName(qUtf8Printable(name));
}

QString removeGlobalUniformBlockName(QString string)
{
    if (string.startsWith(globalUniformBlockName))
        return string.mid(sizeof(globalUniformBlockName));
    return string;
}

bool isBufferBinding(SpvReflectDescriptorType type)
{
    return (type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        || type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

SpvReflectShaderStageFlagBits getShaderStage(Shader::ShaderType shaderType)
{
    using enum Shader::ShaderType;
    switch (shaderType) {
    case Includable:  break;
    case Vertex:      return SPV_REFLECT_SHADER_STAGE_VERTEX_BIT;
    case Fragment:    return SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT;
    case Geometry:    return SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT;
    case TessControl: return SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case TessEvaluation:
        return SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case Compute:         return SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT;
    case Task:            return SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT;
    case Mesh:            return SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT;
    case RayGeneration:   return SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR;
    case RayIntersection: return SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case RayAnyHit:       return SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case RayClosestHit:   return SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case RayMiss:         return SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR;
    case RayCallable:     return SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR;
    }
    return {};
}

bool isBuiltIn(const SpvReflectInterfaceVariable &variable)
{
    return (variable.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) != 0;
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
