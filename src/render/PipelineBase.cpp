
#include "PipelineBase.h"
#include "BufferBase.h"

namespace {
    QString getBufferMemberFullName(const SpvReflectBlockVariable &block,
        uint32_t arrayElement, const SpvReflectBlockVariable &member)
    {
        const auto &type_desc = *block.type_description;
        if (isGlobalUniformBlockName(type_desc.type_name))
            return member.name;

        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
            return QStringLiteral("%1[%2].%3")
                .arg(type_desc.type_name)
                .arg(arrayElement)
                .arg(member.name);

        return QStringLiteral("%1.%2").arg(type_desc.type_name, member.name);
    }

    QStringView getBaseName(QStringView name)
    {
        if (!name.endsWith(']'))
            return name;
        if (auto bracket = name.lastIndexOf('['))
            return getBaseName(name.left(bracket));
        return {};
    }

    std::vector<int> getArrayIndices(QStringView name)
    {
        auto result = std::vector<int>();
        while (name.endsWith(']')) {
            const auto pos = name.lastIndexOf('[');
            if (pos < 0)
                break;
            const auto index = name.mid(pos + 1, name.size() - pos - 2).toInt();
            result.insert(result.begin(), index);
            name = name.left(pos);
        }
        return result;
    }

    std::span<const uint32_t> getBufferMemberArrayDims(
        const SpvReflectBlockVariable &variable)
    {
        const auto &type_desc = *variable.type_description;
        if (type_desc.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
            return { variable.array.dims, variable.array.dims_count };
        return {};
    }

    template <typename T>
    void copyBufferMember(const SpvReflectBlockVariable &member,
        std::byte *dest, int elementOffset, int elementCount, const T *source)
    {
        const auto arraySize = getBufferMemberArraySize(member);
        const auto columns = getBufferMemberColumnCount(member);
        const auto rows = getBufferMemberRowCount(member);
        const auto arrayStride = getBufferMemberArrayStride(member);
        const auto columnStride = getBufferMemberColumnStride(member);
        const auto elementSize = sizeof(T) * rows;

        for (auto a = 0; a < arraySize; ++a) {
            const auto arrayBegin = dest;
            if (elementOffset-- <= 0 && elementCount-- > 0) {
                for (auto c = 0; c < columns; ++c) {
                    std::memcpy(dest, source, elementSize);
                    source += rows;
                    dest += (columnStride ? columnStride : elementSize);
                }
            }
            if (arrayStride)
                dest = arrayBegin + arrayStride;
        }
    }

    template <typename F>
    void forEachBufferMemberRec(const QString &name,
        const SpvReflectBlockVariable &member, int memberOffset,
        const F &function)
    {
        memberOffset += member.offset;

        if (!member.member_count)
            return function(name, member, memberOffset);

        if (auto size = getBufferMemberArraySize(member)) {
            const auto arrayStride = getBufferMemberArrayStride(member);
            for (auto a = 0; a < size; ++a) {
                const auto element = QString("[%1]").arg(a);
                for (auto i = 0u; i < member.member_count; ++i)
                    forEachBufferMemberRec(
                        name + element + '.' + member.members[i].name,
                        member.members[i], memberOffset + a * arrayStride,
                        function);
            }
        } else {
            for (auto i = 0u; i < member.member_count; ++i)
                forEachBufferMemberRec(name + '.' + member.members[i].name,
                    member.members[i], memberOffset, function);
        }
    }
} // namespace

PipelineBase::PipelineBase(ItemId itemId) : mItemId(itemId) { }

void PipelineBase::setBindings(Bindings &&bindings)
{
    mBindings = std::move(bindings);
}

std::pair<uint32_t, uint32_t> PipelineBase::getBufferBindingOffsetSize(
    const BufferBinding &binding, ScriptEngine &scriptEngine) const
{
    const auto &buffer = *binding.buffer;
    const auto offset =
        scriptEngine.evaluateUInt(binding.offset, buffer.itemId());
    const auto rowCount =
        scriptEngine.evaluateUInt(binding.rowCount, buffer.itemId());
    const auto size =
        (binding.stride ? rowCount * binding.stride : buffer.size() - offset);
    Q_ASSERT(size >= 0 && offset + size <= static_cast<size_t>(buffer.size()));
    return std::pair(offset, size);
};

void PipelineBase::applyBufferMemberBinding(std::span<std::byte> bufferData,
    const SpvReflectBlockVariable &member, const UniformBinding &binding,
    int memberOffset, int elementOffset, int elementCount,
    ScriptEngine &scriptEngine)
{
    const auto type = getBufferMemberDataType(member);
    const auto memberData = &bufferData[memberOffset];
    const auto valuesPerElement = getBufferMemberColumnCount(member)
        * getBufferMemberRowCount(member);
    switch (type) {
#define ADD(DATATYPE, TYPE)                                                  \
    case DATATYPE: {                                                         \
        const auto values = getValues<TYPE>(scriptEngine, binding.values, 0, \
            elementCount * valuesPerElement, binding.bindingItemId);         \
        copyBufferMember<TYPE>(member, memberData, elementOffset,            \
            elementCount, values.data());                                    \
        break;                                                               \
    }
        ADD(Field::DataType::Int32, int32_t);
        ADD(Field::DataType::Uint32, uint32_t);
        ADD(Field::DataType::Int64, int64_t);
        ADD(Field::DataType::Uint64, uint64_t);
        ADD(Field::DataType::Float, float);
        ADD(Field::DataType::Double, double);
#undef ADD
    default: Q_ASSERT(!"not handled data type");
    }
}

bool PipelineBase::applyBufferMemberBindings(std::span<std::byte> bufferData,
    const QString &name, const SpvReflectBlockVariable &member,
    int memberOffset, ScriptEngine &scriptEngine)
{
    auto bindingSet = false;
    for (const auto &[bindingName, binding] : mBindings.uniforms) {
        if (bindingName == name) {
            applyBufferMemberBinding(bufferData, member, binding, memberOffset,
                0, getBufferMemberArraySize(member), scriptEngine);
            mUsedItems += binding.bindingItemId;
            bindingSet = true;
            continue;
        }

        // compare array elements also by basename
        const auto baseName = getBaseName(bindingName);
        if (baseName == name) {
            const auto indices = getArrayIndices(bindingName);
            Q_ASSERT(!indices.empty());
            const auto dims = getBufferMemberArrayDims(member);
            if (indices.size() > dims.size())
                continue;

            auto elementOffset = 0;
            for (auto i = 0u; i < indices.size(); ++i)
                elementOffset += indices[i]
                    * (i == dims.size() - 1 ? 1 : dims[i]);

            auto elementCount = 1;
            for (auto i = indices.size(); i < dims.size(); ++i)
                elementCount *= dims[i];

            applyBufferMemberBinding(bufferData, member, binding, memberOffset,
                elementOffset, elementCount, scriptEngine);
            mUsedItems += binding.bindingItemId;
            bindingSet = true;
        }
    }
    return bindingSet;
}

bool PipelineBase::applyBufferMemberBindings(std::span<std::byte> bufferData,
    const SpvReflectBlockVariable &block, uint32_t arrayElement,
    ScriptEngine &scriptEngine)
{
    auto memberUsed = false;
    auto memberSet = false;
    for (auto i = 0u; i < block.member_count; ++i) {
        const auto &member = block.members[i];
        if (member.flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
            continue;
        memberUsed = true;

        const auto name = getBufferMemberFullName(block, arrayElement, member);
        forEachBufferMemberRec(name, member, 0,
            [&](const QString &name, const auto &member, int memberOffset) {
                if (applyBufferMemberBindings(bufferData, name, member,
                        memberOffset, scriptEngine)) {
                    memberSet = true;
                } else {
                    mMessages += MessageList::insert(mItemId,
                        MessageType::UniformNotSet, name);
                }
            });
    }
    return (!memberUsed || memberSet
        || isGlobalUniformBlockName(block.type_description->type_name));
}

//-------------------------------------------------------------------------

void forEachArrayElementRec(SpvReflectDescriptorBinding desc, uint32_t arrayDim,
    uint32_t &arrayElement,
    const std::function<void(SpvReflectDescriptorBinding, uint32_t, bool *)> &f,
    bool *variableLengthArrayDonePtr)
{
    if (arrayDim >= desc.array.dims_count)
        return f(desc, arrayElement++, variableLengthArrayDonePtr);

    // update descriptor to contain array index in the names
    const auto baseName = desc.name;
    const auto baseTypeName = (desc.type_description->type_name
            ? desc.type_description->type_name
            : "");
    auto typeDesc = *desc.type_description;
    desc.type_description = &typeDesc;

    const auto count = desc.array.dims[arrayDim];
    const auto variableLengthArray = (count == 0);
    auto variableLengthArrayDone = false;

    for (auto i = 0u; variableLengthArray || i < count; ++i) {
        const auto formatArray = [](std::string baseName, uint32_t i) {
            return baseName + "[" + std::to_string(i) + "]";
        };
        const auto name = formatArray(baseName, i);
        const auto typeName = formatArray(baseTypeName, i);
        desc.name = name.c_str();
        typeDesc.type_name = typeName.c_str();
        forEachArrayElementRec(desc, arrayDim + 1, arrayElement, f,
            (variableLengthArray ? &variableLengthArrayDone : nullptr));
        if (variableLengthArrayDone)
            break;
    }
}