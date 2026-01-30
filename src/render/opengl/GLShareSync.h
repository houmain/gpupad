#pragma once

#include "GLContext.h"
#include "render/ShareSync.h"
#include <QMutex>

class GLShareSync : public ShareSync
{
public:
    void cleanup(QOpenGLFunctions_4_5_Core &gl);
    void beginUpdate(QOpenGLFunctions_4_5_Core &gl);
    void endUpdate(QOpenGLFunctions_4_5_Core &gl);
    void beginUsage(QOpenGLFunctions_4_5_Core &gl) override;
    void endUsage(QOpenGLFunctions_4_5_Core &gl) override;

private:
    QMutex mMutex;
    GLsync mUpdateFenceSync{};
    QList<GLsync> mUsageFenceSyncs;
};
