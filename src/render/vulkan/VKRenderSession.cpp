#include "VKRenderSession.h"
#include "Settings.h"
#include "Singletons.h"
#include "VKShareSync.h"
#include "VKBuffer.h"
#include "VKCall.h"
#include "VKProgram.h"
#include "VKRenderer.h"
#include "VKStream.h"
#include "VKTarget.h"
#include "VKTexture.h"
#include "editors/EditorManager.h"
#include "editors/texture/TextureEditor.h"
#include "scripting/ScriptEngine.h"
#include "scripting/ScriptSession.h"
#include <QStack>
#include <deque>
#include <functional>
#include <type_traits>

namespace {
    using BindingState = QStack<VKBindings>;
    using Command = std::function<void(BindingState &)>;

    VKBindings mergeBindingState(const BindingState &state)
    {
        auto merged = VKBindings{};
        for (const VKBindings &scope : state) {
            for (const auto &[name, binding] : scope.uniforms)
                merged.uniforms[name] = binding;
            for (const auto &[name, binding] : scope.samplers)
                merged.samplers[name] = binding;
            for (const auto &[name, binding] : scope.images)
                merged.images[name] = binding;
            for (const auto &[name, binding] : scope.buffers)
                merged.buffers[name] = binding;
        }
        return merged;
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
                if constexpr (std::is_same_v<T, VKBuffer>)
                    it->second.updateUntitledFilename(kv.second);

                if (kv.second == it->second)
                    kv.second = std::move(it->second);
            }
        }
    }
} // namespace

struct VKRenderSession::CommandQueue
{
    VKContext context;

    std::map<ItemId, VKTexture> textures;
    std::map<ItemId, VKBuffer> buffers;
    std::map<ItemId, VKProgram> programs;
    std::map<ItemId, VKTarget> targets;
    std::map<ItemId, VKStream> vertexStreams;
    std::deque<Command> commands;
    std::vector<VKProgram> failedPrograms;
};

VKRenderSession::VKRenderSession(RendererPtr renderer)
    : RenderSessionBase(std::move(renderer))
{
}

VKRenderSession::~VKRenderSession()
{
    releaseResources();
}

VKRenderer &VKRenderSession::renderer()
{
    return static_cast<VKRenderer &>(RenderSessionBase::renderer());
}

void VKRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue{
        .context =
            VKContext{
                .device = renderer().device(),
                .queue = renderer().device().queues()[0],
                .ktxDeviceInfo = renderer().ktxDeviceInfo(),
            },
    });
    mUsedItems.clear();

    auto &scriptEngine = mScriptSession->engine();
    const auto &session = mSessionCopy;

    const auto addCommand = [&](auto &&command) {
        mCommandQueue->commands.emplace_back(std::move(command));
    };

    const auto addProgramOnce = [&](ItemId programId) {
        return addOnce(mCommandQueue->programs,
            session.findItem<Program>(programId), session.sessionItem());
    };

    const auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(mCommandQueue->buffers,
            session.findItem<Buffer>(bufferId), scriptEngine);
    };

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Texture>(textureId), scriptEngine);
    };

    const auto addTextureBufferOnce = [&](ItemId bufferId, VKBuffer *buffer,
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
                    fb->setTexture(i, addTextureOnce(attachment->textureId));
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
            // mark begin of iteration
            addCommand([this, groupId = group->id](BindingState &) {
                auto &iteration = mGroupIterations[groupId];
                iteration.iterationsLeft = iteration.iterations;
            });
            auto &iteration = mGroupIterations[group->id];
            iteration.commandQueueBeginIndex = mCommandQueue->commands.size();
            const auto max_iterations = 1000;
            iteration.iterations = std::min(max_iterations,
                scriptEngine.evaluateInt(group->iterations, group->id,
                    mMessages));

            // push binding scope
            if (!group->inlineScope)
                addCommand([](BindingState &state) { state.push({}); });
        } else if (castItem<ScopeItem>(item)) {
            // push binding scope
            addCommand([](BindingState &state) { state.push({}); });
        } else if (auto script = castItem<Script>(item)) {
            mUsedItems += script->id;
        } else if (auto binding = castItem<Binding>(item)) {
            const auto &b = *binding;
            switch (b.bindingType) {
            case Binding::BindingType::Uniform:
                addCommand(
                    [binding = VKUniformBinding{ b.id, b.name, b.bindingType,
                         b.values, false }](BindingState &state) {
                        state.top().uniforms[binding.name] = binding;
                    });
                break;

            case Binding::BindingType::Sampler: {
                auto texture = addTextureOnce(b.textureId);
                if (texture)
                    texture->addUsage(KDGpu::TextureUsageFlagBits::SampledBit);
                addCommand([binding = VKSamplerBinding{ b.id, b.name,
                                std::move(texture), b.minFilter, b.magFilter,
                                b.anisotropic, b.wrapModeX, b.wrapModeY,
                                b.wrapModeZ, b.borderColor,
                                b.comparisonFunc }](BindingState &state) {
                    state.top().samplers[binding.name] = binding;
                });
                break;
            }
            case Binding::BindingType::Image: {
                auto texture = addTextureOnce(b.textureId);
                if (texture)
                    texture->addUsage(KDGpu::TextureUsageFlagBits::StorageBit);
                addCommand([binding = VKImageBinding{ b.id, b.name,
                                std::move(texture), b.level, b.layer,
                                b.imageFormat }](BindingState &state) {
                    state.top().images[binding.name] = binding;
                });
                break;
            }

            case Binding::BindingType::TextureBuffer: {
                addCommand(
                    [binding = VKImageBinding{ b.id, b.name,
                         addTextureBufferOnce(b.bufferId,
                             addBufferOnce(b.bufferId),
                             static_cast<Texture::Format>(b.imageFormat)),
                         b.level, b.layer,
                         b.imageFormat }](BindingState &state) {
                        state.top().images[binding.name] = binding;
                    });
                break;
            }

            case Binding::BindingType::Buffer:
                addCommand([binding = VKBufferBinding{ b.id, b.name,
                                addBufferOnce(b.bufferId), {}, {}, 0 }](BindingState &state) {
                    state.top().buffers[binding.name] = binding;
                });
                break;

            case Binding::BindingType::BufferBlock:
                if (auto block = session.findItem<Block>(b.blockId))
                    addCommand(
                        [binding = VKBufferBinding{ b.id, b.name,
                             addBufferOnce(block->parent->id), block->offset,
                             block->rowCount, getBlockStride(*block) }](BindingState &state) {
                            state.top().buffers[binding.name] = binding;
                        });
                break;

            case Binding::BindingType::Subroutine:
                mMessages += MessageList::insert(b.id,
                    MessageType::SubroutinesNotAvailableInVulkan);
                break;
            }
        } else if (auto call = castItem<Call>(item)) {
            if (call->checked) {
                mUsedItems += call->id;
                auto pvkcall = std::make_shared<VKCall>(*call);
                auto &vkcall = *pvkcall;
                switch (call->callType) {
                case Call::CallType::Draw:
                case Call::CallType::DrawIndexed:
                case Call::CallType::DrawIndirect:
                case Call::CallType::DrawIndexedIndirect:
                    vkcall.setProgram(addProgramOnce(call->programId));
                    vkcall.setTarget(addTargetOnce(call->targetId));
                    vkcall.setVextexStream(
                        addVertexStreamOnce(call->vertexStreamId));
                    if (auto block =
                            session.findItem<Block>(call->indexBufferBlockId))
                        vkcall.setIndexBuffer(addBufferOnce(block->parent->id),
                            *block);
                    if (auto block = session.findItem<Block>(
                            call->indirectBufferBlockId))
                        vkcall.setIndirectBuffer(
                            addBufferOnce(block->parent->id), *block);
                    break;

                case Call::CallType::Compute:
                case Call::CallType::ComputeIndirect:
                    vkcall.setProgram(addProgramOnce(call->programId));
                    if (auto block = session.findItem<Block>(
                            call->indirectBufferBlockId))
                        vkcall.setIndirectBuffer(
                            addBufferOnce(block->parent->id), *block);
                    break;

                case Call::CallType::ClearTexture:
                case Call::CallType::CopyTexture:
                case Call::CallType::SwapTextures:
                    vkcall.setTextures(addTextureOnce(call->textureId),
                        addTextureOnce(call->fromTextureId));
                    break;

                case Call::CallType::ClearBuffer:
                case Call::CallType::CopyBuffer:
                case Call::CallType::SwapBuffers:
                    vkcall.setBuffers(addBufferOnce(call->bufferId),
                        addBufferOnce(call->fromBufferId));
                    break;
                }
                addCommand(
                    [this, executeOn = call->executeOn,
                        pcall = std::move(pvkcall)](BindingState &state) {
                        auto &call = *pcall;
                        if (!shouldExecute(executeOn, mEvaluationType))
                            return;

                        if (auto pipeline =
                                call.getPipeline(mCommandQueue->context))
                            pipeline->setBindings(mergeBindingState(state));

                        call.execute(mCommandQueue->context, mMessages,
                            mScriptSession->engine());

                        mUsedItems += call.usedItems();
                    });
            }
        }

        // pop binding scope(s) after scopes's last item
        if (!castItem<ScopeItem>(&item)) {
            for (auto it = &item; it && castItem<ScopeItem>(it->parent)
                 && it->parent->items.back() == it;
                 it = it->parent)
                if (auto group = castItem<Group>(it->parent)) {
                    if (!group->inlineScope)
                        addCommand([](BindingState &state) { state.pop(); });

                    // jump to begin of group
                    addCommand([this, groupId = group->id](BindingState &) {
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
                } else {
                    addCommand([](BindingState &state) { state.pop(); });
                }
        }
    });
}

void VKRenderSession::render()
{
    if (!mShareSync)
        mShareSync = std::make_shared<VKShareSync>(renderer().device());

    if (mItemsChanged || mEvaluationType == EvaluationType::Reset)
        createCommandQueue();

    reuseUnmodifiedItems();
    executeCommandQueue();
    downloadModifiedResources();
    if (!updatingPreviewTextures())
        outputTimerQueries();
}

void VKRenderSession::reuseUnmodifiedItems()
{
    if (mPrevCommandQueue) {
        replaceEqual(mCommandQueue->textures, mPrevCommandQueue->textures);
        replaceEqual(mCommandQueue->buffers, mPrevCommandQueue->buffers);
        replaceEqual(mCommandQueue->programs, mPrevCommandQueue->programs);

        // immediately try to link programs
        // when failing restore previous version but keep error messages
        auto &device = mCommandQueue->context.device;
        for (auto &[id, program] : mCommandQueue->programs) {
            auto it = mPrevCommandQueue->programs.find(id);
            if (it != mPrevCommandQueue->programs.end()) {
                auto &prev = it->second;
                if (!shaderSessionSettingsDiffer(prev.session(), program.session())
                    && !program.link(device)
                    && prev.link(device)) {
                    mCommandQueue->failedPrograms.push_back(std::move(program));
                    program = std::move(prev);
                }
            }
        }
        mPrevCommandQueue.reset();
    }
}

void VKRenderSession::executeCommandQueue()
{
    mShareSync->beginUpdate();

    auto state = BindingState{};
    mCommandQueue->context.timestampQueries.clear();

    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex < mCommandQueue->commands.size()) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        mCommandQueue->commands[index](state);
    }

    auto submitOptions = KDGpu::SubmitOptions{
        .commandBuffers = std::vector<KDGpu::Handle<KDGpu::CommandBuffer_t>>(
            mCommandQueue->context.commandBuffers.begin(),
            mCommandQueue->context.commandBuffers.end()),
        .signalSemaphores = { mShareSync->usageSemaphore() },
    };
    if (auto& semaphore = mShareSync->usageSemaphore(); semaphore.isValid())
        submitOptions.waitSemaphores.push_back(semaphore);

    mCommandQueue->context.queue.submit(submitOptions);
    mCommandQueue->context.queue.waitUntilIdle();
    mCommandQueue->context.commandBuffers.clear();

    mShareSync->endUpdate();
}

void VKRenderSession::downloadModifiedResources()
{
    for (auto &[itemId, program] : mCommandQueue->programs)
        if (program.printf().isUsed())
            mMessages += program.printf().formatMessages(mCommandQueue->context,
                program.itemId());

    for (auto &[itemId, texture] : mCommandQueue->textures)
        if (!texture.fileName().isEmpty()) {
            texture.updateMipmaps(mCommandQueue->context);
            if (!updatingPreviewTextures()
                && texture.download(mCommandQueue->context))
                mModifiedTextures[texture.itemId()] = texture.data();
        }

    for (auto &[itemId, buffer] : mCommandQueue->buffers)
        if (!buffer.fileName().isEmpty()
            && (mItemsChanged || mEvaluationType != EvaluationType::Steady)
            && buffer.download(mCommandQueue->context,
                mEvaluationType != EvaluationType::Reset))
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

void VKRenderSession::outputTimerQueries()
{
    mTimerMessages.clear();

    auto total = std::chrono::nanoseconds::zero();
    auto &queries = mCommandQueue->context.timestampQueries;
    for (auto &[itemId, query] : queries) {
        const auto duration = std::chrono::nanoseconds(query.nsInterval(0, 1));
        mTimerMessages += MessageList::insert(itemId, MessageType::CallDuration,
            formatDuration(duration), false);
        total += duration;
    }
    if (queries.size() > 1)
        mTimerMessages += MessageList::insert(0, MessageType::TotalDuration,
            formatDuration(total), false);
}

void VKRenderSession::finish()
{
    RenderSessionBase::finish();

    if (!mCommandQueue)
        return;

    auto &editors = Singletons::editorManager();
    auto &session = Singletons::sessionModel();

    if (updatingPreviewTextures())
        for (const auto &[itemId, texture] : mCommandQueue->textures)
            if (texture.deviceCopyModified())
                if (auto fileItem =
                        castItem<FileItem>(session.findItem(itemId)))
                    if (auto editor =
                            editors.getTextureEditor(fileItem->fileName))
                        if (auto handle = texture.getSharedMemoryHandle();
                            handle.handle)
                            editor->updatePreviewTexture(mShareSync, handle,
                                texture.samples());
}

void VKRenderSession::release()
{
    mShareSync->cleanup();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
}
