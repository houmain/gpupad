#include "GLFramebuffer.h"

GLFramebuffer::GLFramebuffer(const Framebuffer &framebuffer)
    : mItemId(framebuffer.id)
{
    mUsedItems += framebuffer.id;

    for (const auto &item : framebuffer.items)
        if (auto attachment = castItem<Attachment>(item)) {
            mUsedItems += attachment->id;
            mTextures.push_back(nullptr);
        }
}

void GLFramebuffer::setAttachment(int index, GLTexture *texture)
{
    if (!texture)
        return;

    mTextures.at(index) = texture;
    mWidth = (!mWidth ? texture->width() : qMin(mWidth, texture->width()));
    mHeight = (!mHeight ? texture->height() : qMin(mHeight, texture->height()));
}

bool GLFramebuffer::bind()
{
    if (!create())
        return false;

    auto &gl = GLContext::currentContext();
    gl.glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferObject);
    gl.glViewport(0, 0, mWidth, mHeight);

    auto colorAttachments = std::vector<GLenum>();
    for (auto i = 0; i < mNumColorAttachments; ++i)
        colorAttachments.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
    gl.glDrawBuffers(static_cast<GLsizei>(colorAttachments.size()),
      colorAttachments.data());

    // mark texture device copies as modified
    for (auto& texture : mTextures) {
        texture->getReadWriteTextureId();
        mUsedItems += texture->usedItems();
    }

    // TODO: move to clear... calls
    gl.glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    gl.glEnable(GL_DEPTH_TEST);
    gl.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.glDisable(GL_BLEND);
    gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.glDisable(GL_CULL_FACE);
    gl.glDepthFunc(GL_LEQUAL);
    return true;
}

void GLFramebuffer::unbind()
{
    auto &gl = GLContext::currentContext();
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

bool GLFramebuffer::create()
{
    if (mFramebufferObject)
        return true;

    auto &gl = GLContext::currentContext();
    auto createFBO = [&]() {
        auto fbo = GLuint{ };
        gl.glGenFramebuffers(1, &fbo);
        return fbo;
    };
    auto freeFBO = [](GLuint fbo) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteFramebuffers(1, &fbo);
    };

    mNumColorAttachments = 0;
    mFramebufferObject = GLObject(createFBO(), freeFBO);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferObject);

    for (auto& texture : mTextures) {
        auto attachment = GLenum(
            texture->isDepthTexture() ? GL_DEPTH_ATTACHMENT :
            texture->isSencilTexture() ? GL_STENCIL_ATTACHMENT :
            texture->isDepthSencilTexture() ? GL_DEPTH_STENCIL_ATTACHMENT :
            GL_COLOR_ATTACHMENT0 + mNumColorAttachments++);

        auto level = 0;
        auto textureId = texture->getReadOnlyTextureId();
        gl.glFramebufferTexture(GL_FRAMEBUFFER, attachment, textureId, level);
    }

    if (gl.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        mMessage = Singletons::messageList().insert(mItemId,
            MessageType::CreatingFramebufferFailed);
        mFramebufferObject.reset();
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    return mFramebufferObject;
}
