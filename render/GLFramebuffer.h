#ifndef GLFRAMEBUFFER_H
#define GLFRAMEBUFFER_H

#include "GLTexture.h"

class GLFramebuffer
{
public:
    void initialize(PrepareContext &context, const Framebuffer &framebuffer);

    void cache(RenderContext &context, GLFramebuffer &&update);
    bool bind(RenderContext &context);
    void unbind(RenderContext &context);
    QList<std::pair<QString, QImage>> getModifiedImages(RenderContext &context);

private:
    void create(RenderContext &context);
    void reset();

    int mWidth{ };
    int mHeight{ };
    std::vector<GLTexture> mTextures;
    int mNumColorAttachments{ };
    GLObject mFramebufferObject;
};

#endif // GLFRAMEBUFFER_H
