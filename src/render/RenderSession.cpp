#include "RenderSession.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "editors/TextureEditor.h"
#include "editors/BinaryEditor.h"
#include "scripting/ScriptEngine.h"
#include "scripting/InputScriptObject.h"
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
#include <QOpenGLTimerQuery>

extern bool gZeroCopyPreview;

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
    QOpenGLVertexArrayObject vao;
    std::map<ItemId, GLTexture> textures;
    std::map<ItemId, GLBuffer> buffers;
    std::map<ItemId, GLProgram> programs;
    std::map<ItemId, GLTarget> targets;
    std::map<ItemId, GLStream> vertexStream;
    std::deque<Command> commands;
};

RenderSession::RenderSession(QObject *parent)
    : RenderTask(parent)
    , mInputScriptObject(new InputScriptObject(this))
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

void RenderSession::prepare(bool itemsChanged,
        EvaluationType evaluationType)
{
    mItemsChanged = itemsChanged;
    mEvaluationType = evaluationType;
    mPrevMessages.swap(mMessages);
    mMessages.clear();
    mInputScriptObject->setMousePosition(Singletons::synchronizeLogic().mousePosition());

    if (!mCommandQueue)
        mEvaluationType = EvaluationType::Reset;

    if (!itemsChanged && mEvaluationType != EvaluationType::Reset)
        return;

    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue.reset(new CommandQueue());
    mUsedItems.clear();

    const auto &session = Singletons::sessionModel();

    const auto addCommand = [&](auto&& command) {
        mCommandQueue->commands.emplace_back(std::move(command));
    };

    const auto addProgramOnce = [&](ItemId programId) {
        return addOnce(mCommandQueue->programs,
            session.findItem<Program>(programId));
    };

    const auto addBufferOnce = [&](ItemId bufferId) {
        return addOnce(mCommandQueue->buffers,
            session.findItem<Buffer>(bufferId));
    };

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            session.findItem<Texture>(textureId));
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
            mUsedItems += script->id;
            auto source = QString();
            if (script->fileName.isEmpty()) {
                source = script->expression;
            }
            else {
                Singletons::fileCache().getSource(script->fileName, &source);
            }
            addCommand(
                [this, source, fileName = script->fileName,
                  executeOn = script->executeOn
                ](BindingState &) {
                      if (shouldExecute(executeOn, mEvaluationType))
                          mScriptEngine->evaluateScript(source, fileName);
                });
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

                glcall.setBuffers(
                    addBufferOnce(call->bufferId),
                    addBufferOnce(call->fromBufferId));
                glcall.setTextures(
                    addTextureOnce(call->textureId),
                    addTextureOnce(call->fromTextureId));

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
                            mUsedItems += applyBindings(
                                state, *program, *mScriptEngine);
                            call.execute(mMessages, *mScriptEngine);
                            program->unbind(call.itemId());
                        }
                        else {
                            call.execute(mMessages, *mScriptEngine);
                        }
                        if (auto timerQuery = call.timerQuery())
                            mTimerQueries[call.itemId()] = std::move(timerQuery);
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
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    auto& gl = GLContext::currentContext();
    if (!gl) {
        mMessages += MessageList::insert(
            0, MessageType::OpenGLVersionNotAvailable, "3.3");
        return;
    }

    gl.glEnable(GL_FRAMEBUFFER_SRGB);
    gl.glEnable(GL_PROGRAM_POINT_SIZE);
    gl.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    gl.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    QOpenGLVertexArrayObject::Binder vaoBinder(&mCommandQueue->vao);
    reuseUnmodifiedItems();
    executeCommandQueue();
    downloadModifiedResources();
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
        mPrevCommandQueue.reset();
    }
}

void RenderSession::executeCommandQueue()
{
    auto& context = GLContext::currentContext();

    // always re-evaluate scripts on reset
    if (!mScriptEngine || mEvaluationType == EvaluationType::Reset)
        mScriptEngine.reset(new ScriptEngine());

    mScriptEngine->setGlobal("input", mInputScriptObject);

    Singletons::glShareSynchronizer().beginUpdate(context);

    BindingState state;
    for (auto &command : mCommandQueue->commands)
        command(state);

    Singletons::glShareSynchronizer().endUpdate(context);
}

bool RenderSession::updatingPreviewTextures() const
{
    return (gZeroCopyPreview &&
        !mItemsChanged &&
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
            buffer.download() &&
            mEvaluationType != EvaluationType::Steady)
            mModifiedBuffers[buffer.itemId()] = buffer.data();
}

void RenderSession::outputTimerQueries()
{
    for (auto itemId : mTimerQueries.keys()) {
        const auto duration = std::chrono::nanoseconds(
            mTimerQueries[itemId]->waitForResult());
        mMessages += MessageList::insert(
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
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
    mTimerQueries.clear();
}
