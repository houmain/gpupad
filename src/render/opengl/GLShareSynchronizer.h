#pragma once

#include "GLContext.h"
#include <QMutex>
#include <QSet>

class GLShareSynchronizer
{
public:
    void beginUpdate(QOpenGLFunctions_3_3_Core &gl)
    {
        mMutex.lock();
        // synchronize with end of usage
        for (const auto &usageSync : std::as_const(mUsageFenceSyncs)) {
            gl.glWaitSync(usageSync, 0, GL_TIMEOUT_IGNORED);
            gl.glDeleteSync(usageSync);
        }
        mUsageFenceSyncs.clear();
    }

    void endUpdate(QOpenGLFunctions_3_3_Core &gl)
    {
        // mark end of update
        if (mUpdateFenceSync)
            gl.glDeleteSync(mUpdateFenceSync);
        mUpdateFenceSync = gl.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        gl.glFlush();
        mMutex.unlock();
    }

    void beginUsage(QOpenGLFunctions_3_3_Core &gl)
    {
        mMutex.lock();
        // synchronize with end of update
        if (mUpdateFenceSync)
            gl.glWaitSync(mUpdateFenceSync, 0, GL_TIMEOUT_IGNORED);
    }

    void endUsage(QOpenGLFunctions_3_3_Core &gl)
    {
        // mark end of usage
        mUsageFenceSyncs.append(
            gl.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
        gl.glFlush();
        mMutex.unlock();
    }

private:
    QMutex mMutex;
    GLsync mUpdateFenceSync{};
    QList<GLsync> mUsageFenceSyncs;
};
