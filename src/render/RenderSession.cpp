#include "RenderSession.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/ImageEditor.h"
#include "editors/BinaryEditor.h"
#include "scripting/ScriptEngine.h"
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
#include <QOpenGLVertexArrayObject>

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

    QSet<ItemId> applyBindings(BindingState &state,
        GLProgram &program, ScriptEngine &scriptEngine)
    {
        QSet<ItemId> usedItems;
        BindingScope bindings;
        foreach (const BindingScope &scope, state) {
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
            if (program.apply(kv.second, unit)) {
                ++unit;
                usedItems += kv.second.bindingItemId;
                usedItems += kv.second.buffer->usedItems();
            }

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
    std::map<ItemId, GLStream> vertexStream;
    std::deque<Command> commands;
    QList<ScriptEngine::Script> scripts;
    QList<GLCall*> timerQueryCalls;
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

void RenderSession::prepare(bool itemsChanged, bool manualEvaluation)
{
    mPrevMessages.swap(mMessages);
    mMessages.clear();

    if (mCommandQueue && !(itemsChanged || manualEvaluation))
        return;

    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue());
    mUsedItems.clear();

    if (manualEvaluation)
        mUpdatingPreviewTexture.clear();

    // always re-evaluate scripts on manual evaluation
    if (!mScriptEngine || manualEvaluation)
        mScriptEngine.reset();

    auto &session = Singletons::sessionModel();

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
            const auto &items = target->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attachment = castItem<Attachment>(items[i]))
                    fb->setAttachment(i, addTextureOnce(attachment->textureId));
        }
        return fb;
    };

    auto addVertexStreamOnce = [&](ItemId vertexStreamId) {
        auto vertexStream = session.findItem<Stream>(vertexStreamId);
        auto vs = addOnce(mCommandQueue->vertexStream, vertexStream);
        if (vs) {
            const auto &items = vertexStream->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attribute = castItem<Attribute>(items[i]))
                    if (auto column = session.findItem<Column>(attribute->columnId))
                        if (column->parent)
                            vs->setAttribute(i, *column,
                                addBufferOnce(column->parent->id));
        }
        return vs;
    };

    session.forEachItem([&](const Item &item) {

        if (auto group = castItem<Group>(item)) {
            // push binding scope
            if (!group->inlineScope)
                addCommand([](BindingState &state) { state.push({ }); });
        }
        else if (auto script = castItem<Script>(item)) {
            // for now all scripts are unconditionally evaluated
            mUsedItems += script->id;
            auto source = QString();
            Singletons::fileCache().getSource(script->fileName, &source);
            mCommandQueue->scripts += ScriptEngine::Script{ script->fileName, source };
        }
        else if (auto binding = castItem<Binding>(item)) {
            const auto &b = *binding;
            switch (b.bindingType) {
                case Binding::BindingType::Uniform:
                    addCommand(
                        [binding = GLUniformBinding{
                            b.id, b.name, b.bindingType, b.values, false }
                        ](BindingState &state) {
                            state.top().uniforms[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::Sampler:
                    addCommand(
                        [binding = GLSamplerBinding{
                            b.id, b.name, addTextureOnce(b.textureId),
                            b.minFilter, b.magFilter,
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
                            b.level, b.layered, b.layer,
                            GLenum{ GL_READ_WRITE } }
                        ](BindingState &state) {
                            state.top().images[binding.name] = binding;
                        });
                    break;

                case Binding::BindingType::Buffer:
                    addCommand(
                        [binding = GLBufferBinding{
                            b.id, b.name, addBufferOnce(b.bufferId) }
                        ](BindingState &state) {
                            state.top().buffers[binding.name] = binding;
                        });
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
                    ](BindingState &state) mutable {
                        if (auto program = call.program()) {
                            mUsedItems += program->usedItems();
                            if (!program->bind(&mMessages))
                                return;
                            mUsedItems += applyBindings(
                                state, *program, *mScriptEngine);
                            call.execute(mMessages, *mScriptEngine);
                            program->unbind(call.itemId());
                        }
                        else {
                            call.execute(mMessages, *mScriptEngine);
                        }
                        mCommandQueue->timerQueryCalls += &call;
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
    if (mScriptEngine &&
        mCommandQueue->scripts == mScriptEngine->scripts())
        return;
    mScriptEngine.reset(new ScriptEngine(mCommandQueue->scripts));
}

void RenderSession::executeCommandQueue()
{
    auto& context = GLContext::currentContext();
    if (!context) {
        mMessages += MessageList::insert(
            0, MessageType::OpenGLVersionNotAvailable, "3.3");
        return;
    }

    Singletons::glShareSynchronizer().beginUpdate(context);

    BindingState state;
    for (auto &command : mCommandQueue->commands)
        command(state);

    Singletons::glShareSynchronizer().endUpdate(context);
}

void RenderSession::downloadModifiedResources()
{
    for (auto &texture : mCommandQueue->textures)
        if (!mUpdatingPreviewTexture.contains(texture.first))
            mModifiedImages = mModifiedImages.unite(
                texture.second.getModifiedImages());

    for (auto &buffer : mCommandQueue->buffers)
        mModifiedBuffers = mModifiedBuffers.unite(
            buffer.second.getModifiedData());
}

void RenderSession::outputTimerQueries()
{
    foreach (const GLCall *call, mCommandQueue->timerQueryCalls)
        if (call->duration().count() >= 0)
            mMessages += MessageList::insert(
                call->itemId(), MessageType::CallDuration,
                formatQueryDuration(call->duration()), false);

    if (mCommandQueue->timerQueryCalls.empty())
        mMessages += MessageList::insert(
            0, MessageType::NoActiveCalls);
}

void RenderSession::finish(bool steadyEvaluation)
{
    // do not emit data changed in automatic refresh mode,
    // since it would keep triggering updates
    auto emitDataChanged = steadyEvaluation;

    auto &manager = Singletons::editorManager();
    auto &session = Singletons::sessionModel();

    for (auto itemId : mModifiedImages.keys())
        if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
            if (auto editor = manager.openImageEditor(fileItem->fileName, false)) {
                editor->replace(mModifiedImages[itemId], emitDataChanged);
                mUpdatingPreviewTexture.insert(itemId);
            }
    mModifiedImages.clear();

    // keep updating preview texture
    for (auto itemId : mUpdatingPreviewTexture) {
        const auto it = mCommandQueue->textures.find(itemId);
        if (it != mCommandQueue->textures.end())
            if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
                if (auto editor = manager.getImageEditor(fileItem->fileName))
                    editor->updatePreviewTexture(it->second.getReadOnlyTextureId());
    }

    for (auto itemId : mModifiedBuffers.keys())
        if (auto fileItem = castItem<FileItem>(session.findItem(itemId)))
            if (auto editor = manager.openBinaryEditor(fileItem->fileName, false))
                editor->replace(mModifiedBuffers[itemId], emitDataChanged);
    mModifiedBuffers.clear();

    mPrevMessages.clear();

    QMutexLocker lock{ &mUsedItemsCopyMutex };
    mUsedItemsCopy = mUsedItems;
}

void RenderSession::release()
{
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
}
