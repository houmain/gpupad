#pragma once

#include <memory>

class QOpenGLFunctions_3_3_Core;
using ShareSyncPtr = std::shared_ptr<class ShareSync>;

class ShareSync
{
public:
    ~ShareSync() = default;
    virtual void beginUsage(QOpenGLFunctions_3_3_Core &gl) = 0;
    virtual void endUsage(QOpenGLFunctions_3_3_Core &gl) = 0;
};
