
#include "GLShareSync.h"

void GLShareSync::cleanup(GLContext &gl)
{
    QMutexLocker lock{ &mMutex };

    if (mUpdateFenceSync)
        gl.glDeleteSync(mUpdateFenceSync);
    mUpdateFenceSync = {};

    for (const auto &usageSync : std::as_const(mUsageFenceSyncs))
        gl.glDeleteSync(usageSync);
    mUsageFenceSyncs.clear();
}

void GLShareSync::beginUpdate(GLContext &gl)
{
    mMutex.lock();
    // synchronize with end of usage
    for (const auto &usageSync : std::as_const(mUsageFenceSyncs)) {
        gl.glWaitSync(usageSync, 0, GL_TIMEOUT_IGNORED);
        gl.glDeleteSync(usageSync);
    }
    mUsageFenceSyncs.clear();
}

void GLShareSync::endUpdate(GLContext &gl)
{
    // mark end of update
    if (mUpdateFenceSync)
        gl.glDeleteSync(mUpdateFenceSync);
    mUpdateFenceSync = gl.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    gl.glFlush();
    mMutex.unlock();
}

void GLShareSync::beginUsage(void *context)
{
    auto &gl = *static_cast<GLContext *>(context);
    mMutex.lock();

    // synchronize with end of update
    if (mUpdateFenceSync)
        gl.glWaitSync(mUpdateFenceSync, 0, GL_TIMEOUT_IGNORED);
}

void GLShareSync::endUsage(void *context)
{
    // mark end of usage
    auto &gl = *static_cast<GLContext *>(context);
    mUsageFenceSyncs.append(gl.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
    gl.glFlush();
    mMutex.unlock();
}
