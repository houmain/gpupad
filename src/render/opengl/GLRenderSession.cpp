#include "GLRenderSession.h"
#include "GLBuffer.h"
#include "GLCall.h"
#include "GLProgram.h"
#include "GLShareSynchronizer.h"
#include "GLStream.h"
#include "GLTarget.h"
#include "GLTexture.h"
#include "Settings.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "editors/texture/TextureEditor.h"
#include "scripting/ScriptSession.h"
#include <QOpenGLTimerQuery>
#include <QStack>
#include <deque>
#include <functional>
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
    using Command = std::function<void(BindingState &)>;

    QSet<ItemId> applyBindings(BindingState &state, GLProgram &program,
        ScriptEngine &scriptEngine)
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
            if (program.apply(kv.second, scriptEngine))
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
            if (program.apply(kv.second, scriptEngine)) {
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

    template <typename T, typename Item, typename... Args>
    T *addOnce(std::map<ItemId, T> &list, const Item *item, Args &&...args)
    {
        if (!item)
            return nullptr;
        auto it = list.find(item->id);
        if (it != list.end())
            return &it->second;
        return &list.emplace(std::piecewise_construct,
                        std::forward_as_tuple(item->id),
                        std::forward_as_tuple(*item,
                            std::forward<Args>(args)...))
                    .first->second;
    }

    template <typename T>
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
} // namespace

struct GLRenderSession::GroupIteration
{
    int iterations;
    int commandQueueBeginIndex;
    int iterationsLeft;
};

struct GLRenderSession::CommandQueue
{
    std::vector<std::pair<ItemId, TimerQueryPtr>> timerQueries;

    std::map<ItemId, GLTexture> textures;
    std::map<ItemId, GLBuffer> buffers;
    std::map<ItemId, GLProgram> programs;
    std::map<ItemId, GLTarget> targets;
    std::map<ItemId, GLStream> vertexStreams;
    std::deque<Command> commands;
    std::vector<GLProgram> failedPrograms;
};

GLRenderSession::GLRenderSession() = default;
GLRenderSession::~GLRenderSession() = default;

void GLRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue());
    mUsedItems.clear();

    auto &scriptEngine = mScriptSession->engine();
    const auto &session = mSessionCopy;

    const auto addCommand = [&](auto &&command) {
        mCommandQueue->commands.emplace_back(std::move(command));
    };

    const auto addProgramOnce = [&](ItemId programId) {
        return addOnce(mCommandQueue->programs,
            session.findItem<Program>(programId), mShaderPreamble,
            mShaderIncludePaths);
    };

    const auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(mCommandQueue->buffers,
            session.findItem<Buffer>(bufferId), scriptEngine);
    };

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Texture>(textureId), scriptEngine);
    };

    const auto addTextureBufferOnce = [&](ItemId bufferId, GLBuffer *buffer,
                                          Texture::Format format) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Buffer>(bufferId), buffer, format, scriptEngine);
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
                    if (auto field =
                            session.findItem<Field>(attribute->fieldId))
                        vs->setAttribute(i, *field,
                            addBufferOnce(field->parent->parent->id),
                            scriptEngine);
        }
        return vs;
    };

    session.forEachItem([&](const Item &item) {
        if (auto group = castItem<Group>(item)) {
            const auto iterations = scriptEngine.evaluateInt(group->iterations,
                group->id, mMessages);

            // mark begin of iteration
            addCommand([this, groupId = group->id](BindingState &) {
                auto &iterations = mGroupIterations[groupId];
                iterations.iterationsLeft = iterations.iterations;
            });
            const auto commandQueueBeginIndex =
                static_cast<int>(mCommandQueue->commands.size());
            mGroupIterations[group->id] = { iterations, commandQueueBeginIndex,
                0 };

            // push binding scope
            if (!group->inlineScope)
                addCommand([](BindingState &state) { state.push({}); });
        } else if (auto script = castItem<Script>(item)) {
            mUsedItems += script->id;
        } else if (auto binding = castItem<Binding>(item)) {
            const auto &b = *binding;
            switch (b.bindingType) {
            case Binding::BindingType::Uniform:
                addCommand(
                    [binding = GLUniformBinding{ b.id, b.name, b.bindingType,
                         b.values, false }](BindingState &state) {
                        state.top().uniforms[binding.name] = binding;
                    });
                break;

            case Binding::BindingType::Sampler:
                addCommand([binding = GLSamplerBinding{ b.id, b.name,
                                addTextureOnce(b.textureId), b.minFilter,
                                b.magFilter, b.anisotropic, b.wrapModeX,
                                b.wrapModeY, b.wrapModeZ, b.borderColor,
                                b.comparisonFunc }](BindingState &state) {
                    state.top().samplers[binding.name] = binding;
                });
                break;

            case Binding::BindingType::Image:
                addCommand([binding = GLImageBinding{ b.id, b.name,
                                addTextureOnce(b.textureId), b.level, b.layer,
                                GLenum{ GL_READ_WRITE },
                                b.imageFormat }](BindingState &state) {
                    state.top().images[binding.name] = binding;
                });
                break;

            case Binding::BindingType::TextureBuffer: {
                addCommand(
                    [binding = GLImageBinding{ b.id, b.name,
                         addTextureBufferOnce(b.bufferId,
                             addBufferOnce(b.bufferId),
                             static_cast<Texture::Format>(b.imageFormat)),
                         b.level, b.layer, GLenum{ GL_READ_WRITE },
                         b.imageFormat }](BindingState &state) {
                        state.top().images[binding.name] = binding;
                    });
                break;
            }

            case Binding::BindingType::Buffer:
                addCommand([binding = GLBufferBinding{ b.id, b.name,
                                addBufferOnce(b.bufferId), {}, {}, 0,
                                false }](BindingState &state) {
                    state.top().buffers[binding.name] = binding;
                });
                break;

            case Binding::BindingType::BufferBlock:
                if (auto block = session.findItem<Block>(b.blockId))
                    addCommand(
                        [binding = GLBufferBinding{ b.id, b.name,
                             addBufferOnce(block->parent->id), block->offset,
                             block->rowCount, getBlockStride(*block),
                             false }](BindingState &state) {
                            state.top().buffers[binding.name] = binding;
                        });
                break;

            case Binding::BindingType::Subroutine:
                addCommand([binding = GLSubroutineBinding{ b.id, b.name,
                                b.subroutine, {} }](BindingState &state) {
                    state.top().subroutines[binding.name] = binding;
                });
                break;
            }
        } else if (auto call = castItem<Call>(item)) {
            if (call->checked) {
                mUsedItems += call->id;
                auto glcall = GLCall(*call);
                switch (call->callType) {
                case Call::CallType::Draw:
                case Call::CallType::DrawIndexed:
                case Call::CallType::DrawIndirect:
                case Call::CallType::DrawIndexedIndirect:
                    glcall.setProgram(addProgramOnce(call->programId));
                    glcall.setTarget(addTargetOnce(call->targetId));
                    glcall.setVextexStream(
                        addVertexStreamOnce(call->vertexStreamId));
                    if (auto block =
                            session.findItem<Block>(call->indexBufferBlockId))
                        glcall.setIndexBuffer(addBufferOnce(block->parent->id),
                            *block);
                    if (auto block = session.findItem<Block>(
                            call->indirectBufferBlockId))
                        glcall.setIndirectBuffer(
                            addBufferOnce(block->parent->id), *block);
                    break;

                case Call::CallType::Compute:
                case Call::CallType::ComputeIndirect:
                    glcall.setProgram(addProgramOnce(call->programId));
                    if (auto block = session.findItem<Block>(
                            call->indirectBufferBlockId))
                        glcall.setIndirectBuffer(
                            addBufferOnce(block->parent->id), *block);
                    break;

                case Call::CallType::ClearTexture:
                case Call::CallType::CopyTexture:
                case Call::CallType::SwapTextures:
                    glcall.setTextures(addTextureOnce(call->textureId),
                        addTextureOnce(call->fromTextureId));
                    break;

                case Call::CallType::ClearBuffer:
                case Call::CallType::CopyBuffer:
                case Call::CallType::SwapBuffers:
                    glcall.setBuffers(addBufferOnce(call->bufferId),
                        addBufferOnce(call->fromBufferId));
                    break;
                }

                addCommand(
                    [this, executeOn = call->executeOn,
                        call = std::move(glcall)](BindingState &state) mutable {
                        if (!shouldExecute(executeOn, mEvaluationType))
                            return;

                        if (auto program = call.program()) {
                            mUsedItems += program->usedItems();
                            if (!program->bind(&mMessages))
                                return;
                            mUsedItems += applyBindings(state, *program,
                                mScriptSession->engine());
                            call.execute(mMessages, mScriptSession->engine());
                            program->unbind(call.itemId());
                        } else {
                            call.execute(mMessages, mScriptSession->engine());
                        }

                        if (!updatingPreviewTextures())
                            if (auto timerQuery = call.timerQuery())
                                mCommandQueue->timerQueries.emplace_back(
                                    call.itemId(), timerQuery);
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
                    addCommand([](BindingState &state) { state.pop(); });

                addCommand([this, groupId = group->id](BindingState &) {
                    // jump to begin of group
                    auto &iteration = mGroupIterations[groupId];
                    if (--iteration.iterationsLeft > 0)
                        setNextCommandQueueIndex(
                            iteration.commandQueueBeginIndex);
                });

                // undo pushing commands, when there is not a single iteration
                const auto &iteration = mGroupIterations[group->id];
                if (!iteration.iterations)
                    mCommandQueue->commands.resize(
                        iteration.commandQueueBeginIndex);

                it = it->parent;
            }
        }
    });
}

void GLRenderSession::render()
{
    if (mItemsChanged || mEvaluationType == EvaluationType::Reset)
        createCommandQueue();

    auto &gl = GLContext::currentContext();
    if (!gl) {
        mMessages += MessageList::insert(0,
            MessageType::OpenGLVersionNotAvailable, "3.3");
        return;
    }

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);

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

void GLRenderSession::reuseUnmodifiedItems()
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

void GLRenderSession::setNextCommandQueueIndex(int index)
{
    mNextCommandQueueIndex = index;
}

void GLRenderSession::executeCommandQueue()
{
    auto &context = GLContext::currentContext();
    Singletons::glShareSynchronizer().beginUpdate(context);

    BindingState state;
    mCommandQueue->timerQueries.clear();

    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex
        < static_cast<int>(mCommandQueue->commands.size())) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        mCommandQueue->commands[index](state);
    }

    Singletons::glShareSynchronizer().endUpdate(context);
}

void GLRenderSession::downloadModifiedResources()
{
    for (auto &[itemId, texture] : mCommandQueue->textures)
        if (!texture.fileName().isEmpty()) {
            texture.updateMipmaps();
            if (!updatingPreviewTextures() && texture.download())
                mModifiedTextures[texture.itemId()] = texture.data();
        }

    for (auto &[itemId, buffer] : mCommandQueue->buffers)
        if (!buffer.fileName().isEmpty()
            && (mItemsChanged || mEvaluationType != EvaluationType::Steady)
            && buffer.download(mEvaluationType != EvaluationType::Reset))
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

void GLRenderSession::outputTimerQueries()
{
    mTimerMessages.clear();

    auto total = std::chrono::nanoseconds::zero();
    auto &queries = mCommandQueue->timerQueries;
    for (const auto &[itemId, query] : queries) {
        const auto duration = std::chrono::nanoseconds(query->waitForResult());
        mTimerMessages += MessageList::insert(itemId, MessageType::CallDuration,
            formatDuration(duration), false);
        total += duration;
    }
    if (queries.size() > 1)
        mTimerMessages += MessageList::insert(0, MessageType::TotalDuration,
            formatDuration(total), false);
}

void GLRenderSession::finish()
{
    RenderSessionBase::finish();

    auto &editors = Singletons::editorManager();
    auto &session = Singletons::sessionModel();

    if (updatingPreviewTextures())
        for (const auto &[itemId, texture] : mCommandQueue->textures)
            if (texture.deviceCopyModified())
                if (auto fileItem =
                        castItem<FileItem>(session.findItem(itemId)))
                    if (auto editor =
                            editors.getTextureEditor(fileItem->fileName))
                        if (auto textureId = texture.textureId())
                            editor->updatePreviewTexture(textureId,
                                texture.samples());
}

void GLRenderSession::release()
{
    mVao.destroy();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
}
