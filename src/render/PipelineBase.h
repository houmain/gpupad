#pragma once

#include "Reflection.h"
#include "RenderSessionBase.h"
#include "scripting/ScriptEngine.h"
#include <span>

class PipelineBase
{
public:
    void setBindings(Bindings &&bindings);
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

    ItemId mItemId{};
    Bindings mBindings;
    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
};

void forEachArrayElementRec(SpvReflectDescriptorBinding desc, uint32_t arrayDim,
    uint32_t &arrayElement,
    const std::function<void(SpvReflectDescriptorBinding, uint32_t, bool *)> &f,
    bool *variableLengthArrayDonePtr = nullptr);
