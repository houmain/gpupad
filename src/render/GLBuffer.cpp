#include "GLBuffer.h"

GLBuffer::GLBuffer(const Buffer &buffer)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
{
    mUsedItems += buffer.id;
}

bool GLBuffer::operator==(const GLBuffer &rhs) const
{
    return std::tie(mFileName, mData) ==
           std::tie(rhs.mFileName, rhs.mData);
}

GLuint GLBuffer::getReadOnlyBufferId()
{
    load();
    upload();
    return mBufferObject;
}

GLuint GLBuffer::getReadWriteBufferId()
{
    load();
    upload();
    mDeviceCopyModified = true;
    return mBufferObject;
}

void GLBuffer::bindReadOnly(GLenum target)
{
    load();
    upload();

    auto& gl = GLContext::currentContext();
    gl.glBindBuffer(target, mBufferObject);
}

void GLBuffer::unbind(GLenum target)
{
    auto& gl = GLContext::currentContext();
    gl.glBindBuffer(target, GL_NONE);
}

QList<std::pair<QString, QByteArray>> GLBuffer::getModifiedData()
{
    if (!download())
        return { };

    return { std::make_pair(mFileName, mData) };
}

void GLBuffer::load()
{
    auto prevData = mData;
    if (!Singletons::fileCache().getBinary(mFileName, &mData)) {
        mMessage = Singletons::messageList().insert(
            mItemId, MessageType::LoadingFileFailed, mFileName);
        return;
    }
    mMessage.reset();
    mSystemCopyModified = (mData != prevData);
}

void GLBuffer::upload()
{
    if (!mSystemCopyModified)
        return;

    auto& gl = GLContext::currentContext();
    auto createBuffer = [&]() {
        auto buffer = GLuint{ };
        gl.glGenBuffers(1, &buffer);
        return buffer;
    };
    auto freeBuffer = [](GLuint buffer) {
        auto& gl = GLContext::currentContext();
        gl.glDeleteBuffers(1, &buffer);
    };

    mBufferObject = GLObject(createBuffer(), freeBuffer);
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glBufferData(GL_ARRAY_BUFFER,
        mData.size(), mData.data(), GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool GLBuffer::download()
{
    if (!mDeviceCopyModified)
        return false;

    auto data = QByteArray(mData.size(), Qt::Uninitialized);
    auto& gl = GLContext::currentContext();
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_ARRAY_BUFFER, 0, data.size(), data.data());
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    mData = data;

    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}
