#include "VKRenderSession.h"
#include "Singletons.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "editors/EditorManager.h"
#include "editors/texture/TextureEditor.h"
#include "scripting/ScriptEngine.h"
#include "scripting/ScriptSession.h"
#include "VKRenderer.h"
#include "VKTexture.h"
#include "VKBuffer.h"
#include "VKProgram.h"
#include "VKTarget.h"
#include "VKStream.h"
#include "VKCall.h"
#include <functional>
#include <deque>
#include <QStack>
#include <type_traits>

namespace {
    struct BindingScope
    {
        std::map<QString, VKUniformBinding> uniforms;
        std::map<QString, VKSamplerBinding> samplers;
        std::map<QString, VKImageBinding> images;
        std::map<QString, VKBufferBinding> buffers;
        //std::map<QString, VKSubroutineBinding> subroutines;
    };
    using BindingState = QStack<BindingScope>;
    using Command = std::function<void(BindingState&)>;

    QSet<ItemId> applyBindings(BindingState &state, VKPipeline &pipeline)
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
            //for (const auto &kv : scope.subroutines)
            //    bindings.subroutines[kv.first] = kv.second;
        }

        pipeline.clearBindings();

        for (const auto &kv : bindings.uniforms)
            if (pipeline.apply(kv.second))
                usedItems += kv.second.bindingItemId;
        
        for (const auto &kv : bindings.samplers)
            if (pipeline.apply(kv.second)) {
                usedItems += kv.second.bindingItemId;
                if (kv.second.texture)
                    usedItems += kv.second.texture->usedItems();
            }
        
        for (const auto &kv : bindings.images)
            if (pipeline.apply(kv.second)) {
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.texture->usedItems();
            }
        
        for (const auto &kv : bindings.buffers)
            if (pipeline.apply(kv.second)) {
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.buffer->usedItems();
            }

        //for (const auto &kv : bindings.subroutines)
        //    if (pipeline.apply(kv.second))
        //        usedItems += kv.second.bindingItemId;
        //call.reapplySubroutines();

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
                if constexpr (std::is_same_v<T, VKBuffer>)
                    it->second.updateUntitledFilename(kv.second);

                if (kv.second == it->second)
                    kv.second = std::move(it->second);
            }
        }
    }
} // namespace

struct VKRenderSession::GroupIteration
{
    int iterations;
    int commandQueueBeginIndex;
    int iterationsLeft;
};

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

VKRenderSession::VKRenderSession(VKRenderer &renderer)
    : mRenderer(renderer)
{
}

VKRenderSession::~VKRenderSession() = default;

void VKRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue{ 
        .context = VKContext{
            .device = mRenderer.device(), 
            .queue = mRenderer.device().queues()[0],
            .ktxDeviceInfo = mRenderer.ktxDeviceInfo(),
        }
    });
    mUsedItems.clear();
    
    auto &scriptEngine = mScriptSession->engine();
    const auto &session = mSessionCopy;

    const auto addCommand = [&](auto&& command) {
        mCommandQueue->commands.emplace_back(std::move(command));
    };

    const auto addProgramOnce = [&](ItemId programId) {
        return addOnce(mCommandQueue->programs,
            session.findItem<Program>(programId), mShaderPreamble, mShaderIncludePaths);
    };

    const auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(mCommandQueue->buffers,
            session.findItem<Buffer>(bufferId), scriptEngine);
    };

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Texture>(textureId), scriptEngine);
    };

    const auto addTextureBufferOnce = [&](ItemId bufferId,
            VKBuffer *buffer, Texture::Format format) {
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
                    if (auto field = session.findItem<Field>(attribute->fieldId))
                        vs->setAttribute(i, *field,
                            addBufferOnce(field->parent->parent->id), scriptEngine);
        }
        return vs;
    };

    session.forEachItem([&](const Item &item) {
        if (auto group = castItem<Group>(item)) {
            const auto iterations =
                scriptEngine.evaluateInt(group->iterations, group->id, mMessages);

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
        }
        else if (auto binding = castItem<Binding>(item)) {
            const auto &b = *binding;
            switch (b.bindingType) {
                case Binding::BindingType::Uniform:
                    addCommand(
                        [binding = VKUniformBinding{
                            b.id, b.name, b.bindingType, b.values, false }
                        ](BindingState &state) {
                            state.top().uniforms[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::Sampler: {
                    auto texture = addTextureOnce(b.textureId);
                    if (texture)
                        texture->addUsage(KDGpu::TextureUsageFlagBits::SampledBit);
                    addCommand(
                        [binding = VKSamplerBinding{
                            b.id, b.name, std::move(texture),
                            b.minFilter, b.magFilter, b.anisotropic,
                            b.wrapModeX, b.wrapModeY, b.wrapModeZ,
                            b.borderColor,
                            b.comparisonFunc }
                        ](BindingState &state) {
                            state.top().samplers[binding.name] = binding;
                        });
                    break;
                }
                case Binding::BindingType::Image: {
                    auto texture = addTextureOnce(b.textureId);
                    if (texture)
                        texture->addUsage(KDGpu::TextureUsageFlagBits::StorageBit);
                    addCommand(
                        [binding = VKImageBinding{
                            b.id, b.name, std::move(texture),
                            b.level, b.layer,
                            b.imageFormat }
                        ](BindingState &state) {
                            state.top().images[binding.name] = binding;
                        });
                    break;
                }

                case Binding::BindingType::TextureBuffer: {
                    addCommand(
                        [binding = VKImageBinding{
                            b.id, b.name,
                            addTextureBufferOnce(b.bufferId, addBufferOnce(b.bufferId),
                                static_cast<Texture::Format>(b.imageFormat)),
                            b.level, b.layer,
                            b.imageFormat }
                        ](BindingState &state) {
                            state.top().images[binding.name] = binding;
                        });
                    break;
                }

                case Binding::BindingType::Buffer:
                    addCommand(
                        [binding = VKBufferBinding{
                            b.id, b.name, addBufferOnce(b.bufferId), { }, { }, 0, false }
                        ](BindingState &state) {
                            state.top().buffers[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::BufferBlock:
                    if (auto block = session.findItem<Block>(b.blockId))
                        addCommand(
                            [binding = VKBufferBinding{
                                b.id, b.name, addBufferOnce(block->parent->id),
                                block->offset, block->rowCount, getBlockStride(*block), false }
                            ](BindingState &state) {
                                state.top().buffers[binding.name] = binding;
                            });
                    break;

                //case Binding::BindingType::Subroutine:
                //    addCommand(
                //        [binding = VKSubroutineBinding{
                //            b.id, b.name, b.subroutine, {} }
                //        ](BindingState &state) {
                //            state.top().subroutines[binding.name] = binding;
                //        });
                //    break;
                }
        }
        else if (auto call = castItem<Call>(item)) {
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
                        vkcall.setVextexStream(addVertexStreamOnce(call->vertexStreamId));
                        if (auto block = session.findItem<Block>(call->indexBufferBlockId))
                            vkcall.setIndexBuffer(addBufferOnce(block->parent->id), *block);
                        if (auto block = session.findItem<Block>(call->indirectBufferBlockId))
                            vkcall.setIndirectBuffer(addBufferOnce(block->parent->id), *block);
                        break;

                    case Call::CallType::Compute:
                    case Call::CallType::ComputeIndirect:
                        vkcall.setProgram(addProgramOnce(call->programId));
                        if (auto block = session.findItem<Block>(call->indirectBufferBlockId))
                            vkcall.setIndirectBuffer(addBufferOnce(block->parent->id), *block);
                        break;

                    case Call::CallType::ClearTexture:
                    case Call::CallType::CopyTexture:
                    case Call::CallType::SwapTextures:
                        vkcall.setTextures(
                            addTextureOnce(call->textureId),
                            addTextureOnce(call->fromTextureId));
                        break;

                    case Call::CallType::ClearBuffer:
                    case Call::CallType::CopyBuffer:
                    case Call::CallType::SwapBuffers:
                        vkcall.setBuffers(
                            addBufferOnce(call->bufferId),
                            addBufferOnce(call->fromBufferId));
                        break;
                }
                addCommand(
                    [this,
                     executeOn = call->executeOn,
                     pcall = std::move(pvkcall)
                    ](BindingState &state) {
                        auto &call = *pcall; 
                        if (!shouldExecute(executeOn, mEvaluationType))
                            return;

                        if (auto pipeline = call.createPipeline(mCommandQueue->context))
                            mUsedItems += applyBindings(state, *pipeline);

                        call.execute(mCommandQueue->context, mMessages, 
                            mScriptSession->engine());

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

                // undo pushing commands, when there is not a sinvke iteration
                const auto &iteration = mGroupIterations[group->id];
                if (!iteration.iterations)
                    mCommandQueue->commands.resize(iteration.commandQueueBeginIndex);

                it = it->parent;
            }
        }
    });
}

void VKRenderSession::render()
{
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
                if (!program.link(device) && prev.link(device)) {
                    mCommandQueue->failedPrograms.push_back(std::move(program));
                    program = std::move(prev);
                }
            }
        }
        mPrevCommandQueue.reset();
    }
}

void VKRenderSession::setNextCommandQueueIndex(int index)
{
    mNextCommandQueueIndex = index;
}

void VKRenderSession::executeCommandQueue()
{
    //Singletons::vkShareSynchronizer().beginUpdate(context);

    auto state = BindingState{ };
    mCommandQueue->context.timestampQueries.clear();

    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex < static_cast<int>(mCommandQueue->commands.size())) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        mCommandQueue->commands[index](state);
    }

    const auto submitOptions = KDGpu::SubmitOptions{
        .commandBuffers = std::vector<KDGpu::Handle<KDGpu::CommandBuffer_t>>(
            mCommandQueue->context.commandBuffers.begin(),
            mCommandQueue->context.commandBuffers.end())
    };
    mCommandQueue->context.queue.submit(submitOptions);
    mCommandQueue->context.queue.waitUntilIdle();
    mCommandQueue->context.commandBuffers.clear();

    //Singletons::vkShareSynchronizer().endUpdate(context);
}

void VKRenderSession::downloadModifiedResources()
{
    for (auto &[itemId, program] : mCommandQueue->programs) 
        if (program.printf().isUsed())
            mMessages += program.printf().formatMessages(
                mCommandQueue->context, program.itemId());

    for (auto &[itemId, texture] : mCommandQueue->textures) {
        texture.updateMipmaps(mCommandQueue->context);
        if (!updatingPreviewTextures() &&
            !texture.fileName().isEmpty() &&
            texture.download(mCommandQueue->context))
            mModifiedTextures[texture.itemId()] = texture.data();
    }
    
    for (auto &[itemId, buffer] : mCommandQueue->buffers)
        if (!buffer.fileName().isEmpty() &&
            (mItemsChanged || mEvaluationType != EvaluationType::Steady) &&
            buffer.download(mCommandQueue->context, mEvaluationType != EvaluationType::Reset))
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

void VKRenderSession::outputTimerQueries()
{
    mTimerMessages.clear();

    auto total = std::chrono::nanoseconds::zero();
    auto &queries = mCommandQueue->context.timestampQueries;
    for (auto &[itemId, query] : queries) {
        const auto duration = std::chrono::nanoseconds(query.nsInterval(0, 1));
        mTimerMessages += MessageList::insert(
            itemId, MessageType::CallDuration,
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

    auto &editors = Singletons::editorManager();
    auto &session = Singletons::sessionModel();

    if (updatingPreviewTextures())
        for (const auto& [itemId, texture] : mCommandQueue->textures)
            if (texture.deviceCopyModified())
                if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
                    if (auto editor = editors.getTextureEditor(fileItem->fileName))
                        if (auto handle = texture.getSharedMemoryHandle(); handle.handle)
                            editor->updatePreviewTexture(handle, texture.samples());
}

void VKRenderSession::release()
{
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
}
