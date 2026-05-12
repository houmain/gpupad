#pragma once

#include <memory>

using ShareSyncPtr = std::shared_ptr<class ShareSync>;

class ShareSync
{
public:
    virtual ~ShareSync() = default;
    virtual void beginUsage() = 0;
    virtual void endUsage() = 0;
};
