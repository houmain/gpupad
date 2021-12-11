#include "RenderSession.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"
#include "scripting/ScriptEngine.h"
#include "scripting/GpupadScriptObject.h"
#include "scripting/MouseScriptObject.h"
#include "FileCache.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLTarget.h"
#include "GLStream.h"
#include "GLCall.h"
#include "GLShareSynchronizer.h"
#include <functional>
#include <deque>
#include <QStack>
#include <QOpenGLTimerQuery>
#include <type_traits>

namespace {
    struct BindingScope
    {
        std::map<QString, GLUniformBinding> uniforms;
        std::map<QString, GLSamplerBinding> samplers;
        std::map<QString, GLImageBinding> images;
        std::map<QString, GLBufferBinding> buffers;
        std::map<QString, GLSubroutineBinding> subroutines;
    };
    using BindingState = QStack<BindingScope>;
    using Command = std::function<void(BindingState&)>;

    QSet<ItemId> applyBindings(BindingState &state, GLProgram &program)
    {
        QSet<ItemId> usedItems;
        BindingScope bindings;
        for (const BindingScope &scope : state) {
            for (const auto &kv : scope.uniforms)
                bindings.uniforms[kv.first] = kv.second;
            for (const auto &kv : scope.samplers)
                bindings.samplers[kv.first] = kv.second;
            for (const auto &kv : scope.images)
                bindings.images[kv.first] = kv.second;
            for (const auto &kv : scope.buffers)
                bindings.buffers[kv.first] = kv.second;
            for (const auto &kv : scope.subroutines)
                bindings.subroutines[kv.first] = kv.second;
        }

        for (const auto &kv : bindings.uniforms)
            if (program.apply(kv.second))
                usedItems += kv.second.bindingItemId;

        auto unit = 0;
        for (const auto &kv : bindings.samplers)
            if (program.apply(kv.second, unit)) {
                ++unit;
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.texture->usedItems();
            }

        for (const auto &kv : bindings.images)
            if (program.apply(kv.second, unit)) {
                ++unit;
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.texture->usedItems();
            }

        for (const auto &kv : bindings.buffers)
            if (program.apply(kv.second)) {
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.buffer->usedItems();
            }

        program.applyPrintfBindings();

        for (const auto &kv : bindings.subroutines)
            if (program.apply(kv.second))
                usedItems += kv.second.bindingItemId;
        program.reapplySubroutines();

        return usedItems;
    }

    template<typename T, typename Item, typename... Args>
    T* addOnce(std::map<ItemId, T> &list, const Item *item, Args &&...args)
    {
        if (!item)
            return nullptr;
        auto it = list.find(item->id);
        if (it != list.end())
            return &it->second;
        return &list.emplace(std::piecewise_construct,
            std::forward_as_tuple(item->id),
            std::forward_as_tuple(*item,
                std::forward<Args>(args)...)).first->second;
    }

    template<typename T>
    void replaceEqual(std::map<ItemId, T> &to, std::map<ItemId, T> &from)
    {
        for (auto &kv : to) {
            auto it = from.find(kv.first);
            if (it != from.end()) {
                // implicitly update untitled filename of buffer
                if constexpr (std::is_same_v<T, GLBuffer>)
                    it->second.updateUntitledFilename(kv.second);

                if (kv.second == it->second)
                    kv.second = std::move(it->second);
            }
        }
    }

    QString formatQueryDuration(std::chrono::duration<double> duration)
    {
        auto toString = [](double v) { return QString::number(v, 'f', 2); };

        auto seconds = duration.count();
        if (duration > std::chrono::seconds(1))
            return toString(seconds) + "s";

        if (duration > std::chrono::milliseconds(1))
            return toString(seconds * 1000) + "ms";

        if (duration > std::chrono::microseconds(1))
            return toString(seconds * 1000000ull) + QChar(181) + "s";

        return toString(seconds * 1000000000ull) + "ns";
    }

    bool shouldExecute(Call::ExecuteOn executeOn, EvaluationType evaluationType)
    {
        switch (executeOn) {
            case Call::ExecuteOn::ResetEvaluation:
                return (evaluationType == EvaluationType::Reset);

            case Call::ExecuteOn::ManualEvaluation:
                return (evaluationType == EvaluationType::Reset ||
                        evaluationType == EvaluationType::Manual);

            case Call::ExecuteOn::EveryEvaluation:
                break;
        }
        return true;
    }
} // namespace

struct RenderSession::CommandQueue
{
    std::map<ItemId, GLTexture> textures;
    std::map<ItemId, GLBuffer> buffers;
    std::map<ItemId, GLProgram> programs;
    std::map<ItemId, GLTarget> targets;
    std::map<ItemId, GLStream> vertexStreams;
    std::deque<Command> commands;
    std::vector<GLProgram> failedPrograms;
};

RenderSession::RenderSession(QObject *parent)
    : RenderTask(parent)
{
}

RenderSession::~RenderSession()
{
    releaseResources();
}

QSet<ItemId> RenderSession::usedItems() const
{
    QMutexLocker lock{ &mUsedItemsCopyMutex };
    return mUsedItemsCopy;
}

bool RenderSession::usesInputState() const
{
    return (mMouseScriptObject && 
            mMouseScriptObject->wasRead());
}

void RenderSession::prepare(bool itemsChanged,
        EvaluationType evaluationType)
{
    mItemsChanged = itemsChanged;
    mEvaluationType = evaluationType;

    if (!mCommandQueue)
        mEvaluationType = EvaluationType::Reset;

    if (!mScriptEngine || mEvaluationType == EvaluationType::Reset) {
        mScriptEngine.reset(new ScriptEngine());
        mGpupadScriptObject = new GpupadScriptObject(this);
        mGpupadScriptObject->initialize(*mScriptEngine);

        mMouseScriptObject = new MouseScriptObject(this);
        mScriptEngine->setGlobal("Mouse", mMouseScriptObject);
    }

    Singletons::inputState().update();
    mMouseScriptObject->update(Singletons::inputState());

    const auto evaluateScript = [&](const Script &script) {
        if (shouldExecute(script.executeOn, mEvaluationType)) {
            auto source = QString();
            if (Singletons::fileCache().getSource(script.fileName, &source)) {
                mScriptEngine->evaluateScript(source, script.fileName, mMessages);

                mGpupadScriptObject->applySessionUpdate(*mScriptEngine);
                Singletons::synchronizeLogic().cancelAutomaticRevalidation();
            }
        }
    };

    const auto &session = Singletons::sessionModel();

    if (!itemsChanged && mEvaluationType != EvaluationType::Reset) {
        mScriptEngine->updateVariables(mMessages);

        session.forEachItem([&](const Item &item) {
            if (auto script = castItem<Script>(item))
                evaluateScript(*script);
        });
        return;
    }

    mPrevMessages.swap(mMessages);
    mMessages.clear();

    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue());
    mUsedItems.clear();

    const auto addCommand = [&](auto&& command) {
        mCommandQueue->commands.emplace_back(std::move(command));
    };

    const auto addProgramOnce = [&](ItemId programId) {
        return addOnce(mCommandQueue->programs,
            session.findItem<Program>(programId));
    };

    const auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(mCommandQueue->buffers,
            session.findItem<Buffer>(bufferId), *mScriptEngine);
    };

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Texture>(textureId), *mScriptEngine);
    };

    const auto addTextureBufferOnce = [&](ItemId bufferId,
            GLBuffer *buffer, Texture::Format format) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Buffer>(bufferId), buffer, format, *mScriptEngine);
    };

    const auto addTargetOnce = [&](ItemId targetId) {
        auto target = session.findItem<Target>(targetId);
        auto fb = addOnce(mCommandQueue->targets, target);
        if (fb) {
            const auto &items = target->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attachment = castItem<Attachment>(items[i]))
                    fb->setAttachment(i, addTextureOnce(attachment->textureId));
        }
        return fb;
    };

    const auto addVertexStreamOnce = [&](ItemId vertexStreamId) {
        auto vertexStream = session.findItem<Stream>(vertexStreamId);
        auto vs = addOnce(mCommandQueue->vertexStreams, vertexStream);
        if (vs) {
            const auto &items = vertexStream->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attribute = castItem<Attribute>(items[i]))
                    if (auto field = session.findItem<Field>(attribute->fieldId))
                        vs->setAttribute(i, *field, 
                            addBufferOnce(field->parent->parent->id), *mScriptEngine);
        }
        return vs;
    };

    session.forEachItem([&](const Item &item) {

        if (auto group = castItem<Group>(item)) {
            const auto iterations =
                mScriptEngine->evaluateInt(group->iterations, group->id, mMessages);

            // mark begin of iteration
            addCommand([this, groupId = group->id](BindingState &) {
                auto &iterations = mGroupIterations[groupId];
                iterations.iterationsLeft = iterations.iterations;
            });
            const auto commandQueueBeginIndex =
                static_cast<int>(mCommandQueue->commands.size());
            mGroupIterations[group->id] = { iterations, commandQueueBeginIndex, 0 };

            // push binding scope
            if (!group->inlineScope)
                addCommand([](BindingState &state) { state.push({ }); });
        }
        else if (auto script = castItem<Script>(item)) {
            mUsedItems += script->id;
            evaluateScript(*script);
        }
        else if (auto binding = castItem<Binding>(item)) {
            const auto &b = *binding;
            switch (b.bindingType) {
                case Binding::BindingType::Uniform:
                    addCommand(
                        [binding = GLUniformBinding{
                            b.id, b.name, b.bindingType,
                            mScriptEngine->getVariable(b.name, b.values, b.id, mMessages),
                            false }
                        ](BindingState &state) {
                            state.top().uniforms[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::Sampler:
                    addCommand(
                        [binding = GLSamplerBinding{
                            b.id, b.name, addTextureOnce(b.textureId),
                            b.minFilter, b.magFilter, b.anisotropic,
                            b.wrapModeX, b.wrapModeY, b.wrapModeZ,
                            b.borderColor,
                            b.comparisonFunc }
                        ](BindingState &state) {
                            state.top().samplers[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::Image:
                    addCommand(
                        [binding = GLImageBinding{
                            b.id, b.name, addTextureOnce(b.textureId),
                            b.level, b.layer, GLenum{ GL_READ_WRITE },
                            b.imageFormat }
                        ](BindingState &state) {
                            state.top().images[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::TextureBuffer: {
                    addCommand(
                        [binding = GLImageBinding{
                            b.id, b.name,
                            addTextureBufferOnce(b.bufferId, addBufferOnce(b.bufferId),
                                static_cast<Texture::Format>(b.imageFormat)),
                            b.level, b.layer, GLenum{ GL_READ_WRITE },
                            b.imageFormat }
                        ](BindingState &state) {
                            state.top().images[binding.name] = binding;
                        });
                    break;
                }

                case Binding::BindingType::Buffer:
                    addCommand(
                        [binding = GLBufferBinding{
                            b.id, b.name, addBufferOnce(b.bufferId), 0, 0, false }
                        ](BindingState &state) {
                            state.top().buffers[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::BufferBlock:
                    if (auto block = session.findItem<Block>(b.blockId)) {
                        const auto offset = mScriptEngine->evaluateInt(block->offset, b.blockId, mMessages);
                        const auto rowCount = mScriptEngine->evaluateInt(block->rowCount, b.blockId, mMessages);
                        const auto stride = getBlockStride(*block);
                        addCommand(
                            [binding = GLBufferBinding{
                                b.id, b.name, addBufferOnce(block->parent->id), offset, rowCount * stride, false }
                            ](BindingState &state) {
                                state.top().buffers[binding.name] = binding;
                            });
                    }
                    break;

                case Binding::BindingType::Subroutine:
                    addCommand(
                        [binding = GLSubroutineBinding{
                            b.id, b.name, b.subroutine, {} }
                        ](BindingState &state) {
                            state.top().subroutines[binding.name] = binding;
                        });
                    break;
                }
        }
        else if (auto call = castItem<Call>(item)) {
            if (call->checked) {
                mUsedItems += call->id;
                auto glcall = GLCall(*call, *mScriptEngine);
                switch (call->callType) {
                    case Call::CallType::Draw:
                    case Call::CallType::DrawIndexed:
                    case Call::CallType::DrawIndirect:
                    case Call::CallType::DrawIndexedIndirect:
                        glcall.setProgram(addProgramOnce(call->programId));
                        glcall.setTarget(addTargetOnce(call->targetId));
                        glcall.setVextexStream(addVertexStreamOnce(call->vertexStreamId));
                        if (auto block = session.findItem<Block>(call->indexBufferBlockId))
                            glcall.setIndexBuffer(addBufferOnce(block->parent->id), *block,
                                *mScriptEngine);
                        if (auto block = session.findItem<Block>(call->indirectBufferBlockId))
                            glcall.setIndirectBuffer(addBufferOnce(block->parent->id), *block,
                                *mScriptEngine);
                        break;

                    case Call::CallType::Compute:
                    case Call::CallType::ComputeIndirect:
                        glcall.setProgram(addProgramOnce(call->programId));
                        if (auto block = session.findItem<Block>(call->indirectBufferBlockId))
                            glcall.setIndirectBuffer(addBufferOnce(block->parent->id), *block,
                                *mScriptEngine);
                        break;

                    case Call::CallType::ClearTexture:
                    case Call::CallType::CopyTexture:
                    case Call::CallType::SwapTextures:
                        glcall.setTextures(
                            addTextureOnce(call->textureId),
                            addTextureOnce(call->fromTextureId));
                        break;

                    case Call::CallType::ClearBuffer:
                    case Call::CallType::CopyBuffer:
                    case Call::CallType::SwapBuffers:
                        glcall.setBuffers(
                            addBufferOnce(call->bufferId),
                            addBufferOnce(call->fromBufferId));
                        break;
                }

                addCommand(
                    [this,
                     executeOn = call->executeOn,
                     call = std::move(glcall)
                    ](BindingState &state) mutable {
                        if (!shouldExecute(executeOn, mEvaluationType))
                            return;

                        if (auto program = call.program()) {
                            mUsedItems += program->usedItems();
                            if (!program->bind(&mMessages))
                                return;
                            mUsedItems += applyBindings(state, *program);
                            call.execute(mMessages);
                            program->unbind(call.itemId());
                        }
                        else {
                            call.execute(mMessages);
                        }

                        if (!updatingPreviewTextures())
                            if (auto timerQuery = call.timerQuery())
                                mTimerQueries.append({ call.itemId(), timerQuery });
                        mUsedItems += call.usedItems();
                    });
            }
        }

        // pop binding scope(s) after last group item
        if (!castItem<Group>(&item)) {
            auto it = &item;
            while (it && it->parent && it->parent->items.back() == it) {
                auto group = castItem<Group>(it->parent);
                if (!group)
                    break;

                if (!group->inlineScope)
                    addCommand([](BindingState &state) {
                        state.pop();
                    });

                addCommand([this, groupId = group->id](BindingState &) {
                    // jump to begin of group
                    auto &iteration = mGroupIterations[groupId];
                    if (--iteration.iterationsLeft > 0)
                        setNextCommandQueueIndex(iteration.commandQueueBeginIndex);
                });

                // undo pushing commands, when there is not a single iteration
                const auto &iteration = mGroupIterations[group->id];
                if (!iteration.iterations)
                    mCommandQueue->commands.resize(iteration.commandQueueBeginIndex);

                it = it->parent;
            }
        }
    });
}

void RenderSession::render()
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);

    auto& gl = GLContext::currentContext();
    if (!gl) {
        mMessages += MessageList::insert(
            0, MessageType::OpenGLVersionNotAvailable, "3.3");
        return;
    }

    gl.glEnable(GL_FRAMEBUFFER_SRGB);
    gl.glEnable(GL_PROGRAM_POINT_SIZE);
    gl.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#if GL_VERSION_4_3
    if (gl.v4_3)
        gl.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif

    reuseUnmodifiedItems();
    executeCommandQueue();
    downloadModifiedResources();
    if (!updatingPreviewTextures())
        outputTimerQueries();

    gl.glFlush();
    Q_ASSERT(glGetError() == GL_NO_ERROR);
}

void RenderSession::reuseUnmodifiedItems()
{
    if (mPrevCommandQueue) {

        replaceEqual(mCommandQueue->textures, mPrevCommandQueue->textures);
        replaceEqual(mCommandQueue->buffers, mPrevCommandQueue->buffers);
        replaceEqual(mCommandQueue->programs, mPrevCommandQueue->programs);

        // immediately try to link programs
        // when failing restore previous version but keep error messages
        for (auto &[id, program] : mCommandQueue->programs) {
            auto it = mPrevCommandQueue->programs.find(id);
            if (it != mPrevCommandQueue->programs.end()) {
                auto &prev = it->second;
                if (!program.link() && prev.link()) {
                    mCommandQueue->failedPrograms.push_back(std::move(program));
                    program = std::move(prev);
                }
            }
        }

        mPrevCommandQueue.reset();
    }
}

void RenderSession::setNextCommandQueueIndex(int index)
{
    mNextCommandQueueIndex = index;
}

void RenderSession::executeCommandQueue()
{
    auto& context = GLContext::currentContext();
    Singletons::glShareSynchronizer().beginUpdate(context);

    BindingState state;

    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex < static_cast<int>(mCommandQueue->commands.size())) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        mCommandQueue->commands[index](state);
    }

    Singletons::glShareSynchronizer().endUpdate(context);
}

bool RenderSession::updatingPreviewTextures() const
{
    return (!mItemsChanged &&
        mEvaluationType == EvaluationType::Steady);
}

void RenderSession::downloadModifiedResources()
{
    for (auto &[itemId, texture] : mCommandQueue->textures) {
        texture.updateMipmaps();
        if (!updatingPreviewTextures() &&
            !texture.fileName().isEmpty() &&
            texture.download())
            mModifiedTextures[texture.itemId()] = texture.data();
    }

    for (auto &[itemId, buffer] : mCommandQueue->buffers)
        if (!buffer.fileName().isEmpty() &&
            (mItemsChanged || mEvaluationType != EvaluationType::Steady) &&
            buffer.download())
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

void RenderSession::outputTimerQueries()
{
    mTimerMessages.clear();
    for (const auto &[itemId, query] : qAsConst(mTimerQueries)) {
        const auto duration = std::chrono::nanoseconds(query->waitForResult());
        mTimerMessages += MessageList::insert(
            itemId, MessageType::CallDuration,
            formatQueryDuration(duration), false);
    }
    mTimerQueries.clear();
}

void RenderSession::finish()
{
    auto &editors = Singletons::editorManager();
    auto &session = Singletons::sessionModel();

    editors.setAutoRaise(false);

    for (auto itemId : mModifiedTextures.keys())
        if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
            if (auto editor = editors.openTextureEditor(fileItem->fileName))
                editor->replace(mModifiedTextures[itemId], false);
    mModifiedTextures.clear();

    for (auto itemId : mModifiedBuffers.keys())
        if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
            if (auto editor = editors.openBinaryEditor(fileItem->fileName))
                editor->replace(mModifiedBuffers[itemId], false);
    mModifiedBuffers.clear();

    editors.setAutoRaise(true);

    if (updatingPreviewTextures())
        for (const auto& [itemId, texture] : mCommandQueue->textures)
            if (texture.deviceCopyModified())
                if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
                    if (auto editor = editors.getTextureEditor(fileItem->fileName))
                        if (auto textureId = texture.textureId())
                            editor->updatePreviewTexture(texture.target(), textureId);

    mPrevMessages.clear();

    QMutexLocker lock{ &mUsedItemsCopyMutex };
    mUsedItemsCopy = mUsedItems;
}

void RenderSession::release()
{
    mVao.destroy();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
    mTimerQueries.clear();
}
