#pragma once

#include "editors/texture/TextureEditorBackground.h"
#include <memory>

class VKWindow;

class VKTextureEditorBackground final : public TextureEditorBackground
{
public:
    explicit VKTextureEditorBackground(VKWindow *window);
    ~VKTextureEditorBackground() override;

    void releaseGpu() override;
    void paintGpu(const QSizeF &size, const QPointF &offset) override;

private:
    struct Pipeline;

    VKWindow &mWindow;
    std::unique_ptr<Pipeline> mPipeline;
};
