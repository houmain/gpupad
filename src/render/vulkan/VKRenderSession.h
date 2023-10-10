#pragma once

#include "../RenderSessionBase.h"

class VKRenderer;
class ScriptSession;

class VKRenderSession final : public RenderSessionBase
{
public:
    explicit VKRenderSession(VKRenderer &renderer);
    ~VKRenderSession();

    void render() override;
    void finish() override;
    void release() override;

private:
    VKRenderer &mRenderer;
};
