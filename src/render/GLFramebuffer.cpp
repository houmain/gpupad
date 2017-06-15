#include "GLFramebuffer.h"

void GLFramebuffer::initialize(PrepareContext &context,
    const Framebuffer &framebuffer)
{
    context.usedItems += framebuffer.id;

    for (const auto &item : framebuffer.items)
        if (auto attachment = castItem<Attachment>(item)) {
            context.usedItems += attachment->id;

            if (auto texture = context.session.findItem<Texture>(attachment->textureId)) {
                context.usedItems += texture->id;

                mTextures.push_back(GLTexture(*texture, context));

                mWidth = (!mWidth ? texture->width :
                    qMin(mWidth, texture->width));

                mHeight = (!mHeight ? texture->height :
                    qMin(mHeight, texture->height));
            }
        }
}

void GLFramebuffer::cache(RenderContext &context, GLFramebuffer &&update)
{
    mWidth = update.mWidth;
    mHeight = update.mHeight;

    if (updateList(mTextures, std::move(update.mTextures)))
        mFramebufferObject.reset();

    if (!mFramebufferObject)
        create(context);
}

void GLFramebuffer::create(RenderContext &context)
{
    if (mTextures.empty())
        return;

    auto createFBO = [&]() {
        auto fbo = GLuint{ };
        context.glGenFramebuffers(1, &fbo);
        return fbo;
    };
    auto freeFBO = [](GLuint fbo) {
        auto& gl = *QOpenGLContext::currentContext()->functions();
        gl.glDeleteFramebuffers(1, &fbo);
    };

    auto fbo = GLObject(createFBO(), freeFBO);
    context.glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    mNumColorAttachments = 0;
    for (auto& texture : mTextures) {
        auto attachment = GLenum(
            texture.isDepthTexture() ? GL_DEPTH_ATTACHMENT :
            texture.isSencilTexture() ? GL_STENCIL_ATTACHMENT :
            texture.isDepthSencilTexture() ? GL_DEPTH_STENCIL_ATTACHMENT :
            GL_COLOR_ATTACHMENT0 + mNumColorAttachments++);

        const auto level = 0;
        context.glFramebufferTexture(GL_FRAMEBUFFER, attachment,
            texture.getReadOnlyTextureId(context), level);
    }
    if (context.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        mFramebufferObject = std::move(fbo);

    context.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

bool GLFramebuffer::bind(RenderContext &context)
{
    if (!mFramebufferObject)
        return false;

    context.glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferObject);
    context.glViewport(0, 0, mWidth, mHeight);

    auto colorAttachments = std::vector<GLenum>();
    for (auto i = 0; i < mNumColorAttachments; ++i)
        colorAttachments.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
    context.glDrawBuffers(static_cast<GLsizei>(colorAttachments.size()),
      colorAttachments.data());

    for (auto& texture : mTextures)
        texture.getReadWriteTextureId(context);

    // TODO: move to clear call/states
    context.glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    context.glEnable(GL_DEPTH_TEST);
    context.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    context.glDisable(GL_BLEND);
    context.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    context.glDisable(GL_CULL_FACE);
    context.glDepthFunc(GL_LEQUAL);

    return true;
}

void GLFramebuffer::unbind(RenderContext &context)
{
    context.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);

    // resolve multisample textures
    for (auto& texture : mTextures)
        if (texture.target() == QOpenGLTexture::Target2DMultisample) {
            // TODO:
        }
}

QList<std::pair<QString, QImage>> GLFramebuffer::getModifiedImages(
    RenderContext &context)
{
    auto result = QList<std::pair<QString, QImage>>();
    for (auto& texture : mTextures)
        result += texture.getModifiedImages(context);
    return result;
}
