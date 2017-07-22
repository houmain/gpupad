#include "GLTarget.h"

GLTarget::GLTarget(const Target &target)
    : mItemId(target.id)
{
    mUsedItems += target.id;

    auto attachmentIndex = 0;
    for (const auto &item : target.items) {
        if (auto attachment = castItem<Attachment>(item)) {
            mAttachments[attachmentIndex] = GLAttachment{
                attachment->level,
                nullptr
            };
            mUsedItems += attachment->id;
        }
        attachmentIndex++;
    }
}

void GLTarget::setAttachment(int index, GLTexture *texture)
{
    mAttachments[index].texture = texture;
}

bool GLTarget::bind()
{
    if (!create())
        return false;

    auto &gl = GLContext::currentContext();
    gl.glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferObject);

    auto colorAttachments = std::vector<GLenum>();
    for (auto i = 0; i < mNumColorAttachments; ++i)
        colorAttachments.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
    gl.glDrawBuffers(static_cast<GLsizei>(colorAttachments.size()),
        colorAttachments.data());

    auto minWidth = 0;
    auto minHeight = 0;
    for (const auto& attachment : mAttachments)
        if (auto texture = attachment.texture) {
            auto width = std::max(texture->width() >> attachment.level, 1);
            auto height = std::max(texture->height() >> attachment.level, 1);
            minWidth = (!minWidth ? width : std::min(minWidth, width));
            minHeight = (!minHeight ? height : std::min(minHeight, height));

            // mark texture device copies as modified
            texture->getReadWriteTextureId();
        }
    gl.glViewport(0, 0, minWidth, minHeight);

    // TODO: move to states
    gl.glEnable(GL_DEPTH_TEST);
    gl.glEnable(GL_BLEND);
    gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.glDisable(GL_CULL_FACE);
    gl.glDepthFunc(GL_LEQUAL);
    return true;
}

void GLTarget::unbind()
{
    auto &gl = GLContext::currentContext();
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

bool GLTarget::create()
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

    mFramebufferObject = GLObject(createFBO(), freeFBO);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferObject);

    mNumColorAttachments = 0;
    for (const auto& attachment : mAttachments)
        if (auto texture = attachment.texture) {
            auto type = GLenum(
                texture->type() == Texture::Type::Depth ? GL_DEPTH_ATTACHMENT :
                texture->type() == Texture::Type::Stencil ? GL_STENCIL_ATTACHMENT :
                texture->type() == Texture::Type::DepthStencil ? GL_DEPTH_STENCIL_ATTACHMENT :
                GL_COLOR_ATTACHMENT0 + mNumColorAttachments++);

            auto textureId = texture->getReadOnlyTextureId();
            gl.glFramebufferTexture(GL_FRAMEBUFFER, type, textureId, attachment.level);
            mUsedItems += texture->usedItems();
        }

    auto status = gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        mMessage = Singletons::messageList().insert(mItemId,
            MessageType::CreatingFramebufferFailed,
            (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ? "(incomplete attachment)" :
             status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT ? "(missing attachment)" :
             status == GL_FRAMEBUFFER_UNSUPPORTED ? "(unsupported)" :
             status == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE ? "(sample mismatch)" : ""));
        mFramebufferObject.reset();
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    return mFramebufferObject;
}
