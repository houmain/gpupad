#pragma once

#include "Reflection.h"
#include "RenderSessionBase.h"
#include "scripting/ScriptEngine.h"
#include <set>
#include <span>

struct ShaderRecordElement
{
    uint32_t offset{};
    Field::DataType dataType{};

    bool operator==(const ShaderRecordElement &) const = default;
};

struct ShaderRecordLayout
{
    uint32_t size{};
    std::vector<ShaderRecordElement> elements;

    bool operator==(const ShaderRecordLayout &) const = default;
};

ShaderRecordLayout getShaderRecordLayout(
    const SpvReflectBlockVariable &block);
std::vector<std::byte> packShaderRecordData(
    const ShaderRecordLayout &layout, const ScriptValueList &values);

class PipelineBase
{
public:
    void setBindings(Bindings &&bindings);
    void markBufferWrites();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

protected:
    explicit PipelineBase(ItemId itemId);

    std::pair<uint32_t, uint32_t> getBufferBindingOffsetSize(
        const BufferBinding &binding, ScriptEngine &scriptEngine) const;
    void applyBufferMemberBinding(std::span<std::byte> bufferData,
        const SpvReflectBlockVariable &member, const UniformBinding &binding,
        int memberOffset, int elementOffset, int count,
        ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(std::span<std::byte> bufferData,
        const QString &name, const SpvReflectBlockVariable &member,
        int memberOffset, ScriptEngine &scriptEngine);
    bool applyBufferMemberBindings(std::span<std::byte> bufferData,
        const SpvReflectBlockVariable &block, uint32_t arrayElement,
        ScriptEngine &scriptEngine);
    void addWritableBuffer(BufferBase *buffer);

    ItemId mItemId{ };
    Bindings mBindings;
    std::set<BufferBase *> mWritableBuffers;
    MessageList mMessages;
    QSet<ItemId> mUsedItems;
};

void forEachArrayElementRec(SpvReflectDescriptorBinding desc, uint32_t arrayDim,
    uint32_t &arrayElement,
    const std::function<void(SpvReflectDescriptorBinding, uint32_t, bool *)> &f,
    bool *variableLengthArrayDonePtr = nullptr);
