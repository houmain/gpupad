#include "GLRenderSession.h"
#include "GLShareSync.h"
#include "GLBuffer.h"
#include "GLCall.h"
#include "GLProgram.h"
#include "GLStream.h"
#include "GLTarget.h"
#include "GLTexture.h"
#include "Singletons.h"
#include "editors/EditorManager.h"
#include "editors/texture/TextureEditor.h"
#include "render/RenderSessionBase_buildCommandQueue.h"
#include <QOpenGLTimerQuery>
#include <QStack>
#include <deque>
#include <functional>
#include <type_traits>

namespace {
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

struct GLRenderSession::CommandQueue
{
    using Call = GLCall;
    GLContext context;
    std::map<ItemId, GLTexture> textures;
    std::map<ItemId, GLBuffer> buffers;
    std::map<ItemId, GLProgram> programs;
    std::map<ItemId, GLTarget> targets;
    std::map<ItemId, GLStream> vertexStreams;
    std::map<ItemId, GLAccelerationStructure> accelerationStructures;
    std::deque<Command> commands;
    std::vector<GLProgram> failedPrograms;
};

GLRenderSession::GLRenderSession(RendererPtr renderer, const QString &basePath)
    : RenderSessionBase(std::move(renderer), basePath)
    , mShareSync(std::make_shared<GLShareSync>())
{
}

GLRenderSession::~GLRenderSession()
{
    releaseResources();
}

void GLRenderSession::createCommandQueue()
{
    Q_ASSERT(!mPrevCommandQueue);
    mPrevCommandQueue.swap(mCommandQueue);
    mCommandQueue = std::make_unique<CommandQueue>();
}

void GLRenderSession::render()
{
    if (mItemsChanged || mEvaluationType == EvaluationType::Reset) {
        createCommandQueue();
        buildCommandQueue<GLRenderSession>(*mCommandQueue);
    }
    Q_ASSERT(mCommandQueue);

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
        if (mEvaluationType != EvaluationType::Reset)
            for (auto &[id, program] : mCommandQueue->programs)
                if (auto it = mPrevCommandQueue->programs.find(id);
                    it != mPrevCommandQueue->programs.end())
                    if (auto &prev = it->second;
                        !shaderSessionSettingsDiffer(prev.session(),
                            program.session())
                        && !program.link() && prev.link()) {
                        mCommandQueue->failedPrograms.push_back(
                            std::move(program));
                        program = std::move(prev);
                    }
        mPrevCommandQueue.reset();
    }
}

void GLRenderSession::executeCommandQueue()
{
    auto &context = GLContext::currentContext();
    mShareSync->beginUpdate(context);

    auto state = BindingState{};
    mCommandQueue->context.timerQueries.clear();

    mNextCommandQueueIndex = 0;
    while (mNextCommandQueueIndex < mCommandQueue->commands.size()) {
        const auto index = mNextCommandQueueIndex++;
        // executing command might call setNextCommandQueueIndex
        mCommandQueue->commands[index](state);
    }

    mShareSync->endUpdate(context);
    Q_ASSERT(glGetError() == GL_NO_ERROR);
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
    auto &queries = mCommandQueue->context.timerQueries;
    for (const auto &[itemId, query] : queries) {
        const auto duration = std::chrono::nanoseconds(query.waitForResult());
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
                            editor->updatePreviewTexture(mShareSync, textureId,
                                texture.samples());
}

void GLRenderSession::release()
{
    auto &context = GLContext::currentContext();
    context.timerQueries.clear();
    mShareSync->cleanup(context);
    mVao.destroy();
    mCommandQueue.reset();
    mPrevCommandQueue.reset();

    RenderSessionBase::release();
}

quint64 GLRenderSession::getTextureHandle(ItemId itemId)
{
    if (!mCommandQueue)
        createCommandQueue();

    const auto &sessionModel = mSessionModelCopy;

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            sessionModel.findItem<Texture>(textureId), *this);
    };

    if (auto texture = addTextureOnce(itemId)) {
        mUsedItems += texture->usedItems();
        return texture->obtainBindlessHandle();
    }
    return 0;
}
