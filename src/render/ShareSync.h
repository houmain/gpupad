#pragma once

#include <memory>

using ShareSyncPtr = std::shared_ptr<class ShareSync>;

class ShareSync
{
public:
    virtual ~ShareSync() = default;
    virtual void beginUsage(void *context) = 0;
    virtual void endUsage(void *context) = 0;
};
