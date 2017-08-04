#include "GLBuffer.h"

GLBuffer::GLBuffer(const Buffer &buffer)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
    , mOffset(buffer.offset)
    , mSize(buffer.rowCount * getStride(buffer))
{
    mUsedItems += buffer.id;
}

bool GLBuffer::operator==(const GLBuffer &rhs) const
{
    return std::tie(mFileName, mOffset, mSize) ==
           std::tie(rhs.mFileName, rhs.mOffset, rhs.mSize);
}

void GLBuffer::clear()
{
    auto& gl = GLContext::currentContext();
    if (auto gl43 = check(gl.v4_3, mItemId, mMessages)) {
        auto data = uint8_t();
        gl.glBindBuffer(GL_ARRAY_BUFFER, getReadWriteBufferId());
        gl43->glClearBufferData(GL_ARRAY_BUFFER, GL_R8,
            GL_RED, GL_UNSIGNED_BYTE, &data);
        gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    }
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

QList<std::pair<ItemId, QByteArray>> GLBuffer::getModifiedData()
{
    if (!download())
        return { };

    return { std::make_pair(mItemId, mData) };
}

void GLBuffer::load()
{
    auto prevData = mData;
    if (!FileDialog::isEmptyOrUntitled(mFileName))
        if (!Singletons::fileCache().getBinary(mFileName, &mData))
            mMessages += Singletons::messageList().insert(
                mItemId, MessageType::LoadingFileFailed, mFileName);

    auto requiredSize = mOffset + mSize;
    if (requiredSize > mData.size())
        mData.append(QByteArray(requiredSize - mData.size(), 0));

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

    Q_ASSERT(mOffset + mSize <= mData.size());
    mBufferObject = GLObject(createBuffer(), freeBuffer);
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glBufferData(GL_ARRAY_BUFFER,
        mSize, mData.data() + mOffset, GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool GLBuffer::download()
{
    if (!mDeviceCopyModified)
        return false;

    Q_ASSERT(mOffset + mSize <= mData.size());
    auto& gl = GLContext::currentContext();
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_ARRAY_BUFFER,
        0, mSize, mData.data() + mOffset);
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}
