#include "GLCall.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLRenderer.h"
#include "GLStream.h"
#include "GLTarget.h"
#include "GLTexture.h"
#include <QOpenGLTimerQuery>
#include <cmath>

namespace {
    template <typename C>
    auto find(C &container, const QString &name)
    {
        const auto it = container.find(name);
        return (it == container.end() ? nullptr : &it->second);
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

    bool isImageUniform(const GLProgram::Interface::Uniform &uniform)
    {
        switch (uniform.dataType) {
        case GL_IMAGE_1D:
        case GL_IMAGE_2D:
        case GL_IMAGE_3D:
        case GL_IMAGE_2D_RECT:
        case GL_IMAGE_CUBE:
        case GL_IMAGE_BUFFER:
        case GL_IMAGE_1D_ARRAY:
        case GL_IMAGE_2D_ARRAY:
        case GL_IMAGE_CUBE_MAP_ARRAY:
        case GL_IMAGE_2D_MULTISAMPLE:
        case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_INT_IMAGE_1D:
        case GL_INT_IMAGE_2D:
        case GL_INT_IMAGE_3D:
        case GL_INT_IMAGE_2D_RECT:
        case GL_INT_IMAGE_CUBE:
        case GL_INT_IMAGE_BUFFER:
        case GL_INT_IMAGE_1D_ARRAY:
        case GL_INT_IMAGE_2D_ARRAY:
        case GL_INT_IMAGE_CUBE_MAP_ARRAY:
        case GL_INT_IMAGE_2D_MULTISAMPLE:
        case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_1D:
        case GL_UNSIGNED_INT_IMAGE_2D:
        case GL_UNSIGNED_INT_IMAGE_3D:
        case GL_UNSIGNED_INT_IMAGE_2D_RECT:
        case GL_UNSIGNED_INT_IMAGE_CUBE:
        case GL_UNSIGNED_INT_IMAGE_BUFFER:
        case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY: return true;
        default:                                         return false;
        }
    }

    bool isSamplerUniform(const GLProgram::Interface::Uniform &uniform)
    {
        switch (uniform.dataType) {
        case GL_SAMPLER_1D:
        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_1D_ARRAY_SHADOW:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_INT_SAMPLER_1D:
        case GL_INT_SAMPLER_1D_ARRAY:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:                 return true;
        default:                                           return false;
        }
    }

    bool isSamplerOnlyUniform(const GLProgram::Interface::Uniform &uniform)
    {
        return (uniform.dataType == GL_SAMPLER);
    }
} // namespace

GLCall::GLCall(const Call &call) : mCall(call), mKind(getKind(call)) { }

void GLCall::setProgram(GLProgram *program)
{
    mProgram = program;
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
        return static_cast<int>(evaluateUInt(scriptEngine, mIndicesRowCount))
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

std::shared_ptr<void> GLCall::beginTimerQuery()
{
    if (!mTimerQuery) {
        mTimerQuery = std::make_shared<QOpenGLTimerQuery>();
        mTimerQuery->create();
    }
    mTimerQuery->begin();
    return std::shared_ptr<void>(nullptr,
        [this](void *) { mTimerQuery->end(); });
}

void GLCall::execute(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    if (mKind.draw || mKind.compute) {
        if (!mProgram) {
            messages +=
                MessageList::insert(mCall.id, MessageType::ProgramNotAssigned);
            return;
        }
        mUsedItems += mProgram->usedItems();
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

#if GL_VERSION_4_2
    auto &gl = GLContext::currentContext();
    if (gl.v4_2)
        gl.v4_2->glMemoryBarrier(GL_ALL_BARRIER_BITS);
#endif

    if (auto errorMessage = getFirstGLError(); !errorMessage.isEmpty())
        messages += MessageList::insert(mCall.id, MessageType::CallFailed,
            errorMessage);
}

int GLCall::evaluateInt(ScriptEngine &scriptEngine, const QString &expression)
{
    return scriptEngine.evaluateInt(expression, mCall.id, mMessages);
}

uint32_t GLCall::evaluateUInt(ScriptEngine &scriptEngine,
    const QString &expression)
{
    return scriptEngine.evaluateUInt(expression, mCall.id, mMessages);
}

void GLCall::executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine)
{
    const auto first = evaluateUInt(scriptEngine, mCall.first);
    const auto maxElementCount = getMaxElementCount(scriptEngine);
    const auto count = (!mCall.count.isEmpty()
            ? evaluateUInt(scriptEngine, mCall.count)
            : std::max(maxElementCount - static_cast<int>(first), 0));
    const auto instanceCount = evaluateUInt(scriptEngine, mCall.instanceCount);
    const auto baseVertex = evaluateUInt(scriptEngine, mCall.baseVertex);
    const auto baseInstance = evaluateUInt(scriptEngine, mCall.baseInstance);
    const auto drawCount = evaluateUInt(scriptEngine, mCall.drawCount);
    const auto indexType = getIndexType();
    const auto indirectOffset =
        (mKind.indirect ? evaluateUInt(scriptEngine, mIndirectOffset) : 0);

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

    mTarget->bind();

    if (mIndexBuffer)
        mIndexBuffer->bindReadOnly(GL_ELEMENT_ARRAY_BUFFER);

    if (mIndirectBuffer)
        mIndirectBuffer->bindReadOnly(GL_DRAW_INDIRECT_BUFFER);

    auto &gl = GLContext::currentContext();

    if (mCall.primitiveType == Call::PrimitiveType::Patches && gl.v4_0)
        gl.v4_0->glPatchParameteri(GL_PATCH_VERTICES,
            evaluateInt(scriptEngine, mCall.patchVertices));

    auto guard = beginTimerQuery();
    if (mCall.callType == Call::CallType::Draw) {
        // DrawArrays(InstancedBaseInstance)
        if (!baseInstance) {
            gl.glDrawArraysInstanced(mCall.primitiveType, first, count,
                instanceCount);
        } else if (auto gl42 = check(gl.v4_2, mCall.id, messages)) {
            gl42->glDrawArraysInstancedBaseInstance(mCall.primitiveType, first,
                count, instanceCount, static_cast<GLuint>(baseInstance));
        }
    } else if (mCall.callType == Call::CallType::DrawIndexed && indexType) {
        // DrawElements(InstancedBaseVertexBaseInstance)
        const auto offset = reinterpret_cast<void *>(static_cast<intptr_t>(
            evaluateUInt(scriptEngine, mIndicesOffset) + first * mIndexSize));
        if (!baseVertex && !baseInstance) {
            gl.glDrawElementsInstanced(mCall.primitiveType, count, indexType,
                offset, instanceCount);
        } else if (auto gl42 = check(gl.v4_2, mCall.id, messages)) {
            gl42->glDrawElementsInstancedBaseVertexBaseInstance(
                mCall.primitiveType, count, indexType, offset, instanceCount,
                baseVertex, static_cast<GLuint>(baseInstance));
        }
    } else if (mCall.callType == Call::CallType::DrawIndirect) {
        // (Multi)DrawArraysIndirect
        const auto offset =
            reinterpret_cast<void *>(static_cast<intptr_t>(indirectOffset));
        if (drawCount == 1) {
            if (auto gl40 = check(gl.v4_0, mCall.id, messages))
                gl40->glDrawArraysIndirect(mCall.primitiveType, offset);
        } else if (auto gl43 = check(gl.v4_3, mCall.id, messages)) {
            gl43->glMultiDrawArraysIndirect(mCall.primitiveType, offset,
                drawCount, mIndirectStride);
        }
    } else if (mCall.callType == Call::CallType::DrawIndexedIndirect
        && indexType) {
        // (Multi)DrawElementsIndirect
        const auto offset =
            reinterpret_cast<void *>(static_cast<intptr_t>(indirectOffset));
        if (drawCount == 1) {
            if (auto gl40 = check(gl.v4_0, mCall.id, messages))
                gl40->glDrawElementsIndirect(mCall.primitiveType, indexType,
                    offset);
        } else if (auto gl43 = check(gl.v4_3, mCall.id, messages)) {
            gl43->glMultiDrawElementsIndirect(mCall.primitiveType, indexType,
                offset, drawCount, mIndirectStride);
        }
    } else if (mCall.callType == Call::CallType::DrawMeshTasks) {
        static auto glDrawMeshTasksNV =
            reinterpret_cast<PFNGLDRAWMESHTASKSNVPROC>(
                gl.getProcAddress("glDrawMeshTasksNV"));
        if (glDrawMeshTasksNV) {
            glDrawMeshTasksNV(0, evaluateUInt(scriptEngine, mCall.workGroupsX));
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
#if GL_VERSION_4_3
    if (mIndirectBuffer)
        mIndirectBuffer->bindReadOnly(GL_DISPATCH_INDIRECT_BUFFER);

    auto &gl = GLContext::currentContext();
    auto guard = beginTimerQuery();
    if (auto gl43 = check(gl.v4_3, mCall.id, messages)) {
        if (mCall.callType == Call::CallType::Compute) {
            gl43->glDispatchCompute(
                evaluateInt(scriptEngine, mCall.workGroupsX),
                evaluateInt(scriptEngine, mCall.workGroupsY),
                evaluateInt(scriptEngine, mCall.workGroupsZ));
        } else if (mCall.callType == Call::CallType::ComputeIndirect) {
            const auto offset = static_cast<GLintptr>(
                evaluateInt(scriptEngine, mIndirectOffset));
            gl43->glDispatchComputeIndirect(offset);
        }
    }

    if (mIndirectBuffer)
        mIndirectBuffer->unbind(GL_DISPATCH_INDIRECT_BUFFER);
#endif // GL_VERSION_4_3
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

    auto guard = beginTimerQuery();
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
    auto guard = beginTimerQuery();
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
    auto guard = beginTimerQuery();
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
    auto guard = beginTimerQuery();
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

bool GLCall::applyBindings(const GLBindings &bindings,
    ScriptEngine &scriptEngine)
{
    if (!mProgram)
        return false;
    const auto &interface = mProgram->interface();

    auto canRender = true;
    for (const auto &[name, bindingPoint] : interface.bufferBindingPoints) {

        if (auto bufferBinding = find(bindings.buffers, name)) {
            if (!applyBufferBinding(bindingPoint, *bufferBinding, scriptEngine))
                canRender = false;
            mUsedItems += bufferBinding->bindingItemId;

        } else if (!applyDynamicBufferBindings(name, bindingPoint,
                       bindings.uniforms, scriptEngine)) {
            mMessages +=
                MessageList::insert(mCall.id, MessageType::BufferNotSet, name);
            canRender = false;
        }
    }

    auto textureUnit = 0;
    for (const auto &[name, uniform] : interface.uniforms) {
        if (isImageUniform(uniform)) {
            if (auto imageBinding = find(bindings.images, name)) {
                mUsedItems += imageBinding->bindingItemId;
                if (!applyImageBinding(uniform, *imageBinding, textureUnit++))
                    canRender = false;
            } else {
                mMessages += MessageList::insert(mCall.id,
                    MessageType::ImageNotSet, name);
                canRender = false;
            }
        } else if (isSamplerUniform(uniform)) {
            if (auto samplerBinding = find(bindings.samplers, name)) {
                mUsedItems += samplerBinding->bindingItemId;
                if (!applySamplerBinding(uniform, *samplerBinding,
                        textureUnit++))
                    canRender = false;
            } else {
                mMessages += MessageList::insert(mCall.id,
                    MessageType::SamplerNotSet, name);
            }
        } else if (isSamplerOnlyUniform(uniform)) {
            mMessages += MessageList::insert(mCall.id,
                MessageType::OpenGLRequiresCombinedTextureSamplers);
            canRender = false;
        } else {
            if (!applyUniformBindings(name, uniform, bindings.uniforms,
                    scriptEngine))
                mMessages += MessageList::insert(mCall.id,
                    MessageType::UniformNotSet, getBaseName(name).toString());
        }
    }

    for (const auto &[stage, subroutines] : interface.stageSubroutines)
        selectSubroutines(stage, subroutines, bindings.subroutines);

    return canRender;
}

bool GLCall::applyUniformBindings(const QString &name,
    const GLProgram::Interface::Uniform &uniform,
    const std::map<QString, GLUniformBinding> &bindings,
    ScriptEngine &scriptEngine)
{
    if (const auto binding = find(bindings, name)) {
        applyUniformBinding(uniform, *binding, -1, uniform.size, scriptEngine);
        mUsedItems += binding->bindingItemId;
        return true;
    }

    // compare array uniforms also by basename
    auto bindingSet = false;
    const auto baseName = getBaseName(name);
    const auto uniformIndices = getArrayIndices(name);
    for (const auto &[bindingName, binding] : bindings)
        if (getBaseName(bindingName) == baseName) {
            const auto bindingIndices = getArrayIndices(bindingName);
            const auto [offset, count] = getValuesOffsetCount(uniformIndices,
                bindingIndices, uniform.size);
            if (count) {
                applyUniformBinding(uniform, binding, offset, count,
                    scriptEngine);
                mUsedItems += binding.bindingItemId;
            }
            bindingSet = true;
        }

    return bindingSet;
}

void GLCall::applyUniformBinding(const GLProgram::Interface::Uniform &uniform,
    const GLUniformBinding &binding, int offset, int count,
    ScriptEngine &scriptEngine)
{
    auto &gl = GLContext::currentContext();
    const auto itemId = binding.bindingItemId;
    switch (uniform.dataType) {
#define ADD(TYPE, DATATYPE, COUNT, FUNCTION)                                  \
    case TYPE:                                                                \
        FUNCTION(uniform.location, uniform.size,                              \
            getValues<DATATYPE>(scriptEngine, binding.values, COUNT * offset, \
                COUNT * count, itemId, mMessages)                             \
                .data());                                                     \
        break

#define ADD_MATRIX(TYPE, DATATYPE, COUNT, FUNCTION)                           \
    case TYPE:                                                                \
        FUNCTION(uniform.location, uniform.size, binding.transpose,           \
            getValues<DATATYPE>(scriptEngine, binding.values, COUNT * offset, \
                COUNT * count, itemId, mMessages)                             \
                .data());                                                     \
        break

        ADD(GL_FLOAT, GLfloat, 1, gl.glUniform1fv);
        ADD(GL_FLOAT_VEC2, GLfloat, 2, gl.glUniform2fv);
        ADD(GL_FLOAT_VEC3, GLfloat, 3, gl.glUniform3fv);
        ADD(GL_FLOAT_VEC4, GLfloat, 4, gl.glUniform4fv);
        ADD(GL_DOUBLE, GLdouble, 1, gl.v4_0->glUniform1dv);
        ADD(GL_DOUBLE_VEC2, GLdouble, 2, gl.v4_0->glUniform2dv);
        ADD(GL_DOUBLE_VEC3, GLdouble, 3, gl.v4_0->glUniform3dv);
        ADD(GL_DOUBLE_VEC4, GLdouble, 4, gl.v4_0->glUniform4dv);
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
        ADD_MATRIX(GL_DOUBLE_MAT2, GLdouble, 4, gl.v4_0->glUniformMatrix2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3, GLdouble, 9, gl.v4_0->glUniformMatrix3dv);
        ADD_MATRIX(GL_DOUBLE_MAT4, GLdouble, 16, gl.v4_0->glUniformMatrix4dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x3, GLdouble, 6,
            gl.v4_0->glUniformMatrix2x3dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x2, GLdouble, 6,
            gl.v4_0->glUniformMatrix3x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x4, GLdouble, 8,
            gl.v4_0->glUniformMatrix2x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x2, GLdouble, 8,
            gl.v4_0->glUniformMatrix4x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x4, GLdouble, 12,
            gl.v4_0->glUniformMatrix3x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x3, GLdouble, 12,
            gl.v4_0->glUniformMatrix4x3dv);
#undef ADD
#undef ADD_MATRIX
    }
}

bool GLCall::applySamplerBinding(const GLProgram::Interface::Uniform &uniform,
    const GLSamplerBinding &binding, int unit)
{
    if (!binding.texture) {
        mMessages += MessageList::insert(binding.bindingItemId,
            MessageType::TextureNotAssigned);
        return false;
    }
    auto &texture = *binding.texture;
    mUsedItems += texture.itemId();

    float borderColor[] = { static_cast<float>(binding.borderColor.redF()),
        static_cast<float>(binding.borderColor.greenF()),
        static_cast<float>(binding.borderColor.blueF()),
        static_cast<float>(binding.borderColor.alphaF()) };

    const auto target = texture.target();
    auto &gl = GLContext::currentContext();
    gl.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    texture.updateMipmaps();
    gl.glBindTexture(target, texture.getReadOnlyTextureId());
    gl.glUniform1i(uniform.location, unit);

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

bool GLCall::applyImageBinding(const GLProgram::Interface::Uniform &uniform,
    const GLImageBinding &binding, int unit)
{
    auto &gl = GLContext::currentContext();
    if (!gl.v4_2) {
        mMessages += MessageList::insert(mCall.id,
            MessageType::OpenGLVersionNotAvailable, "4.2");
        return false;
    }

    if (!binding.texture) {
        mMessages += MessageList::insert(binding.bindingItemId,
            MessageType::TextureNotAssigned);
        return false;
    }
    auto &texture = *binding.texture;
    mUsedItems += texture.itemId();

#if GL_VERSION_4_2
    const auto target = texture.target();
    const auto textureId = texture.getReadWriteTextureId();
    const auto format = (binding.format
            ? static_cast<GLenum>(binding.format)
            : static_cast<GLenum>(texture.format()));

    auto formatSupported = GLint();
    gl.v4_2->glGetInternalformativ(target, format, GL_SHADER_IMAGE_LOAD, 1,
        &formatSupported);
    if (formatSupported == GL_NONE) {
        mMessages += MessageList::insert(binding.bindingItemId,
            MessageType::ImageFormatNotBindable);
        return false;
    }
    gl.v4_2->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    gl.v4_2->glBindTexture(target, textureId);
    gl.v4_2->glUniform1i(uniform.location, unit);
    gl.v4_2->glBindImageTexture(static_cast<GLuint>(unit), textureId,
        binding.level, (binding.layer < 0), std::max(binding.layer, 0),
        binding.access, format);
#endif
    return true;
}

bool GLCall::applyBufferBinding(
    const GLProgram::Interface::BufferBindingPoint &bufferBindingPoint,
    const GLBufferBinding &binding, ScriptEngine &scriptEngine)
{
    const auto offset =
        scriptEngine.evaluateInt(binding.offset, mCall.id, mMessages);
    const auto rowCount =
        scriptEngine.evaluateInt(binding.rowCount, mCall.id, mMessages);

    if (!binding.buffer) {
        mMessages += MessageList::insert(binding.bindingItemId,
            MessageType::BufferNotAssigned);
        return false;
    }

    auto &buffer = *binding.buffer;
    mUsedItems += buffer.usedItems();
    mUsedItems += binding.blockItemId;

    const auto bufferSize =
        (binding.stride ? rowCount * binding.stride : buffer.size());
    if (bufferSize < bufferBindingPoint.minimumSize) {
        mMessages += MessageList::insert(binding.bindingItemId,
            MessageType::UniformComponentMismatch,
            QStringLiteral("(%1 bytes < %2 bytes)")
                .arg(bufferSize)
                .arg(bufferBindingPoint.minimumSize));
        return false;
    }
    buffer.bindIndexedRange(bufferBindingPoint.target, bufferBindingPoint.index,
        offset, bufferSize, bufferBindingPoint.readonly);
    return true;
}

bool GLCall::applyDynamicBufferBindings(const QString &bufferName,
    const GLProgram::Interface::BufferBindingPoint &bufferBindingPoint,
    const std::map<QString, GLUniformBinding> &bindings,
    ScriptEngine &scriptEngine)
{
    auto &buffer = mProgram->getDynamicUniformBuffer(bufferName,
        bufferBindingPoint.minimumSize);

    auto memberSet = false;
    for (const auto &[name, member] : bufferBindingPoint.members) {
        if (applyBufferMemberBindings(buffer, name, member, bindings,
                scriptEngine)) {
            memberSet = true;
        } else {
            mMessages +=
                MessageList::insert(mCall.id, MessageType::UniformNotSet, name);
        }
    }
    if (!memberSet && !isGlobalUniformBlockName(bufferName))
        return false;

    applyBufferBinding(bufferBindingPoint,
        GLBufferBinding{ .name = bufferName, .buffer = &buffer }, scriptEngine);
    return true;
}

bool GLCall::applyBufferMemberBindings(GLBuffer &buffer, const QString &name,
    const GLProgram::Interface::BufferMember &member,
    const std::map<QString, GLUniformBinding> &bindings,
    ScriptEngine &scriptEngine)
{
    if (const auto binding = find(bindings, name)) {
        applyBufferMemberBinding(buffer, member, *binding, -1, member.size,
            scriptEngine);
        mUsedItems += binding->bindingItemId;
        return true;
    }

    // compare array elements also by basename
    auto bindingSet = false;
    const auto baseName = getBaseName(name);
    const auto uniformIndices = getArrayIndices(name);
    for (const auto &[bindingName, binding] : bindings)
        if (getBaseName(bindingName) == baseName) {
            const auto bindingIndices = getArrayIndices(bindingName);
            const auto [offset, count] = getValuesOffsetCount(uniformIndices,
                bindingIndices, member.size);
            if (count) {
                applyBufferMemberBinding(buffer, member, binding, offset, count,
                    scriptEngine);
                mUsedItems += binding.bindingItemId;
            }
            bindingSet = true;
        }

    return bindingSet;
}

bool GLCall::applyBufferMemberBinding(GLBuffer &buffer,
    const GLProgram::Interface::BufferMember &member,
    const GLUniformBinding &binding, int offset, int count,
    ScriptEngine &scriptEngine)
{
    auto &data = buffer.getWriteableData();
    const auto itemId = binding.bindingItemId;
    auto write = [&](const auto &values) {
        using T = std::decay_t<decltype(values)>::value_type;
        const auto size = static_cast<qsizetype>(values.size() * sizeof(T));
        Q_ASSERT(member.offset + size <= data.size());
        std::memcpy(data.data() + member.offset, values.data(), size);
    };

    switch (member.dataType) {
#define ADD(TYPE, DATATYPE, COUNT)                              \
    case TYPE:                                                  \
        write(getValues<DATATYPE>(scriptEngine, binding.values, \
            COUNT * offset, COUNT * count, itemId, mMessages)); \
        break

#define ADD_MATRIX(TYPE, DATATYPE, COUNT) ADD(TYPE, DATATYPE, COUNT)

        ADD(GL_FLOAT, GLfloat, 1);
        ADD(GL_FLOAT_VEC2, GLfloat, 2);
        ADD(GL_FLOAT_VEC3, GLfloat, 3);
        ADD(GL_FLOAT_VEC4, GLfloat, 4);
        ADD(GL_DOUBLE, GLdouble, 1);
        ADD(GL_DOUBLE_VEC2, GLdouble, 2);
        ADD(GL_DOUBLE_VEC3, GLdouble, 3);
        ADD(GL_DOUBLE_VEC4, GLdouble, 4);
        ADD(GL_INT, GLint, 1);
        ADD(GL_INT_VEC2, GLint, 2);
        ADD(GL_INT_VEC3, GLint, 3);
        ADD(GL_INT_VEC4, GLint, 4);
        ADD(GL_UNSIGNED_INT, GLuint, 1);
        ADD(GL_UNSIGNED_INT_VEC2, GLuint, 2);
        ADD(GL_UNSIGNED_INT_VEC3, GLuint, 3);
        ADD(GL_UNSIGNED_INT_VEC4, GLuint, 4);
        ADD(GL_BOOL, GLint, 1);
        ADD(GL_BOOL_VEC2, GLint, 2);
        ADD(GL_BOOL_VEC3, GLint, 3);
        ADD(GL_BOOL_VEC4, GLint, 4);
        ADD_MATRIX(GL_FLOAT_MAT2, GLfloat, 4);
        ADD_MATRIX(GL_FLOAT_MAT3, GLfloat, 9);
        ADD_MATRIX(GL_FLOAT_MAT4, GLfloat, 16);
        ADD_MATRIX(GL_FLOAT_MAT2x3, GLfloat, 6);
        ADD_MATRIX(GL_FLOAT_MAT3x2, GLfloat, 6);
        ADD_MATRIX(GL_FLOAT_MAT2x4, GLfloat, 8);
        ADD_MATRIX(GL_FLOAT_MAT4x2, GLfloat, 8);
        ADD_MATRIX(GL_FLOAT_MAT3x4, GLfloat, 12);
        ADD_MATRIX(GL_FLOAT_MAT4x3, GLfloat, 12);
        ADD_MATRIX(GL_DOUBLE_MAT2, GLdouble, 4);
        ADD_MATRIX(GL_DOUBLE_MAT3, GLdouble, 9);
        ADD_MATRIX(GL_DOUBLE_MAT4, GLdouble, 16);
        ADD_MATRIX(GL_DOUBLE_MAT2x3, GLdouble, 6);
        ADD_MATRIX(GL_DOUBLE_MAT3x2, GLdouble, 6);
        ADD_MATRIX(GL_DOUBLE_MAT2x4, GLdouble, 8);
        ADD_MATRIX(GL_DOUBLE_MAT4x2, GLdouble, 8);
        ADD_MATRIX(GL_DOUBLE_MAT3x4, GLdouble, 12);
        ADD_MATRIX(GL_DOUBLE_MAT4x3, GLdouble, 12);
#undef ADD
#undef ADD_MATRIX
    }
    return true;
}

void GLCall::selectSubroutines(Shader::ShaderType stage,
    const std::vector<GLProgram::Interface::Subroutine> &subroutines,
    const std::map<QString, GLSubroutineBinding> &bindings)
{
    if (subroutines.empty())
        return;

    auto &gl = GLContext::currentContext();
    if (!gl.v4_0) {
        mMessages += MessageList::insert(mCall.id,
            MessageType::OpenGLVersionNotAvailable, "4.0");
        return;
    }

    auto subroutineIndices = std::vector<GLuint>();
    for (const auto &subroutine : subroutines) {
        const auto binding = [&]() -> const GLSubroutineBinding * {
            for (const auto &[name, binding] : bindings)
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

    gl.v4_0->glUniformSubroutinesuiv(stage,
        static_cast<GLsizei>(subroutineIndices.size()),
        subroutineIndices.data());
}

bool GLCall::bindVertexStream()
{
    if (mVertexStream)
        mUsedItems += mVertexStream->itemId();

    auto canRender = true;
    auto &gl = GLContext::currentContext();
    const auto &attributeLocations = mProgram->interface().attributeLocations;
    for (const auto &[name, location] : attributeLocations) {
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
            if (gl.v4_2)
                gl.v4_2->glVertexAttribLPointer(location, attribute.count,
                    attribute.type, attribute.stride,
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
    for (const auto &[name, location] :
        mProgram->interface().attributeLocations)
        gl.glDisableVertexAttribArray(location);
}
