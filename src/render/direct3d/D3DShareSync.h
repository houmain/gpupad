#pragma once

#include "D3DContext.h"
#include "render/ShareSync.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QMutex>

class D3DShareSync : public ShareSync
{
public:
    explicit D3DShareSync(ID3D12Device &device);
    void cleanup();
    void beginUpdate();
    void endUpdate();
    void beginUsage(QOpenGLFunctions_3_3_Core &gl) override;
    void endUsage(QOpenGLFunctions_3_3_Core &gl) override;

private:
    QMutex mMutex;
};
