#include "GLRenderSession.h"
#include "GLShareSync.h"
#include "GLAccelerationStructure.h"
#include "GLBuffer.h"
#include "GLCall.h"
#include "GLProgram.h"
#include "GLStream.h"
#include "GLTarget.h"
#include "GLTexture.h"
#include "render/RenderSessionBase_CommandQueue.h"
#include <QStack>
#include <deque>
#include <functional>
#include <type_traits>

struct GLRenderSession::CommandQueue
{
    using Call = GLCall;
    GLContext &context;
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
    mCommandQueue = std::make_unique<CommandQueue>(CommandQueue{
        GLContext::currentContext(),
    });
}

std::vector<Duration> GLRenderSession::resetTimeQueries(size_t count)
{
    Q_ASSERT(count <= mTimeQueries.size());
    auto durations = std::vector<Duration>();
    durations.reserve(count);
    for (auto i = 0u; i < count; ++i)
        durations.push_back(
            std::chrono::nanoseconds(mTimeQueries[i].waitForResult()));
    return durations;
}

std::shared_ptr<void> GLRenderSession::beginTimeQuery(size_t index)
{
    if (index >= mTimeQueries.size())
        mTimeQueries.emplace_back().create();
    mTimeQueries[index].begin();
    return std::shared_ptr<void>(nullptr,
        [this, index](void *) { mTimeQueries[index].end(); });
}

void GLRenderSession::render()
{
    if (itemsChanged() || evaluationType() == EvaluationType::Reset) {
        createCommandQueue();
        buildCommandQueue<GLRenderSession>(*mCommandQueue);
    }
    Q_ASSERT(mCommandQueue);

    auto &gl = GLContext::currentContext();

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);

    gl.glEnable(GL_FRAMEBUFFER_SRGB);
    gl.glEnable(GL_PROGRAM_POINT_SIZE);
    gl.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    gl.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    if (mPrevCommandQueue) {
        reuseUnmodifiedItems(*mCommandQueue, *mPrevCommandQueue);
        mPrevCommandQueue.reset();
    }

    mShareSync->beginUpdate(gl);

    executeCommandQueue(*mCommandQueue);

    beginDownloadModifiedResources(*mCommandQueue);
    obtainTimeQueryResults();

    mShareSync->endUpdate(gl);

    Q_ASSERT(glGetError() == GL_NO_ERROR);
}

void GLRenderSession::finish()
{
    if (mCommandQueue)
        finishCommandQueue(*mCommandQueue, mShareSync);
}

void GLRenderSession::release()
{
    auto &context = GLContext::currentContext();
    mTimeQueries.clear();
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

    const auto addTextureOnce = [&](ItemId textureId) {
        return addOnce(mCommandQueue->textures,
            sessionModelCopy().findItem<Texture>(textureId), *this);
    };

    if (auto texture = addTextureOnce(itemId)) {
        addUsedItems(texture->usedItems());
        return texture->obtainBindlessHandle();
    }
    return 0;
}
