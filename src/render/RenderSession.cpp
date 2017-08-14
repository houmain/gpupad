#include "RenderSession.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/ImageEditor.h"
#include "editors/BinaryEditor.h"
#include "FileCache.h"
#include "ScriptEngine.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "GLProgram.h"
#include "GLTarget.h"
#include "GLVertexStream.h"
#include "GLCall.h"
#include <functional>
#include <deque>
#include <QStack>
#include <QOpenGLVertexArrayObject>

namespace {
    struct BindingScope
    {
        std::map<QString, GLUniformBinding> uniforms;
        std::map<QString, GLSamplerBinding> samplers;
        std::map<QString, GLImageBinding> images;
        std::map<QString, GLBufferBinding> buffers;
    };
    using BindingState = QStack<BindingScope>;
    using Command = std::function<void(BindingState&)>;

    QSet<ItemId> applyBindings(BindingState& state,
            GLProgram& program, ScriptEngine &scriptEngine)
    {
        QSet<ItemId> usedItems;
        BindingScope bindings;
        foreach (const BindingScope &scope, state) {
            for (const auto& kv : scope.uniforms)
                bindings.uniforms[kv.first] = kv.second;
            for (const auto& kv : scope.samplers)
                bindings.samplers[kv.first] = kv.second;
            for (const auto& kv : scope.images)
                bindings.images[kv.first] = kv.second;
            for (const auto& kv : scope.buffers)
                bindings.buffers[kv.first] = kv.second;
        }

        for (const auto& kv : bindings.uniforms)
            if (program.apply(kv.second, scriptEngine))
                usedItems += kv.second.bindingItemId;

        auto unit = 0;
        for (const auto& kv : bindings.samplers)
            if (program.apply(kv.second, unit++)) {
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.texture->usedItems();
            }

        unit = 0;
        for (const auto& kv : bindings.images)
            if (program.apply(kv.second, unit++)) {
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.texture->usedItems();
            }

        unit = 0;
        for (const auto& kv : bindings.buffers)
            if (program.apply(kv.second, unit++)) {
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.buffer->usedItems();
            }
        return usedItems;
    }

    template<typename T, typename Item, typename... Args>
    T* addOnce(std::map<ItemId, T>& list, const Item* item, Args&&... args)
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
    void replaceEqual(std::map<ItemId, T>& to, std::map<ItemId, T>& from)
    {
        for (auto& kv : to) {
            auto it = from.find(kv.first);
            if (it != from.end() && kv.second == it->second)
                kv.second = std::move(it->second);
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
} // namespace

struct RenderSession::CommandQueue
{
    QOpenGLVertexArrayObject vao;
    std::map<ItemId, GLTexture> textures;
    std::map<ItemId, GLBuffer> buffers;
    std::map<ItemId, GLProgram> programs;
    std::map<ItemId, GLTarget> targets;
    std::map<ItemId, GLVertexStream> vertexStream;
    std::deque<Command> commands;
    QList<ScriptEngine::Script> scripts;
};

struct RenderSession::TimerQueries
{
    QList<GLCall*> calls;
    MessagePtrSet messages;
};

RenderSession::RenderSession(QObject *parent)
    : RenderTask(parent)
    , mTimerQueries(new TimerQueries())
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

void RenderSession::prepare(bool itemsChanged, bool manualEvaluation)
{
    if (mCommandQueue && !(itemsChanged || manualEvaluation))
        return;

    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue());
    mUsedItems.clear();

    // always re-evaluate scripts on manual evaluation
    if (!mScriptEngine || manualEvaluation)
        mScriptEngine.reset(new ScriptEngine());

    auto& session = Singletons::sessionModel();

    auto addCommand = [&](auto&& command) {
        mCommandQueue->commands.emplace_back(std::move(command));
    };

    auto addProgramOnce = [&](ItemId programId) {
        return addOnce(mCommandQueue->programs,
            session.findItem<Program>(programId));
    };

    auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(mCommandQueue->buffers,
            session.findItem<Buffer>(bufferId));
    };

    auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Texture>(textureId));
    };

    auto addTargetOnce = [&](ItemId targetId) {
        auto target = session.findItem<Target>(targetId);
        auto fb = addOnce(mCommandQueue->targets, target);
        if (fb) {
            const auto& items = target->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attachment = castItem<Attachment>(items[i]))
                    fb->setAttachment(i, addTextureOnce(attachment->textureId));
        }
        return fb;
    };

    auto addVertexStreamOnce = [&](ItemId vertexStreamId) {
        auto vertexStream = session.findItem<VertexStream>(vertexStreamId);
        auto vs = addOnce(mCommandQueue->vertexStream, vertexStream);
        if (vs) {
            const auto& items = vertexStream->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attribute = castItem<Attribute>(items[i]))
                    if (auto column = session.findItem<Column>(attribute->columnId))
                        if (column->parent)
                            vs->setAttribute(i, *column,
                                addBufferOnce(column->parent->id));
        }
        return vs;
    };

    session.forEachItem([&](const Item& item) {

        if (auto group = castItem<Group>(item)) {
            // push binding scope
            if (!group->inlineScope)
                addCommand([](BindingState& state) { state.push({ }); });
        }
        else if (auto script = castItem<Script>(item)) {
            // for now all scripts are unconditionally evaluated
            mUsedItems += script->id;
            auto source = QString();
            Singletons::fileCache().getSource(script->fileName, &source);
            mCommandQueue->scripts += ScriptEngine::Script{ script->fileName, source };
        }
        else if (auto binding = castItem<Binding>(item)) {
            switch (binding->type) {
                case Binding::Type::Uniform:
                    for (auto index = 0; index < binding->valueCount; ++index)
                        addCommand(
                            [binding = GLUniformBinding{
                                binding->id, getUniformName(binding->name, index),
                                binding->type, binding->values[index].fields }
                            ](BindingState& state) {
                                state.top().uniforms[binding.name] = binding;
                            });
                    break;

                case Binding::Type::Sampler:
                    for (auto index = 0; index < binding->valueCount; ++index) {
                        const auto &value = binding->values[index];
                        addCommand(
                            [binding = GLSamplerBinding{
                                binding->id, getUniformName(binding->name, index),
                                addTextureOnce(value.textureId),
                                value.minFilter, value.magFilter,
                                value.wrapModeX, value.wrapModeY, value.wrapModeZ,
                                value.borderColor,
                                value.comparisonFunc }
                            ](BindingState& state) {
                                state.top().samplers[binding.name] = binding;
                            });
                    }
                    break;

                case Binding::Type::Image:
                    for (auto index = 0; index < binding->valueCount; ++index) {
                        const auto &value = binding->values[index];
                        auto access = GLenum{ GL_READ_WRITE };
                        addCommand(
                            [binding = GLImageBinding{
                                binding->id, getUniformName(binding->name, index),
                                addTextureOnce(value.textureId),
                                value.level, value.layered, value.layer, access }
                            ](BindingState& state) {
                                state.top().images[binding.name] = binding;
                            });
                    }
                    break;

                case Binding::Type::Buffer:
                    for (auto index = 0; index < binding->valueCount; ++index) {
                        const auto &value = binding->values[index];
                        addCommand(
                            [binding = GLBufferBinding{
                                binding->id, getUniformName(binding->name, index),
                                addBufferOnce(value.bufferId) }
                            ](BindingState& state) {
                                state.top().buffers[binding.name] = binding;
                            });
                    }
                    break;
                }
        }
        else if (auto call = castItem<Call>(item)) {
            if (call->checked) {
                mUsedItems += call->id;

                auto glcall = GLCall(*call);
                glcall.setProgram(addProgramOnce(call->programId));
                glcall.setTarget(addTargetOnce(call->targetId));
                glcall.setVextexStream(addVertexStreamOnce(call->vertexStreamId));

                if (auto buffer = session.findItem<Buffer>(call->indexBufferId))
                    glcall.setIndexBuffer(addBufferOnce(buffer->id), *buffer);

                if (auto buffer = session.findItem<Buffer>(call->indirectBufferId))
                    glcall.setIndirectBuffer(addBufferOnce(buffer->id), *buffer);

                glcall.setBuffer(addBufferOnce(call->bufferId));
                glcall.setTexture(addTextureOnce(call->textureId));

                addCommand(
                    [this,
                     call = std::move(glcall)
                    ](BindingState& state) mutable {
                        auto program = call.program();
                        if (program && program->bind())
                            mUsedItems += applyBindings(state,
                                *program, *mScriptEngine);

                        call.execute();
                        mTimerQueries->calls += &call;
                        mUsedItems += call.usedItems();

                        if (program) {
                            program->unbind();
                            mUsedItems += program->usedItems();
                        }
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
                    addCommand([](BindingState& state) {
                        state.pop();
                    });
                it = it->parent;
            }
        }
    });
}

void RenderSession::render()
{
    QOpenGLVertexArrayObject::Binder vaoBinder(&mCommandQueue->vao);

    reuseUnmodifiedItems();
    evaluateScripts();
    executeCommandQueue();
    downloadModifiedResources();
    outputTimerQueries();
}

void RenderSession::reuseUnmodifiedItems()
{
    if (mPrevCommandQueue) {
        replaceEqual(mCommandQueue->textures, mPrevCommandQueue->textures);
        replaceEqual(mCommandQueue->buffers, mPrevCommandQueue->buffers);
        replaceEqual(mCommandQueue->programs, mPrevCommandQueue->programs);
        mPrevCommandQueue.reset();
    }
}

void RenderSession::evaluateScripts()
{
    mScriptEngine->evalScripts(mCommandQueue->scripts);
}

void RenderSession::executeCommandQueue()
{
    BindingState state;
    for (auto& command : mCommandQueue->commands)
        command(state);
}

void RenderSession::downloadModifiedResources()
{
    for (auto& texture : mCommandQueue->textures)
        mModifiedImages += texture.second.getModifiedImages();

    for (auto& buffer : mCommandQueue->buffers)
        mModifiedBuffers += buffer.second.getModifiedData();
}

void RenderSession::outputTimerQueries()
{
    auto messages = MessagePtrSet();
    foreach (const GLCall *call, mTimerQueries->calls)
        if (call->duration().count() >= 0)
            messages += Singletons::messageList().insert(
                call->itemId(), MessageType::CallDuration,
                formatQueryDuration(call->duration()), false);

    if (mTimerQueries->calls.empty())
        messages += Singletons::messageList().insert(
            0, MessageType::NoActiveCalls);

    mTimerQueries->messages = messages;
    mTimerQueries->calls.clear();
}

void RenderSession::finish()
{
    auto& manager = Singletons::editorManager();
    auto& session = Singletons::sessionModel();

    for (auto& image : mModifiedImages)
        if (auto fileItem = castItem<FileItem>(session.findItem(image.first)))
            if (auto editor = manager.openImageEditor(fileItem->fileName, false))
                editor->replace(image.second, false);
    mModifiedImages.clear();

    for (auto& buffer : mModifiedBuffers)
        if (auto fileItem = castItem<FileItem>(session.findItem(buffer.first)))
            if (auto editor = manager.openBinaryEditor(fileItem->fileName, false))
                editor->replace(buffer.second, false);
    mModifiedBuffers.clear();

    QMutexLocker lock{ &mUsedItemsCopyMutex };
    mUsedItemsCopy = mUsedItems;
}

void RenderSession::release()
{
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
}
