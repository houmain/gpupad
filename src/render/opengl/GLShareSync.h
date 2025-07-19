#pragma once

#include "GLContext.h"
#include "render/ShareSync.h"
#include <QMutex>

class GLShareSync : public ShareSync
{
public:
    void cleanup(QOpenGLFunctions_3_3_Core &gl);
    void beginUpdate(QOpenGLFunctions_3_3_Core &gl);
    void endUpdate(QOpenGLFunctions_3_3_Core &gl);
    void beginUsage(QOpenGLFunctions_3_3_Core &gl) override;
    void endUsage(QOpenGLFunctions_3_3_Core &gl) override;

private:
    QMutex mMutex;
    GLsync mUpdateFenceSync{};
    QList<GLsync> mUsageFenceSyncs;
};
