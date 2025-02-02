
#include "Spirv.h"

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
