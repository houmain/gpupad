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
#include "GLFramebuffer.h"
#include "GLPrimitives.h"
#include <QStack>

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
                usedItems += kv.second.samplerItemId;
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
} // namespace

struct RenderSession::CommandQueue
{
    std::map<ItemId, GLTexture> textures;
    std::map<ItemId, GLBuffer> buffers;
    std::map<ItemId, GLProgram> programs;
    std::map<ItemId, GLFramebuffer> framebuffers;
    std::map<ItemId, GLPrimitives> primitives;
    std::vector<Command> commands;
    QList<ScriptEngine::Script> scripts;
};

RenderSession::RenderSession(QObject *parent)
    : RenderTask(parent)
{
}

RenderSession::~RenderSession()
{
    releaseResources();
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

    auto addFramebufferOnce = [&](ItemId framebufferId) {
        auto framebuffer = session.findItem<Framebuffer>(framebufferId);
        auto fb = addOnce(mCommandQueue->framebuffers, framebuffer);
        if (fb) {
            const auto& items = framebuffer->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attachment = castItem<Attachment>(items[i]))
                    fb->setAttachment(i, addTextureOnce(attachment->textureId));
        }
        return fb;
    };

    auto addPrimitivesOnce = [&](ItemId primitivesId) {
        auto primitives = session.findItem<Primitives>(primitivesId);
        auto p = addOnce(mCommandQueue->primitives, primitives);
        if (p) {
            if (primitives->indexBufferId)
                if (auto buffer = session.findItem<Buffer>(primitives->indexBufferId))
                    if (!buffer->items.empty())
                        if (auto column = session.findItem<Column>(buffer->items.first()->id))
                            p->setIndices(*column, addBufferOnce(buffer->id));

            const auto& items = primitives->items;
            for (auto i = 0; i < items.size(); ++i)
                if (auto attribute = castItem<Attribute>(items[i]))
                    if (auto column = session.findItem<Column>(attribute->columnId))
                        if (column->parent)
                            p->setAttribute(i, *column,
                                addBufferOnce(column->parent->id));
        }
        return p;
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
                    addCommand(
                        [binding = GLUniformBinding{
                            binding->id, binding->name,
                            binding->type, binding->values
                        }](BindingState& state) {
                            state.top().uniforms[binding.name] = binding;
                        });
                    break;

                case Binding::Type::Sampler:
                    for (auto index = 0; index < binding->valueCount(); ++index) {
                        auto samplerId = binding->getField(index, 0).toInt();
                        if (auto sampler = session.findItem<Sampler>(samplerId)) {
                            addCommand(
                                [binding = GLSamplerBinding{
                                    binding->id, sampler->id, sampler->name, index,
                                    addTextureOnce(sampler->textureId),
                                    sampler->minFilter, sampler->magFilter,
                                    sampler->wrapModeX, sampler->wrapModeY,
                                    sampler->wrapModeZ
                                }](BindingState& state) {
                                    state.top().samplers[binding.name] = binding;
                                });
                        }
                    }
                    break;

                case Binding::Type::Image:
                    for (auto index = 0; index < binding->valueCount(); ++index) {
                        auto textureId = binding->getField(index, 0).toInt();
                        auto layer = binding->getField(index, 1).toInt();
                        auto level = 0;
                        auto layered = false;
                        auto access = GLenum{ GL_READ_WRITE };
                        addCommand(
                            [binding = GLImageBinding{
                                binding->id, binding->name, index,
                                addTextureOnce(textureId),
                                level, layered, layer, access
                            }](BindingState& state) {
                                state.top().images[binding.name] = binding;
                            });
                    }
                    break;

                case Binding::Type::Buffer:
                    for (auto index = 0; index < binding->valueCount(); ++index) {
                        auto bufferId = binding->getField(index, 0).toInt();
                        addCommand(
                            [binding = GLBufferBinding{
                                binding->id,
                                binding->name,
                                index,
                                addBufferOnce(bufferId)
                            }](BindingState& state) {
                                state.top().buffers[binding.name] = binding;
                            });
                    }
                    break;
                }
        }
        else if (auto sampler = castItem<Sampler>(item)) {
            addCommand(
                [binding = GLSamplerBinding{
                    0, sampler->id, sampler->name, 0,
                    addTextureOnce(sampler->textureId),
                    sampler->minFilter, sampler->magFilter,
                    sampler->wrapModeX, sampler->wrapModeY, sampler->wrapModeZ
                }](BindingState& state) {
                    state.top().samplers[binding.name] = binding;
                });
        }
        else if (auto call = castItem<Call>(item)) {
            if (call->checked) {
                mUsedItems += call->id;

                switch (call->type) {
                    case Call::Type::Draw:
                        addCommand(
                            [ this,
                              framebuffer = addFramebufferOnce(call->framebufferId),
                              primitives = addPrimitivesOnce(call->primitivesId),
                              program = addProgramOnce(call->programId)
                            ](BindingState& state) {
                                if (program && program->bind()) {
                                    mUsedItems += applyBindings(state,
                                        *program, *mScriptEngine);

                                    if (framebuffer && framebuffer->bind()) {
                                        primitives->draw(*program);
                                        framebuffer->unbind();
                                    }
                                    program->unbind();
                                }
                                if (program)
                                    mUsedItems += program->usedItems();
                                if (framebuffer)
                                    mUsedItems += framebuffer->usedItems();
                                if (primitives)
                                    mUsedItems += primitives->usedItems();
                            });
                        break;

                    case Call::Type::Compute:
                        addCommand(
                            [ this,
                              program = addProgramOnce(call->programId),
                              numGroupsX = call->numGroupsX,
                              numGroupsY = call->numGroupsY,
                              numGroupsZ = call->numGroupsZ
                            ](BindingState& state) {
                                if (program && program->bind()) {
                                    mUsedItems += applyBindings(state,
                                        *program, *mScriptEngine);

                                    auto& gl = GLContext::currentContext();
                                    if (gl.v4_3)
                                        gl.v4_3->glDispatchCompute(
                                            numGroupsX, numGroupsY, numGroupsZ);

                                    program->unbind();
                                }
                                if (program)
                                    mUsedItems += program->usedItems();
                            });
                        break;
                }
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
    // reuse unmodified items
    if (mPrevCommandQueue) {
        replaceEqual(mCommandQueue->textures, mPrevCommandQueue->textures);
        replaceEqual(mCommandQueue->buffers, mPrevCommandQueue->buffers);
        replaceEqual(mCommandQueue->programs, mPrevCommandQueue->programs);
        mPrevCommandQueue.reset();
    }

    // evaluate scripts
    mScriptEngine->evalScripts(mCommandQueue->scripts);

    // execute command queue
    BindingState state;
    for (auto& command : mCommandQueue->commands)
        command(state);

    // download modified resources
    for (auto& texture : mCommandQueue->textures)
        mModifiedImages += texture.second.getModifiedImages();

    for (auto& buffer : mCommandQueue->buffers)
        mModifiedBuffers += buffer.second.getModifiedData();
}

void RenderSession::finish()
{
    auto& manager = Singletons::editorManager();

    for (auto& image : mModifiedImages) {
        const auto& fileName = image.first;
        if (auto editor = manager.openImageEditor(fileName, false))
            editor->replace(image.second, false);
    }
    mModifiedImages.clear();

    for (auto& binary : mModifiedBuffers) {
        const auto& fileName = binary.first;
        if (auto editor = manager.openBinaryEditor(fileName, false))
            editor->replace(binary.second, false);
    }
    mModifiedBuffers.clear();
}

void RenderSession::release()
{
    mCommandQueue.reset();
    mPrevCommandQueue.reset();
}
