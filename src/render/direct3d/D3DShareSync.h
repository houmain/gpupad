#pragma once

#if defined(_WIN32)

#  include "D3DContext.h"
#  include "render/ShareSync.h"
#  include <QMutex>

class D3DShareSync : public ShareSync
{
public:
    explicit D3DShareSync(ID3D12Device &device);
    void cleanup();
    void beginUpdate();
    void endUpdate();
    void beginUsage(void *context) override;
    void endUsage(void *context) override;

private:
    QMutex mMutex;
};

#endif // _WIN32
