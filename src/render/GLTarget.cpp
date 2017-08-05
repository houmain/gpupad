#include "GLTarget.h"

GLTarget::GLTarget(const Target &target)
    : mItemId(target.id)
    , mFrontFace(target.frontFace)
    , mCullMode(target.cullMode)
    , mLogicOperation(target.logicOperation)
    , mBlendConstant(target.blendConstant)
{
    mUsedItems += target.id;

    auto attachmentIndex = 0;
    for (const auto &item : target.items) {
        if (auto attachment = castItem<Attachment>(item)) {
            static_cast<Attachment&>(mAttachments[attachmentIndex]) = *attachment;
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
    for (const auto& attachment : mAttachments)
        if (attachment.texture && attachment.texture->kind().color)
            colorAttachments.push_back(attachment.attachmentPoint);
    gl.glDrawBuffers(static_cast<GLsizei>(colorAttachments.size()),
        colorAttachments.data());

    // mark texture device copies as modified
    for (const auto& attachment : mAttachments)
        if (auto texture = attachment.texture)
            texture->getReadWriteTextureId();

    applyStates();
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

    auto nextColorAttachment = GLenum(GL_COLOR_ATTACHMENT0);
    for (auto& attachment : mAttachments)
        if (auto texture = attachment.texture) {
            auto kind = texture->kind();
            auto level = attachment.level;

            if (kind.depth && kind.stencil) {
                attachment.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;
                level = 0;
            }
            else if (kind.depth) {
                attachment.attachmentPoint = GL_DEPTH_ATTACHMENT;
                level = 0;
            }
            else if (kind.stencil) {
                attachment.attachmentPoint = GL_STENCIL_ATTACHMENT;
                level = 0;
            }
            else {
                attachment.attachmentPoint = nextColorAttachment++;
            }

            if (kind.array && attachment.layered) {
                gl.glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment.attachmentPoint,
                    texture->getReadOnlyTextureId(), level, attachment.layer);
            }
            else {
                gl.glFramebufferTexture(GL_FRAMEBUFFER, attachment.attachmentPoint,
                    texture->getReadOnlyTextureId(), level);
            }
            mUsedItems += texture->usedItems();
        }

    auto status = gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        mMessages += Singletons::messageList().insert(mItemId,
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

void GLTarget::applyStates()
{
    auto &gl = GLContext::currentContext();

    gl.glFrontFace(mFrontFace);
    if (mCullMode != Target::CullDisabled) {
        gl.glEnable(GL_CULL_FACE);
        gl.glCullFace(mCullMode);
    }
    else {
        gl.glDisable(GL_CULL_FACE);
    }

    if (mLogicOperation != Target::LogicOperationDisabled) {
      gl.glEnable(GL_COLOR_LOGIC_OP);
      gl.glLogicOp(mLogicOperation);
    }
    else {
        gl.glDisable(GL_COLOR_LOGIC_OP);
    }
    gl.glBlendColor(mBlendConstant.redF(), mBlendConstant.blueF(),
                    mBlendConstant.greenF(), mBlendConstant.alphaF());

    gl.glEnable(GL_PROGRAM_POINT_SIZE);
    gl.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    auto minWidth = 0;
    auto minHeight = 0;
    for (const auto& attachment : mAttachments)
        if (auto texture = attachment.texture) {
            auto width = std::max(texture->width() >> attachment.level, 1);
            auto height = std::max(texture->height() >> attachment.level, 1);
            minWidth = (!minWidth ? width : std::min(minWidth, width));
            minHeight = (!minHeight ? height : std::min(minHeight, height));
            applyAttachmentStates(attachment);
        }
    gl.glViewport(0, 0, minWidth, minHeight);
}

void GLTarget::applyAttachmentStates(const GLAttachment &a)
{
    auto &gl = GLContext::currentContext();
    auto kind = a.texture->kind();

    if (kind.color) {
        auto index = a.attachmentPoint - GL_COLOR_ATTACHMENT0;
        if (index == 0) {
            gl.glEnable(GL_BLEND);
            gl.glBlendEquationSeparate(a.blendColorEq, a.blendAlphaEq);
            gl.glBlendFuncSeparate(a.blendColorSource, a.blendColorDest,
                a.blendAlphaSource, a.blendAlphaDest);
        }
        else if (auto gl40 = check(gl.v4_0, mItemId, mMessages)) {
            gl40->glEnablei(GL_BLEND, index);
            gl40->glBlendEquationSeparatei(index, a.blendColorEq, a.blendAlphaEq);
            gl40->glBlendFuncSeparatei(index, a.blendColorSource, a.blendColorDest,
                a.blendAlphaSource, a.blendAlphaDest);
        }

        auto isSet = [](auto v, auto bit) { return (v & (1 << bit)) != 0; };
        gl.glColorMaski(index,
            isSet(a.colorWriteMask, 0),
            isSet(a.colorWriteMask, 1),
            isSet(a.colorWriteMask, 2),
            isSet(a.colorWriteMask, 3));
    }

    if (kind.depth) {
        gl.glEnable(GL_DEPTH_TEST);
        gl.glDepthFunc(a.depthComparisonFunc);
        gl.glEnable(GL_POLYGON_OFFSET_POINT);
        gl.glEnable(GL_POLYGON_OFFSET_LINE);
        gl.glEnable(GL_POLYGON_OFFSET_FILL);
        gl.glPolygonOffset(a.depthOffsetFactor, a.depthOffsetUnits);
        if (a.depthClamp)
            gl.glEnable(GL_DEPTH_CLAMP);
        else
            gl.glDisable(GL_DEPTH_CLAMP);
        gl.glDepthMask(a.depthWrite);
    }

    if (kind.stencil) {
        gl.glEnable(GL_STENCIL_TEST);
        if (auto gl40 = gl.v4_0) {
            gl40->glStencilFuncSeparate(GL_FRONT, a.stencilFrontComparisonFunc,
                a.stencilFrontReference, a.stencilFrontReadMask);
            gl40->glStencilOpSeparate(GL_FRONT, a.stencilFrontFailOp,
                a.stencilFrontDepthFailOp, a.stencilFrontDepthPassOp);
            gl40->glStencilMaskSeparate(GL_FRONT, a.stencilFrontWriteMask);

            gl40->glStencilFuncSeparate(GL_BACK, a.stencilBackComparisonFunc,
                a.stencilBackReference, a.stencilFrontReadMask);
            gl40->glStencilOpSeparate(GL_BACK, a.stencilBackFailOp,
                a.stencilBackDepthFailOp, a.stencilBackDepthPassOp);
            gl40->glStencilMaskSeparate(GL_BACK, a.stencilBackWriteMask);
        }
        else {
            gl40->glStencilFunc(a.stencilFrontComparisonFunc,
                a.stencilFrontReference, a.stencilFrontReadMask);
            gl40->glStencilOp(a.stencilFrontFailOp,
                a.stencilFrontDepthFailOp, a.stencilFrontDepthPassOp);
            gl40->glStencilMask( a.stencilFrontWriteMask);
        }
    }
}
