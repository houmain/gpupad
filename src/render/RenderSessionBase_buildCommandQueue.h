#pragma once

#include "RenderSessionBase.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include "editors/texture/TextureEditor.h"

template <typename T>
void replaceEqual(std::map<ItemId, T> &to, std::map<ItemId, T> &from)
{
    for (auto &kv : to) {
        auto it = from.find(kv.first);
        if (it != from.end()) {
            // implicitly update untitled filename of buffer
            if constexpr (std::is_base_of_v<BufferBase, T>)
                it->second.updateUntitledFilename(kv.second);

            if (kv.second == it->second)
                kv.second = std::move(it->second);
        }
    }
}

template <typename CommandQueue>
void RenderSessionBase::reuseUnmodifiedItems(CommandQueue &commandQueue,
    CommandQueue &prevCommandQueue)
{
    replaceEqual(commandQueue.textures, prevCommandQueue.textures);
    replaceEqual(commandQueue.buffers, prevCommandQueue.buffers);
    replaceEqual(commandQueue.programs, prevCommandQueue.programs);
    replaceEqual(commandQueue.accelerationStructures,
        prevCommandQueue.accelerationStructures);

    // immediately try to link programs
    // when failing restore previous version but keep error messages
    if (mEvaluationType != EvaluationType::Reset)
        for (auto &[id, program] : commandQueue.programs)
            if (auto it = prevCommandQueue.programs.find(id);
                it != prevCommandQueue.programs.end()) {
                auto &prev = it->second;
                if (!shaderSessionSettingsDiffer(prev.session(),
                        program.session())
                    && !program.link(commandQueue.context)
                    && prev.link(commandQueue.context)) {
                    commandQueue.failedPrograms.push_back(std::move(program));
                    program = std::move(prev);
                }
            }
}

template <typename RenderSession, typename CommandQueue>
void RenderSessionBase::buildCommandQueue(CommandQueue &commandQueue)
{
    auto &self = *static_cast<RenderSession *>(this);
    auto &scriptEngine = mScriptSession->engine();
    const auto &sessionModel = mSessionModelCopy;

    const auto addCommand = [&](auto &&command) {
        commandQueue.commands.emplace_back(std::move(command));
    };

    const auto addProgramOnce = [&](ItemId programId) {
        return addOnce(commandQueue.programs,
            sessionModel.findItem<Program>(programId),
            sessionModel.sessionItem());
    };

    const auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(commandQueue.buffers,
            sessionModel.findItem<Buffer>(bufferId), self);
    };

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(commandQueue.textures,
            sessionModel.findItem<Texture>(textureId), self);
    };

    const auto addTextureBufferOnce = [&](ItemId bufferId, auto *buffer,
                                          Texture::Format format) {
        return addOnce(commandQueue.textures,
            sessionModel.findItem<Buffer>(bufferId), buffer, format, self);
    };

    const auto addTargetOnce = [&](ItemId targetId) {
        auto target = sessionModel.findItem<Target>(targetId);
        auto fb = addOnce(commandQueue.targets, target, self);
        if (fb) {
            const auto &items = target->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attachment = castItem<Attachment>(items[i]))
                    fb->setTexture(i, addTextureOnce(attachment->textureId));
        }
        return fb;
    };

    const auto addVertexStreamOnce = [&](ItemId vertexStreamId) {
        auto vertexStream = sessionModel.findItem<Stream>(vertexStreamId);
        auto vs = addOnce(commandQueue.vertexStreams, vertexStream);
        if (vs) {
            const auto &items = vertexStream->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attribute = castItem<Attribute>(items[i]))
                    if (auto field =
                            sessionModel.findItem<Field>(attribute->fieldId))
                        vs->setAttribute(i, *field,
                            addBufferOnce(field->parent->parent->id),
                            scriptEngine);
        }
        return vs;
    };

    const auto addAccelerationStructureOnce = [&](ItemId accelStructId) {
        const auto accelerationStructure =
            sessionModel.findItem<AccelerationStructure>(accelStructId);
        auto as =
            addOnce(commandQueue.accelerationStructures, accelerationStructure);
        if (as) {
            const auto &items = accelerationStructure->items;
            for (auto i = 0; i < items.size(); ++i)
                for (auto j = 0; j < items[i]->items.size(); ++j)
                    if (auto geometry =
                            castItem<Geometry>(items[i]->items[j])) {
                        if (auto block = sessionModel.findItem<Block>(
                                geometry->vertexBufferBlockId))
                            as->setVertexBuffer(i, j,
                                addBufferOnce(block->parent->id), *block, self);
                        if (auto block = sessionModel.findItem<Block>(
                                geometry->indexBufferBlockId))
                            as->setIndexBuffer(i, j,
                                addBufferOnce(block->parent->id), *block, self);
                        if (auto block = sessionModel.findItem<Block>(
                                geometry->transformBufferBlockId))
                            as->setTransformBuffer(i, j,
                                addBufferOnce(block->parent->id), *block, self);
                    }
        }
        return as;
    };

    sessionModel.forEachItem([&](const Item &item) {
        if (auto group = castItem<Group>(item)) {
            // mark begin of iteration
            addCommand([this, groupId = group->id](BindingState &) {
                auto &iteration = mGroupIterations[groupId];
                iteration.iterationsLeft = iteration.iterations;
            });
            auto &iteration = mGroupIterations[group->id];
            iteration.commandQueueBeginIndex = commandQueue.commands.size();
            const auto max_iterations = 1000;
            iteration.iterations = std::min(max_iterations,
                scriptEngine.evaluateInt(group->iterations, group->id));

            // push binding scope
            if (!group->inlineScope)
                addCommand([](BindingState &state) { state.push({}); });
        } else if (castItem<ScopeItem>(item)) {
            // push binding scope
            addCommand([](BindingState &state) { state.push({}); });
        } else if (auto script = castItem<Script>(item)) {
            if (script->executeOn == Script::ExecuteOn::EveryEvaluation)
                mUsedItems += script->id;
        } else if (auto binding = castItem<Binding>(item)) {
            const auto &b = *binding;
            switch (b.bindingType) {
            case Binding::BindingType::Uniform:
                addCommand([this, b](BindingState &state) {
                    state.top().uniforms[b.name] = UniformBinding{ b.id, b.name,
                        b.bindingType, false, mUniformBindingValues[b.id] };
                });
                break;

            case Binding::BindingType::Sampler: {
                auto texture = addTextureOnce(b.textureId);
                if (texture)
                    texture->boundAsSampler();
                addCommand(
                    [binding = SamplerBinding{ b.id, b.name, std::move(texture),
                         b.minFilter, b.magFilter, b.anisotropic, b.wrapModeX,
                         b.wrapModeY, b.wrapModeZ, b.borderColor,
                         b.comparisonFunc }](BindingState &state) {
                        state.top().samplers[binding.name] = binding;
                    });
                break;
            }
            case Binding::BindingType::Image: {
                auto texture = addTextureOnce(b.textureId);
                if (texture)
                    texture->boundAsImage();
                addCommand([binding = ImageBinding{ b.id, b.name,
                                std::move(texture), b.level, b.layer,
                                b.imageFormat }](BindingState &state) {
                    state.top().images[binding.name] = binding;
                });
                break;
            }

            case Binding::BindingType::TextureBuffer: {
                addCommand(
                    [binding = ImageBinding{ b.id, b.name,
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
                addCommand([binding = BufferBinding{ b.id, b.name,
                                addBufferOnce(b.bufferId), 0, "", "",
                                0 }](BindingState &state) {
                    state.top().buffers[binding.name] = binding;
                });
                break;

            case Binding::BindingType::BufferBlock:
                if (auto block = sessionModel.findItem<Block>(b.blockId))
                    addCommand(
                        [binding = BufferBinding{ b.id, b.name,
                             addBufferOnce(block->parent->id), block->id,
                             block->offset, block->rowCount,
                             getBlockStride(*block) }](BindingState &state) {
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

                if (call->executeOn == Call::ExecuteOn::EveryEvaluation)
                    mUsedItems += call->id;

                auto queueCallPtr =
                    std::make_shared<typename CommandQueue::Call>(*call,
                        sessionModel.sessionItem());
                auto &queueCall = *queueCallPtr;
                switch (call->callType) {
                case Call::CallType::Draw:
                case Call::CallType::DrawIndexed:
                case Call::CallType::DrawIndirect:
                case Call::CallType::DrawIndexedIndirect:
                case Call::CallType::DrawMeshTasks:
                case Call::CallType::DrawMeshTasksIndirect:
                    queueCall.setProgram(addProgramOnce(call->programId));
                    queueCall.setTarget(addTargetOnce(call->targetId));
                    queueCall.setVextexStream(
                        addVertexStreamOnce(call->vertexStreamId));
                    if (auto block = sessionModel.findItem<Block>(
                            call->indexBufferBlockId))
                        queueCall.setIndexBuffer(
                            addBufferOnce(block->parent->id), *block);
                    if (auto block = sessionModel.findItem<Block>(
                            call->indirectBufferBlockId))
                        queueCall.setIndirectBuffer(
                            addBufferOnce(block->parent->id), *block);
                    break;

                case Call::CallType::Compute:
                case Call::CallType::ComputeIndirect:
                    queueCall.setProgram(addProgramOnce(call->programId));
                    if (auto block = sessionModel.findItem<Block>(
                            call->indirectBufferBlockId))
                        queueCall.setIndirectBuffer(
                            addBufferOnce(block->parent->id), *block);
                    break;

                case Call::CallType::TraceRays:
                    queueCall.setProgram(addProgramOnce(call->programId));
                    if (auto accelStruct = addAccelerationStructureOnce(
                            call->accelerationStructureId))
                        queueCall.setAccelerationStructure(accelStruct);
                    break;

                case Call::CallType::ClearTexture:
                case Call::CallType::CopyTexture:
                case Call::CallType::SwapTextures:
                    queueCall.setTextures(addTextureOnce(call->textureId),
                        addTextureOnce(call->fromTextureId));
                    break;

                case Call::CallType::ClearBuffer:
                case Call::CallType::CopyBuffer:
                case Call::CallType::SwapBuffers:
                    queueCall.setBuffers(addBufferOnce(call->bufferId),
                        addBufferOnce(call->fromBufferId));
                    break;
                }
                addCommand([this, executeOn = call->executeOn,
                               context = &commandQueue.context,
                               queueCallPtr = std::move(queueCallPtr)](
                               BindingState &state) {
                    auto &queueCall = *queueCallPtr;
                    if (!shouldExecute(executeOn, mEvaluationType))
                        return;

                    auto merged = Bindings{};
                    for (const Bindings &scope : state) {
                        for (const auto &[name, binding] : scope.uniforms)
                            merged.uniforms[name] = binding;
                        for (const auto &[name, binding] : scope.samplers)
                            merged.samplers[name] = binding;
                        for (const auto &[name, binding] : scope.images)
                            merged.images[name] = binding;
                        for (const auto &[name, binding] : scope.buffers)
                            merged.buffers[name] = binding;
                    }
                    queueCall.execute(*context, std::move(merged), mMessages,
                        mScriptSession->engine());

                    if (executeOn == Call::ExecuteOn::EveryEvaluation)
                        mUsedItems += queueCall.usedItems();
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
                    if (!iteration.iterations) {
                        commandQueue.commands.resize(
                            iteration.commandQueueBeginIndex);
                    } else {
                        mUsedItems += group->id;
                    }
                } else {
                    addCommand([](BindingState &state) { state.pop(); });
                }
        }
    });
}

template <typename CommandQueue>
void RenderSessionBase::executeCommandQueue(CommandQueue &commandQueue)
{
    auto state = BindingState{};
    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex < commandQueue.commands.size()) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        commandQueue.commands[index](state);
    }
}

template <typename CommandQueue>
void RenderSessionBase::downloadModifiedResources(CommandQueue &commandQueue)
{
    for (auto &[itemId, program] : commandQueue.programs)
        if (program.printf().isUsed())
            mMessages += program.printf().formatMessages(commandQueue.context,
                program.itemId());

    for (auto &[itemId, texture] : commandQueue.textures)
        if (!texture.fileName().isEmpty()) {
            texture.updateMipmaps(commandQueue.context);
            if (!updatingPreviewTextures()
                && texture.download(commandQueue.context))
                mModifiedTextures[texture.itemId()] = texture.data();
        }

    for (auto &[itemId, buffer] : commandQueue.buffers)
        if (!buffer.fileName().isEmpty()
            && (mItemsChanged || mEvaluationType != EvaluationType::Steady)
            && buffer.download(commandQueue.context,
                mEvaluationType != EvaluationType::Reset))
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

template <typename TimerQueries, typename ToNanoseconds>
void RenderSessionBase::outputTimerQueries(TimerQueries &timerQueries,
    const ToNanoseconds &toNanoseconds)
{
    mTimerMessages.clear();

    auto total = std::chrono::nanoseconds::zero();
    for (auto &[itemId, query] : timerQueries) {
        const auto duration = toNanoseconds(query);
        mTimerMessages += MessageList::insert(itemId, MessageType::CallDuration,
            formatDuration(duration), false);
        total += duration;
    }
    if (timerQueries.size() > 1)
        mTimerMessages += MessageList::insert(0, MessageType::TotalDuration,
            formatDuration(total), false);
}

template <typename CommandQueue>
void RenderSessionBase::updatePreviewTextures(CommandQueue &commandQueue,
    ShareSyncPtr shareSync)
{
    auto &editors = Singletons::editorManager();
    auto &sessionModel = Singletons::sessionModel();
    for (const auto &[itemId, texture] : commandQueue.textures)
        if (texture.deviceCopyModified())
            if (auto fileItem =
                    castItem<FileItem>(sessionModel.findItem(itemId)))
                if (auto editor = editors.getTextureEditor(fileItem->fileName))
                    editor->updatePreviewTexture(shareSync,
                        texture.getSharedMemoryHandle(), texture.samples());
}
