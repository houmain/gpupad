
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
    return (name == "$Globals" || name == "$Global" || name == "_Global");
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
    case Includable: break;
    case Vertex:     return SPV_REFLECT_SHADER_STAGE_VERTEX_BIT;
    case Fragment:   return SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT;
    case Compute:    return SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT;
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

GLenum getBufferMemberGLType(const SpvReflectBlockVariable &variable)
{
    using DT = Field::DataType;
    const auto columns = getBufferMemberColumnCount(variable);
    const auto rows = getBufferMemberRowCount(variable);
    const auto dataType = getBufferMemberDataType(variable);
    constexpr auto hash = [](int columns, int rows, Field::DataType type) {
        return columns * 10000 + rows * 100 + static_cast<int>(type);
    };
    switch (hash(columns, rows, dataType)) {
    case hash(1, 1, DT::Float):  return GL_FLOAT;
    case hash(1, 2, DT::Float):  return GL_FLOAT_VEC2;
    case hash(1, 3, DT::Float):  return GL_FLOAT_VEC3;
    case hash(1, 4, DT::Float):  return GL_FLOAT_VEC4;
    case hash(1, 1, DT::Double): return GL_DOUBLE;
    case hash(1, 2, DT::Double): return GL_DOUBLE_VEC2;
    case hash(1, 3, DT::Double): return GL_DOUBLE_VEC3;
    case hash(1, 4, DT::Double): return GL_DOUBLE_VEC4;
    case hash(1, 1, DT::Int32):  return GL_INT;
    case hash(1, 2, DT::Int32):  return GL_INT_VEC2;
    case hash(1, 3, DT::Int32):  return GL_INT_VEC3;
    case hash(1, 4, DT::Int32):  return GL_INT_VEC4;
    case hash(1, 1, DT::Uint32): return GL_UNSIGNED_INT;
    case hash(1, 2, DT::Uint32): return GL_UNSIGNED_INT_VEC2;
    case hash(1, 3, DT::Uint32): return GL_UNSIGNED_INT_VEC3;
    case hash(1, 4, DT::Uint32): return GL_UNSIGNED_INT_VEC4;
    //case hash(1, 1, DT::Bool): return GL_BOOL;
    //case hash(1, 2, DT::Bool): return GL_BOOL_VEC2;
    //case hash(1, 3, DT::Bool): return GL_BOOL_VEC3;
    //case hash(1, 4, DT::Bool): return GL_BOOL_VEC4;
    case hash(2, 2, DT::Float):  return GL_FLOAT_MAT2;
    case hash(3, 3, DT::Float):  return GL_FLOAT_MAT3;
    case hash(4, 4, DT::Float):  return GL_FLOAT_MAT4;
    case hash(2, 3, DT::Float):  return GL_FLOAT_MAT2x3;
    case hash(2, 4, DT::Float):  return GL_FLOAT_MAT2x4;
    case hash(3, 2, DT::Float):  return GL_FLOAT_MAT3x2;
    case hash(3, 4, DT::Float):  return GL_FLOAT_MAT3x4;
    case hash(4, 2, DT::Float):  return GL_FLOAT_MAT4x2;
    case hash(4, 3, DT::Float):  return GL_FLOAT_MAT4x3;
    case hash(2, 2, DT::Double): return GL_DOUBLE_MAT2;
    case hash(3, 3, DT::Double): return GL_DOUBLE_MAT3;
    case hash(4, 4, DT::Double): return GL_DOUBLE_MAT4;
    case hash(2, 3, DT::Double): return GL_DOUBLE_MAT2x3;
    case hash(2, 4, DT::Double): return GL_DOUBLE_MAT2x4;
    case hash(3, 2, DT::Double): return GL_DOUBLE_MAT3x2;
    case hash(3, 4, DT::Double): return GL_DOUBLE_MAT3x4;
    case hash(4, 2, DT::Double): return GL_DOUBLE_MAT4x2;
    case hash(4, 3, DT::Double): return GL_DOUBLE_MAT4x3;
    }
    return GL_NONE;
}
