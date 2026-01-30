#include "GLCall.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLRenderer.h"
#include "GLStream.h"
#include "GLTarget.h"
#include "GLTexture.h"
#include <cmath>

namespace {
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

    std::pair<int, int> getValuesOffsetCount(
        const std::vector<int> &uniformIndices,
        const std::vector<int> &bindingIndices, int arraySize)
    {
        // bindings can only less specific (contain more values)
        if (bindingIndices.size() >= uniformIndices.size())
            return {};

        // but the specified indices must match
        if (!std::equal(bindingIndices.begin(), bindingIndices.end(),
                uniformIndices.begin()))
            return {};

        auto offset = 0;
        auto count = 1;
        if (uniformIndices.size() == 2 && bindingIndices.size() == 1) {
            offset += uniformIndices[1];
        } else if (uniformIndices.size() == 2 && bindingIndices.size() == 0) {
            offset += uniformIndices[0] * arraySize + uniformIndices[1];
        } else if (uniformIndices.size() == 1 && bindingIndices.size() == 0) {
            offset += uniformIndices[0];
        } else {
            Q_ASSERT(!"higher dimensions are not supported yet");
            return {};
        }
        return { offset, count };
    }
} // namespace

GLCall::GLCall(const Call &call, const Session &session)
    : PipelineBase(call.id)
    , mCall(call)
    , mKind(getKind(call))
{
}

void GLCall::setProgram(GLProgram *program)
{
    if (mKind.draw || mKind.compute)
        mProgram = program;

    if (mProgram)
        mUsedItems += mProgram->usedItems();
}

void GLCall::setTarget(GLTarget *target)
{
    mTarget = target;
}

void GLCall::setVextexStream(GLStream *stream)
{
    mVertexStream = stream;
}

void GLCall::setIndexBuffer(GLBuffer *indices, const Block &block)
{
    if (!mKind.indexed)
        return;

    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    mIndexSize = 0;
    for (auto item : block.items)
        if (auto field = castItem<Field>(item)) {
            mIndexSize += getFieldSize(*field);
            mUsedItems += field->id;
        }
    if (!getIndexType()) {
        mMessages += MessageList::insert(block.id,
            MessageType::InvalidIndexType,
            QStringLiteral("%1 bytes").arg(mIndexSize));
        return;
    }

    mIndexBuffer = indices;
    mIndicesOffset = block.offset;
    mIndicesPerRow = getBlockStride(block) / mIndexSize;
    mIndicesRowCount = block.rowCount;
}

GLenum GLCall::getIndexType() const
{
    switch (mIndexSize) {
    case 1:  return GL_UNSIGNED_BYTE;
    case 2:  return GL_UNSIGNED_SHORT;
    case 4:  return GL_UNSIGNED_INT;
    default: return GL_NONE;
    }
}

int GLCall::getMaxElementCount(ScriptEngine &scriptEngine)
{
    if (mKind.indexed)
        return scriptEngine.evaluateInt(mIndicesRowCount, mCall.id)
            * mIndicesPerRow;
    if (mVertexStream)
        return mVertexStream->maxElementCount();
    return -1;
}

void GLCall::setIndirectBuffer(GLBuffer *commands, const Block &block)
{
    if (!mKind.indirect)
        return;

    mUsedItems += block.id;
    mUsedItems += block.parent->id;
    for (auto field : block.items)
        mUsedItems += field->id;

    mIndirectStride = getBlockStride(block);
    const auto expectedStride =
        static_cast<int>((mKind.compute ? 3 : 4) * sizeof(uint32_t));
    if (mIndirectStride != expectedStride) {
        mMessages += MessageList::insert(block.id,
            MessageType::InvalidIndirectStride,
            QStringLiteral("%1/%2 bytes")
                .arg(mIndirectStride)
                .arg(expectedStride));
        return;
    }

    mIndirectBuffer = commands;
    mIndirectOffset = block.offset;
}

void GLCall::setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer)
{
    mBuffer = buffer;
    mFromBuffer = fromBuffer;
}

void GLCall::setTextures(GLTexture *texture, GLTexture *fromTexture)
{
    mTexture = texture;
    mFromTexture = fromTexture;
}

void GLCall::execute(GLContext &context, Bindings &&bindings,
    MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (mProgram) {
        if (validateShaderTypes() && mProgram->bind()) {
            setBindings(std::move(bindings));
            if (updateBindings(scriptEngine))
                execute(messages, scriptEngine);
            mProgram->unbind();
        }
    } else {
        execute(messages, scriptEngine);
    }
}

void GLCall::execute(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (mKind.draw || mKind.compute) {
        if (!mProgram) {
            messages +=
                MessageList::insert(mCall.id, MessageType::ProgramNotAssigned);
            return;
        }
    }

    if (mKind.draw) {
        if (!mTarget) {
            messages +=
                MessageList::insert(mCall.id, MessageType::TargetNotAssigned);
            return;
        }
        mUsedItems += mTarget->usedItems();
    }

    if (mKind.indexed && !mIndexBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::IndexBufferNotAssigned);
        return;
    }

    if (mKind.indirect && !mIndirectBuffer) {
        messages += MessageList::insert(mCall.id,
            MessageType::IndirectBufferNotAssigned);
        return;
    }

    switch (mCall.callType) {
    case Call::CallType::Draw:
    case Call::CallType::DrawIndexed:
    case Call::CallType::DrawIndirect:
    case Call::CallType::DrawIndexedIndirect:
    case Call::CallType::DrawMeshTasks:
    case Call::CallType::DrawMeshTasksIndirect:
        executeDraw(messages, scriptEngine);
        break;
    case Call::CallType::Compute:
    case Call::CallType::ComputeIndirect:
        executeCompute(messages, scriptEngine);
        break;
    case Call::CallType::TraceRays:
        messages +=
            MessageList::insert(mCall.id, MessageType::RayTracingNotAvailable);
        break;
    case Call::CallType::ClearTexture: executeClearTexture(messages); break;
    case Call::CallType::CopyTexture:  executeCopyTexture(messages); break;
    case Call::CallType::ClearBuffer:  executeClearBuffer(messages); break;
    case Call::CallType::CopyBuffer:   executeCopyBuffer(messages); break;
    case Call::CallType::SwapTextures: executeSwapTextures(messages); break;
    case Call::CallType::SwapBuffers:  executeSwapBuffers(messages); break;
    }

    auto &gl = GLContext::currentContext();
    gl.glMemoryBarrier(GL_ALL_BARRIER_BITS);

    if (auto errorMessage = getFirstGLError(); !errorMessage.isEmpty())
        messages += MessageList::insert(mCall.id, MessageType::CallFailed,
            errorMessage);
}

void GLCall::executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    const auto first = scriptEngine.evaluateUInt(mCall.first, mCall.id);
    const auto maxElementCount = getMaxElementCount(scriptEngine);
    const auto count = (!mCall.count.isEmpty()
            ? scriptEngine.evaluateUInt(mCall.count, mCall.id)
            : std::max(maxElementCount - static_cast<int>(first), 0));
    const auto instanceCount =
        scriptEngine.evaluateUInt(mCall.instanceCount, mCall.id);
    const auto baseVertex =
        scriptEngine.evaluateUInt(mCall.baseVertex, mCall.id);
    const auto baseInstance =
        scriptEngine.evaluateUInt(mCall.baseInstance, mCall.id);
    const auto drawCount = scriptEngine.evaluateUInt(mCall.drawCount, mCall.id);
    const auto indexType = getIndexType();
    const auto indirectOffset = (mKind.indirect
            ? scriptEngine.evaluateUInt(mIndirectOffset, mCall.id)
            : 0);

    if (!count)
        return;

    if (maxElementCount >= 0
        && first + count > static_cast<uint32_t>(maxElementCount)) {
        mMessages += MessageList::insert(mCall.id, MessageType::CountExceeded,
            first ? QStringLiteral("%1 + %2 > %3")
                        .arg(first)
                        .arg(count)
                        .arg(maxElementCount)
                  : QStringLiteral("%1 > %2").arg(count).arg(maxElementCount));
        return;
    }

    if (!bindVertexStream())
        return;

    selectSubroutines();

    mTarget->bind();

    if (mIndexBuffer)
        mIndexBuffer->bindReadOnly(GL_ELEMENT_ARRAY_BUFFER);

    if (mIndirectBuffer)
        mIndirectBuffer->bindReadOnly(GL_DRAW_INDIRECT_BUFFER);

    auto &gl = GLContext::currentContext();

    if (mCall.primitiveType == Call::PrimitiveType::Patches)
        gl.glPatchParameteri(GL_PATCH_VERTICES,
            scriptEngine.evaluateInt(mCall.patchVertices, mCall.id));

    if (mCall.callType == Call::CallType::Draw) {
        // DrawArrays(InstancedBaseInstance)
        if (!baseInstance) {
            gl.glDrawArraysInstanced(mCall.primitiveType, first, count,
                instanceCount);
        } else {
            gl.glDrawArraysInstancedBaseInstance(mCall.primitiveType, first,
                count, instanceCount, static_cast<GLuint>(baseInstance));
        }
    } else if (mCall.callType == Call::CallType::DrawIndexed && indexType) {
        // DrawElements(InstancedBaseVertexBaseInstance)
        const auto offset = reinterpret_cast<void *>(static_cast<intptr_t>(
            scriptEngine.evaluateUInt(mIndicesOffset, mCall.id)
            + first * mIndexSize));
        if (!baseVertex && !baseInstance) {
            gl.glDrawElementsInstanced(mCall.primitiveType, count, indexType,
                offset, instanceCount);
        } else {
            gl.glDrawElementsInstancedBaseVertexBaseInstance(
                mCall.primitiveType, count, indexType, offset, instanceCount,
                baseVertex, static_cast<GLuint>(baseInstance));
        }
    } else if (mCall.callType == Call::CallType::DrawIndirect) {
        // (Multi)DrawArraysIndirect
        const auto offset =
            reinterpret_cast<void *>(static_cast<intptr_t>(indirectOffset));
        if (drawCount == 1) {
            gl.glDrawArraysIndirect(mCall.primitiveType, offset);
        } else {
            gl.glMultiDrawArraysIndirect(mCall.primitiveType, offset, drawCount,
                mIndirectStride);
        }
    } else if (mCall.callType == Call::CallType::DrawIndexedIndirect
        && indexType) {
        // (Multi)DrawElementsIndirect
        const auto offset =
            reinterpret_cast<void *>(static_cast<intptr_t>(indirectOffset));
        if (drawCount == 1) {
            gl.glDrawElementsIndirect(mCall.primitiveType, indexType, offset);
        } else {
            gl.glMultiDrawElementsIndirect(mCall.primitiveType, indexType,
                offset, drawCount, mIndirectStride);
        }
    } else if (mCall.callType == Call::CallType::DrawMeshTasks) {
        static auto glDrawMeshTasksNV =
            reinterpret_cast<PFNGLDRAWMESHTASKSNVPROC>(
                gl.getProcAddress("glDrawMeshTasksNV"));
        if (glDrawMeshTasksNV) {
            glDrawMeshTasksNV(0,
                scriptEngine.evaluateUInt(mCall.workGroupsX, mCall.id));
        } else {
            messages += MessageList::insert(mCall.id,
                MessageType::UnsupportedShaderType);
        }
    } else if (mCall.callType == Call::CallType::DrawMeshTasksIndirect) {
        static auto glDrawMeshTasksIndirectNV =
            reinterpret_cast<PFNGLDRAWMESHTASKSINDIRECTNVPROC>(
                gl.getProcAddress("glDrawMeshTasksIndirectNV"));
        static auto glMultiDrawMeshTasksIndirectNV =
            reinterpret_cast<PFNGLMULTIDRAWMESHTASKSINDIRECTNVPROC>(
                gl.getProcAddress("glMultiDrawMeshTasksIndirectNV"));
        const auto offset = static_cast<intptr_t>(indirectOffset);
        if (drawCount == 1 && glDrawMeshTasksIndirectNV) {
            glDrawMeshTasksIndirectNV(offset);
        } else if (drawCount != 1 && glMultiDrawMeshTasksIndirectNV) {
            glMultiDrawMeshTasksIndirectNV(offset, drawCount, mIndirectStride);
        } else {
            messages += MessageList::insert(mCall.id,
                MessageType::UnsupportedShaderType);
        }
    }

    unbindVertexStream();

    if (mIndirectBuffer)
        mIndirectBuffer->unbind(GL_DRAW_INDIRECT_BUFFER);

    if (mIndexBuffer)
        mIndexBuffer->unbind(GL_ELEMENT_ARRAY_BUFFER);
}

void GLCall::executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (mIndirectBuffer)
        mIndirectBuffer->bindReadOnly(GL_DISPATCH_INDIRECT_BUFFER);

    auto &gl = GLContext::currentContext();
    if (mCall.callType == Call::CallType::Compute) {
        gl.glDispatchCompute(
            scriptEngine.evaluateInt(mCall.workGroupsX, mCall.id),
            scriptEngine.evaluateInt(mCall.workGroupsY, mCall.id),
            scriptEngine.evaluateInt(mCall.workGroupsZ, mCall.id));
    } else if (mCall.callType == Call::CallType::ComputeIndirect) {
        const auto offset = static_cast<GLintptr>(
            scriptEngine.evaluateInt(mIndirectOffset, mCall.id));
        gl.glDispatchComputeIndirect(offset);
    }

    if (mIndirectBuffer)
        mIndirectBuffer->unbind(GL_DISPATCH_INDIRECT_BUFFER);
}

void GLCall::executeClearTexture(MessagePtrSet &messages)
{
    if (!mTexture) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TextureNotAssigned);
        return;
    }

    auto color = std::array<double, 4>{ mCall.clearColor.redF(),
        mCall.clearColor.greenF(), mCall.clearColor.blueF(),
        mCall.clearColor.alphaF() };

    const auto sampleType = getTextureSampleType(mTexture->format());
    if (sampleType == TextureSampleType::Float) {
        const auto srgbToLinear = [](double s) {
            if (s > 0.0031308)
                return 1.055 * (std::pow(s, (1.0 / 2.4))) - 0.055;
            return 12.92 * s;
        };
        color[0] = srgbToLinear(color[0]);
        color[1] = srgbToLinear(color[1]);
        color[2] = srgbToLinear(color[2]);
    }

    if (!mTexture->clear(color, mCall.clearDepth, mCall.clearStencil))
        messages +=
            MessageList::insert(mCall.id, MessageType::ClearingTextureFailed);

    mUsedItems += mTexture->usedItems();
}

void GLCall::executeCopyTexture(MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    if (!mTexture->copy(*mFromTexture))
        messages +=
            MessageList::insert(mCall.id, MessageType::CopyingTextureFailed);

    mUsedItems += mTexture->usedItems();
    mUsedItems += mFromTexture->usedItems();
}

void GLCall::executeClearBuffer(MessagePtrSet &messages)
{
    if (!mBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    mBuffer->clear();
    mUsedItems += mBuffer->usedItems();
}

void GLCall::executeCopyBuffer(MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    mBuffer->copy(*mFromBuffer);
    mUsedItems += mBuffer->usedItems();
    mUsedItems += mFromBuffer->usedItems();
}

void GLCall::executeSwapTextures(MessagePtrSet &messages)
{
    if (!mTexture || !mFromTexture) {
        messages +=
            MessageList::insert(mCall.id, MessageType::TextureNotAssigned);
        return;
    }
    if (!mTexture->swap(*mFromTexture))
        messages +=
            MessageList::insert(mCall.id, MessageType::SwappingTexturesFailed);
}

void GLCall::executeSwapBuffers(MessagePtrSet &messages)
{
    if (!mBuffer || !mFromBuffer) {
        messages +=
            MessageList::insert(mCall.id, MessageType::BufferNotAssigned);
        return;
    }
    if (!mBuffer->swap(*mFromBuffer))
        messages +=
            MessageList::insert(mCall.id, MessageType::SwappingBuffersFailed);
}

bool GLCall::validateShaderTypes()
{
    if (!mProgram)
        return false;
    for (const auto &shader : mProgram->shaders())
        if (!callTypeSupportsShaderType(mCall.callType, shader.type())) {
            mMessages += MessageList::insert(mCall.id,
                MessageType::InvalidShaderTypeForCall);
            return false;
        }
    return true;
}

bool GLCall::updateBindings(ScriptEngine &scriptEngine)
{
    if (!mProgram)
        return false;

    const auto &reflection = mProgram->reflection();

    auto canRender = true;
    for (const auto &desc : reflection.descriptorBindings()) {
        //if (!desc.accessed)
        //    continue;

        auto arrayElement = uint32_t{};
        forEachArrayElementRec(desc, 0, arrayElement,
            [&](const SpvReflectDescriptorBinding &desc, uint32_t arrayElement,
                bool *variableLengthArrayDone) {
                const auto message = applyBinding(desc, arrayElement,
                    (variableLengthArrayDone ? true : false), scriptEngine);

                const auto failed = (message != MessageType::None);
                if (variableLengthArrayDone && failed) {
                    *variableLengthArrayDone = true;
                    if (message == MessageType::SamplerNotSet
                        || message == MessageType::BufferNotSet)
                        return;
                }

                if (failed) {
                    auto name = desc.name;
                    if (isBufferBinding(desc.descriptor_type))
                        name = desc.type_description->type_name;
                    mMessages += MessageList::insert(mItemId, message, name);
                    canRender = false;
                }
            });
    }

    for (const auto &[name, uniform] : mProgram->uniforms())
        applyUniformBindings(name, uniform, scriptEngine);

    selectSubroutines();

    return canRender;
}

MessageType GLCall::applyBinding(const SpvReflectDescriptorBinding &desc,
    uint32_t arrayElement, bool isVariableLengthArray,
    ScriptEngine &scriptEngine)
{
    switch (desc.descriptor_type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        if (const auto bufferBinding =
                find(mBindings.buffers, desc.type_description->type_name)) {
            if (!bufferBinding->buffer)
                return MessageType::BufferNotSet;
            auto &buffer = static_cast<GLBuffer &>(*bufferBinding->buffer);
            mUsedItems += bufferBinding->bindingItemId;
            mUsedItems += bufferBinding->blockItemId;
            mUsedItems += buffer.usedItems();

            const auto [offset, size] =
                getBufferBindingOffsetSize(*bufferBinding, scriptEngine);

            buffer.bindIndexedRange(GL_UNIFORM_BUFFER,
                desc.binding + arrayElement, offset, size, true);
        } else {
            auto &buffer = mProgram->getDynamicUniformBuffer(
                desc.type_description->type_name, desc.block.size);

            Q_ASSERT(desc.block.size == buffer.size());
            if (desc.block.size != buffer.size())
                return MessageType::BufferNotSet;

            auto bufferData = std::span<std::byte>(
                reinterpret_cast<std::byte *>(buffer.writableData().data()),
                buffer.size());
            if (!applyBufferMemberBindings(bufferData, desc.block, arrayElement,
                    scriptEngine))
                return MessageType::BufferNotSet;

            buffer.bindIndexedRange(GL_UNIFORM_BUFFER, desc.binding, 0,
                buffer.size(), true);
        }
        break;

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
        const auto &name = desc.type_description->type_name;
        auto buffer = std::add_pointer_t<GLBuffer>();
        auto offset = uint32_t{};
        auto size = uint32_t{};
        if (name == PrintfBase::bufferBindingName()) {
            auto &gl = GLContext::currentContext();
            buffer = &mProgram->printf().getInitializedBuffer(gl);
        } else if (const auto bufferBinding = find(mBindings.buffers, name)) {
            buffer = static_cast<GLBuffer *>(bufferBinding->buffer);
            std::tie(offset, size) =
                getBufferBindingOffsetSize(*bufferBinding, scriptEngine);
            mUsedItems += bufferBinding->bindingItemId;
            mUsedItems += buffer->usedItems();
        }
        if (!buffer)
            return MessageType::BufferNotSet;

        const auto readonly =
            (desc.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
        // TODO: GL_ATOMIC_COUNTER_BUFFER
        buffer->bindIndexedRange(GL_SHADER_STORAGE_BUFFER,
            desc.binding + arrayElement, offset, size, readonly);
    } break;

    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return MessageType::OpenGLRequiresCombinedTextureSamplers;

    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
        const auto samplerBinding = find(mBindings.samplers, desc.name);
        if (!samplerBinding)
            return MessageType::SamplerNotSet;
        mUsedItems += samplerBinding->bindingItemId;

        if (!samplerBinding->texture)
            return MessageType::TextureNotAssigned;
        mUsedItems += samplerBinding->texture->itemId();

        if (!applySamplerBinding(desc, *samplerBinding))
            return MessageType::TextureNotAssigned;
        break;
    }

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
        const auto imageBinding = find(mBindings.images, desc.name);
        if (!imageBinding)
            return MessageType::ImageNotSet;
        mUsedItems += imageBinding->bindingItemId;

        if (!imageBinding->texture)
            return MessageType::TextureNotAssigned;
        mUsedItems += imageBinding->texture->itemId();

        if (!applyImageBinding(desc, *imageBinding))
            return MessageType::TextureNotAssigned;
        break;
    }

    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return MessageType::NotImplemented;

    default:
        Q_ASSERT(!"descriptor type not handled");
        return MessageType::NotImplemented;
    }
    return MessageType::None;
}

void GLCall::applyUniformBindings(const QString &name,
    const GLProgram::Uniform &uniform, ScriptEngine &scriptEngine)
{
    if (const auto binding = find(mBindings.uniforms, name)) {
        applyUniformBinding(uniform, *binding, -1, uniform.arrayElements,
            scriptEngine);
        mUsedItems += binding->bindingItemId;
        return;
    }

    // compare array uniforms also by basename
    auto bindingSet = false;
    const auto baseName = getBaseName(name);
    const auto uniformIndices = getArrayIndices(name);
    for (const auto &[bindingName, binding] : mBindings.uniforms)
        if (getBaseName(bindingName) == baseName) {
            const auto bindingIndices = getArrayIndices(bindingName);
            const auto [offset, count] = getValuesOffsetCount(uniformIndices,
                bindingIndices, uniform.arrayElements);
            if (count) {
                applyUniformBinding(uniform, binding, offset, count,
                    scriptEngine);
                mUsedItems += binding.bindingItemId;
            }
            bindingSet = true;
        }

    if (!bindingSet)
        mMessages += MessageList::insert(mCall.id, MessageType::UniformNotSet,
            getBaseName(name).toString());
}

void GLCall::applyUniformBinding(const GLProgram::Uniform &uniform,
    const UniformBinding &binding, int offset, int count,
    ScriptEngine &scriptEngine)
{
    auto &gl = GLContext::currentContext();
    const auto itemId = binding.bindingItemId;
    switch (uniform.dataType) {
#define ADD(TYPE, DATATYPE, COUNT, FUNCTION)                                  \
    case TYPE:                                                                \
        FUNCTION(uniform.location, uniform.arrayElements,                     \
            getValues<DATATYPE>(scriptEngine, binding.values, COUNT * offset, \
                COUNT * count, itemId)                                        \
                .data());                                                     \
        break

#define ADD_MATRIX(TYPE, DATATYPE, COUNT, FUNCTION)                           \
    case TYPE:                                                                \
        FUNCTION(uniform.location, uniform.arrayElements, binding.transpose,  \
            getValues<DATATYPE>(scriptEngine, binding.values, COUNT * offset, \
                COUNT * count, itemId)                                        \
                .data());                                                     \
        break

        ADD(GL_FLOAT, GLfloat, 1, gl.glUniform1fv);
        ADD(GL_FLOAT_VEC2, GLfloat, 2, gl.glUniform2fv);
        ADD(GL_FLOAT_VEC3, GLfloat, 3, gl.glUniform3fv);
        ADD(GL_FLOAT_VEC4, GLfloat, 4, gl.glUniform4fv);
        ADD(GL_DOUBLE, GLdouble, 1, gl.glUniform1dv);
        ADD(GL_DOUBLE_VEC2, GLdouble, 2, gl.glUniform2dv);
        ADD(GL_DOUBLE_VEC3, GLdouble, 3, gl.glUniform3dv);
        ADD(GL_DOUBLE_VEC4, GLdouble, 4, gl.glUniform4dv);
        ADD(GL_INT, GLint, 1, gl.glUniform1iv);
        ADD(GL_INT_VEC2, GLint, 2, gl.glUniform2iv);
        ADD(GL_INT_VEC3, GLint, 3, gl.glUniform3iv);
        ADD(GL_INT_VEC4, GLint, 4, gl.glUniform4iv);
        ADD(GL_UNSIGNED_INT, GLuint, 1, gl.glUniform1uiv);
        ADD(GL_UNSIGNED_INT_VEC2, GLuint, 2, gl.glUniform2uiv);
        ADD(GL_UNSIGNED_INT_VEC3, GLuint, 3, gl.glUniform3uiv);
        ADD(GL_UNSIGNED_INT_VEC4, GLuint, 4, gl.glUniform4uiv);
        ADD(GL_BOOL, GLint, 1, gl.glUniform1iv);
        ADD(GL_BOOL_VEC2, GLint, 2, gl.glUniform2iv);
        ADD(GL_BOOL_VEC3, GLint, 3, gl.glUniform3iv);
        ADD(GL_BOOL_VEC4, GLint, 4, gl.glUniform4iv);
        ADD_MATRIX(GL_FLOAT_MAT2, GLfloat, 4, gl.glUniformMatrix2fv);
        ADD_MATRIX(GL_FLOAT_MAT3, GLfloat, 9, gl.glUniformMatrix3fv);
        ADD_MATRIX(GL_FLOAT_MAT4, GLfloat, 16, gl.glUniformMatrix4fv);
        ADD_MATRIX(GL_FLOAT_MAT2x3, GLfloat, 6, gl.glUniformMatrix2x3fv);
        ADD_MATRIX(GL_FLOAT_MAT3x2, GLfloat, 6, gl.glUniformMatrix3x2fv);
        ADD_MATRIX(GL_FLOAT_MAT2x4, GLfloat, 8, gl.glUniformMatrix2x4fv);
        ADD_MATRIX(GL_FLOAT_MAT4x2, GLfloat, 8, gl.glUniformMatrix4x2fv);
        ADD_MATRIX(GL_FLOAT_MAT3x4, GLfloat, 12, gl.glUniformMatrix3x4fv);
        ADD_MATRIX(GL_FLOAT_MAT4x3, GLfloat, 12, gl.glUniformMatrix4x3fv);
        ADD_MATRIX(GL_DOUBLE_MAT2, GLdouble, 4, gl.glUniformMatrix2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3, GLdouble, 9, gl.glUniformMatrix3dv);
        ADD_MATRIX(GL_DOUBLE_MAT4, GLdouble, 16, gl.glUniformMatrix4dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x3, GLdouble, 6, gl.glUniformMatrix2x3dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x2, GLdouble, 6, gl.glUniformMatrix3x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x4, GLdouble, 8, gl.glUniformMatrix2x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x2, GLdouble, 8, gl.glUniformMatrix4x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x4, GLdouble, 12, gl.glUniformMatrix3x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x3, GLdouble, 12, gl.glUniformMatrix4x3dv);
#undef ADD
#undef ADD_MATRIX
    }
}

bool GLCall::applySamplerBinding(const SpvReflectDescriptorBinding &desc,
    const SamplerBinding &binding)
{
    Q_ASSERT(desc.binding >= 0);
    Q_ASSERT(binding.texture);

    float borderColor[] = { static_cast<float>(binding.borderColor.redF()),
        static_cast<float>(binding.borderColor.greenF()),
        static_cast<float>(binding.borderColor.blueF()),
        static_cast<float>(binding.borderColor.alphaF()) };

    auto &texture = static_cast<GLTexture &>(*binding.texture);
    const auto target = texture.target();
    auto &gl = GLContext::currentContext();
    gl.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + desc.binding));
    texture.updateMipmaps(gl);
    gl.glBindTexture(target, texture.getReadOnlyTextureId());
    if (auto location = mProgram->getUniformLocation(desc); location >= 0)
        gl.glUniform1i(location, desc.binding);

    switch (target) {
    case QOpenGLTexture::Target1D:
    case QOpenGLTexture::Target1DArray:
    case QOpenGLTexture::Target2D:
    case QOpenGLTexture::Target2DArray:
    case QOpenGLTexture::Target3D:
    case QOpenGLTexture::TargetCubeMap:
    case QOpenGLTexture::TargetCubeMapArray:
    case QOpenGLTexture::TargetRectangle:
        gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, binding.minFilter);
        gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, binding.magFilter);
        if (binding.minFilter != Binding::Filter::Nearest) {
            auto anisotropy = 1.0f;
            if (binding.anisotropic)
                gl.glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
            gl.glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                anisotropy);
        }
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, binding.wrapModeX);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, binding.wrapModeY);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, binding.wrapModeZ);
        gl.glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
        if (binding.comparisonFunc) {
            gl.glTexParameteri(target, GL_TEXTURE_COMPARE_MODE,
                GL_COMPARE_REF_TO_TEXTURE);
            gl.glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC,
                binding.comparisonFunc);
        } else {
            gl.glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
        break;

    default: return false;
    }
    return true;
}

bool GLCall::applyImageBinding(const SpvReflectDescriptorBinding &desc,
    const ImageBinding &binding)
{
    Q_ASSERT(desc.binding >= 0);
    Q_ASSERT(binding.texture);

    auto &gl = GLContext::currentContext();
    auto &texture = static_cast<GLTexture &>(*binding.texture);
    const auto target = texture.target();
    const auto textureId = texture.getReadWriteTextureId();
    const auto format = (binding.format
            ? static_cast<GLenum>(binding.format)
            : static_cast<GLenum>(texture.format()));

    auto formatSupported = GLint();
    gl.glGetInternalformativ(target, format, GL_SHADER_IMAGE_LOAD, 1,
        &formatSupported);
    if (formatSupported == GL_NONE) {
        mMessages += MessageList::insert(binding.bindingItemId,
            MessageType::ImageFormatNotBindable);
        return false;
    }
    gl.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + desc.binding));
    gl.glBindTexture(target, textureId);
    if (auto location = mProgram->getUniformLocation(desc); location >= 0)
        gl.glUniform1i(location, desc.binding);
    gl.glBindImageTexture(static_cast<GLuint>(desc.binding), textureId,
        binding.level, (binding.layer < 0), std::max(binding.layer, 0),
        GL_READ_WRITE, format);

    return true;
}

void GLCall::selectSubroutines()
{
    auto &gl = GLContext::currentContext();
    for (const auto &[stage, subroutines] : mProgram->stageSubroutines()) {
        auto subroutineIndices = std::vector<GLuint>();
        for (const auto &subroutine : subroutines) {
            const auto binding = [&]() -> const SubroutineBinding * {
                for (const auto &[name, binding] : mBindings.subroutines)
                    if (name == subroutine.name)
                        return &binding;
                return nullptr;
            }();

            auto index = 0;
            if (binding) {
                mUsedItems += binding->bindingItemId;

                index = subroutine.subroutines.indexOf(binding->subroutine);
                if (index < 0) {
                    index = 0;
                    mMessages += MessageList::insert(binding->bindingItemId,
                        MessageType::InvalidSubroutine, binding->subroutine);
                }
            } else {
                mMessages += MessageList::insert(mCall.id,
                    MessageType::SubroutineNotSet, subroutine.name);
            }
            subroutineIndices.push_back(index);
        }

        gl.glUniformSubroutinesuiv(stage,
            static_cast<GLsizei>(subroutineIndices.size()),
            subroutineIndices.data());
    }
}

bool GLCall::bindVertexStream()
{
    if (mVertexStream)
        mUsedItems += mVertexStream->itemId();

    auto &gl = GLContext::currentContext();
    auto canRender = true;
    for (const auto *input : mProgram->reflection().inputVariables()) {
        if (isBuiltIn(*input))
            continue;

        const auto name = (input->semantic ? input->semantic : input->name);
        const auto *attributePtr =
            (mVertexStream ? mVertexStream->findAttribute(name) : nullptr);
        if (attributePtr)
            mUsedItems += attributePtr->usedItems;

        if (!attributePtr || !attributePtr->buffer) {
            mMessages += MessageList::insert(mCall.id,
                MessageType::AttributeNotSet, name);
            canRender = false;
            continue;
        }

        const auto location = input->location;
        const auto &attribute = *attributePtr;
        auto &buffer = *attribute.buffer;
        buffer.bindReadOnly(GL_ARRAY_BUFFER);

        switch (attribute.type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
            // normalized integer types fall through to glVertexAttribPointer
            if (!attribute.normalize) {
                gl.glVertexAttribIPointer(location, attribute.count,
                    attribute.type, attribute.stride,
                    reinterpret_cast<void *>(
                        static_cast<intptr_t>(attribute.offset)));
                break;
            }
            [[fallthrough]];

        default:
            gl.glVertexAttribPointer(location, attribute.count, attribute.type,
                attribute.normalize, attribute.stride,
                reinterpret_cast<void *>(
                    static_cast<intptr_t>(attribute.offset)));
            break;

        case GL_DOUBLE:
            gl.glVertexAttribLPointer(location, attribute.count, attribute.type,
                attribute.stride,
                reinterpret_cast<void *>(
                    static_cast<intptr_t>(attribute.offset)));
            break;
        }

        gl.glVertexAttribDivisor(location,
            static_cast<GLuint>(attribute.divisor));

        gl.glEnableVertexAttribArray(location);
        attribute.buffer->unbind(GL_ARRAY_BUFFER);
    }
    return canRender;
}

void GLCall::unbindVertexStream()
{
    auto &gl = GLContext::currentContext();
    for (const auto *input : mProgram->reflection().inputVariables())
        if (!isBuiltIn(*input))
            gl.glDisableVertexAttribArray(input->location);
}
