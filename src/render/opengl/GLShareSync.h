#pragma once

#include "GLContext.h"
#include "render/ShareSync.h"
#include <QMutex>

class GLShareSync : public ShareSync
{
public:
    void cleanup(GLContext &gl);
    void beginUpdate(GLContext &gl);
    void endUpdate(GLContext &gl);
    void beginUsage(void *context) override;
    void endUsage(void *context) override;

private:
    QMutex mMutex;
    GLsync mUpdateFenceSync{};
    QList<GLsync> mUsageFenceSyncs;
};
