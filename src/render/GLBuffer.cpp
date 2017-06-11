#include "GLBuffer.h"

GLBuffer::GLBuffer(const Buffer &buffer, PrepareContext &context)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
{
    Q_UNUSED(context);
}

bool GLBuffer::operator==(const GLBuffer &rhs) const
{
    return std::tie(mFileName, mData) ==
           std::tie(rhs.mFileName, rhs.mData);
}

GLuint GLBuffer::getReadOnlyBufferId(RenderContext &context)
{
    load(context.messages);
    upload(context);
    return mBufferObject;
}

GLuint GLBuffer::getReadWriteBufferId(RenderContext &context)
{
    load(context.messages);
    upload(context);
    mDeviceCopyModified = true;
    return mBufferObject;
}

void GLBuffer::bindReadOnly(RenderContext &context, GLenum target)
{
    load(context.messages);
    upload(context);
    context.glBindBuffer(target, mBufferObject);
}

void GLBuffer::unbind(RenderContext &context, GLenum target)
{
    context.glBindBuffer(target, GL_NONE);
}

QList<std::pair<QString, QByteArray>> GLBuffer::getModifiedData(
    RenderContext &context)
{
    if (!download(context))
        return { };

    return { std::make_pair(mFileName, mData) };
}

void GLBuffer::load(MessageList &messages) {
    if (!mData.isNull())
        return;

    if (!Singletons::fileCache().getBinary(mFileName, &mData)) {
        messages.setContext(mItemId);
        return messages.insert(MessageType::LoadingFileFailed, mFileName);
    }
    mSystemCopyModified = true;
}

void GLBuffer::upload(RenderContext &context)
{
    if (!mSystemCopyModified)
        return;

    auto createBuffer = [&]() {
        auto buffer = GLuint{ };
        context.glGenBuffers(1, &buffer);
        return buffer;
    };
    auto freeBuffer = [](GLuint buffer) {
        auto& gl = *QOpenGLContext::currentContext()->functions();
        gl.glDeleteBuffers(1, &buffer);
    };

    mBufferObject = GLObject(createBuffer(), freeBuffer);
    context.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    context.glBufferData(GL_ARRAY_BUFFER,
        mData.size(), mData.data(), GL_DYNAMIC_DRAW);
    context.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool GLBuffer::download(RenderContext &context)
{
    if (!mDeviceCopyModified)
        return false;

    auto data = QByteArray(mData.size(), Qt::Uninitialized);
    context.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    context.glGetBufferSubData(GL_ARRAY_BUFFER, 0, data.size(), data.data());
    context.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    mData = data;

    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}
