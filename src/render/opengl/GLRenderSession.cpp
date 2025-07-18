#include "GLRenderSession.h"
#include "GLShareSync.h"
#include "GLBuffer.h"
#include "GLCall.h"
#include "GLProgram.h"
#include "GLStream.h"
#include "GLTarget.h"
#include "GLTexture.h"
#include "render/RenderSessionBase_CommandQueue.h"
#include <QOpenGLTimerQuery>
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

void GLRenderSession::render()
{
    if (itemsChanged() || evaluationType() == EvaluationType::Reset) {
        createCommandQueue();
        buildCommandQueue<GLRenderSession>(*mCommandQueue);
    }
    Q_ASSERT(mCommandQueue);

    auto &gl = GLContext::currentContext();
    if (!gl)
        return addMessage(MessageList::insert(0,
            MessageType::OpenGLVersionNotAvailable, "3.3"));

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);

    gl.glEnable(GL_FRAMEBUFFER_SRGB);
    gl.glEnable(GL_PROGRAM_POINT_SIZE);
    gl.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#if GL_VERSION_4_3
    if (gl.v4_3)
        gl.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif

    if (mPrevCommandQueue) {
        reuseUnmodifiedItems(*mCommandQueue, *mPrevCommandQueue);
        mPrevCommandQueue.reset();
    }

    mShareSync->beginUpdate(gl);

    gl.timerQueries.clear();

    executeCommandQueue(*mCommandQueue);

    mShareSync->endUpdate(gl);
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    downloadModifiedResources(*mCommandQueue);

    if (!updatingPreviewTextures())
        outputTimerQueries(gl.timerQueries, [](QOpenGLTimerQuery &query) {
            return std::chrono::nanoseconds(query.waitForResult());
        });

    gl.glFlush();
    Q_ASSERT(glGetError() == GL_NO_ERROR);
}

void GLRenderSession::finish()
{
    RenderSessionBase::finish();

    if (updatingPreviewTextures() && mCommandQueue)
        updatePreviewTextures(*mCommandQueue, mShareSync);
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
