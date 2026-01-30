#pragma once

#include <memory>

class QOpenGLFunctions_4_5_Core;
using ShareSyncPtr = std::shared_ptr<class ShareSync>;

class ShareSync
{
public:
    ~ShareSync() = default;
    virtual void beginUsage(QOpenGLFunctions_4_5_Core &gl) = 0;
    virtual void endUsage(QOpenGLFunctions_4_5_Core &gl) = 0;
};
