#include "GLBuffer.h"
#include "Singletons.h"

GLBuffer::GLBuffer(int size) : BufferBase(size) { }

GLBuffer::GLBuffer(const Buffer &buffer, GLRenderSession &renderSession)
    : BufferBase(buffer, renderSession.getBufferSize(buffer))
{
    mUsedItems += buffer.id;
    for (const auto item : buffer.items)
        if (auto block = static_cast<const Block *>(item)) {
            mUsedItems += block->id;
            for (const auto item : block->items)
                if (auto field = static_cast<const Block *>(item))
                    mUsedItems += field->id;
        }
}

QByteArray &GLBuffer::getWriteableData()
{
    Q_ASSERT(mFileName.isEmpty());
    mData.resize(mSize);
    mSystemCopyModified = true;
    return mData;
}

void GLBuffer::clear()
{
    auto &gl = GLContext::currentContext();
    if (auto gl43 = check(gl.v4_3, mItemId, mMessages)) {
        auto data = uint8_t();
        gl.glBindBuffer(GL_ARRAY_BUFFER, getReadWriteBufferId());
        gl43->glClearBufferData(GL_ARRAY_BUFFER, GL_R8, GL_RED,
            GL_UNSIGNED_BYTE, &data);
        gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    }
}

void GLBuffer::copy(GLBuffer &source)
{
    auto &gl = GLContext::currentContext();
    gl.glBindBuffer(GL_COPY_READ_BUFFER, source.getReadOnlyBufferId());
    gl.glBindBuffer(GL_COPY_WRITE_BUFFER, getReadWriteBufferId());
    gl.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0,
        std::min(source.mSize, mSize));
    gl.glBindBuffer(GL_COPY_READ_BUFFER, GL_NONE);
    gl.glBindBuffer(GL_COPY_WRITE_BUFFER, GL_NONE);
}

bool GLBuffer::swap(GLBuffer &other)
{
    if (mSize != other.mSize)
        return false;
    mData.swap(other.mData);
    std::swap(mBufferObject, other.mBufferObject);
    std::swap(mSystemCopyModified, other.mSystemCopyModified);
    std::swap(mDeviceCopyModified, other.mDeviceCopyModified);
    return true;
}

GLuint GLBuffer::getReadOnlyBufferId()
{
    reload();
    createBuffer();
    upload();
    return mBufferObject;
}

GLuint GLBuffer::getReadWriteBufferId()
{
    reload();
    createBuffer();
    upload();
    mDeviceCopyModified = true;
    return mBufferObject;
}

void GLBuffer::bindIndexedRange(GLenum target, int index, int offset, int size,
    bool readonly)
{
    const auto bufferObject =
        (readonly ? getReadOnlyBufferId() : getReadWriteBufferId());
    auto &gl = GLContext::currentContext();
    if (size <= 0) {
        gl.glBindBufferBase(target, index, bufferObject);
    } else {
        gl.glBindBufferRange(target, index, bufferObject, offset, size);
    }
}

void GLBuffer::bindReadOnly(GLenum target)
{
    auto &gl = GLContext::currentContext();
    gl.glBindBuffer(target, getReadOnlyBufferId());
}

void GLBuffer::unbind(GLenum target)
{
    auto &gl = GLContext::currentContext();
    gl.glBindBuffer(target, GL_NONE);
}

void GLBuffer::reload()
{
    auto prevData = mData;
    if (!mFileName.isEmpty())
        if (!Singletons::fileCache().getBinary(mFileName, &mData))
            if (!FileDialog::isEmptyOrUntitled(mFileName))
                mMessages += MessageList::insert(mItemId,
                    MessageType::LoadingFileFailed, mFileName);

    if (mSize > mData.size())
        mData.append(QByteArray(mSize - mData.size(), 0));

    mSystemCopyModified |= !mData.isSharedWith(prevData);
}

void GLBuffer::createBuffer()
{
    if (mBufferObject)
        return;

    auto &gl = GLContext::currentContext();
    auto createBuffer = [&]() {
        auto buffer = GLuint{};
        gl.glGenBuffers(1, &buffer);
        return buffer;
    };
    auto freeBuffer = [](GLuint buffer) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteBuffers(1, &buffer);
    };

    mBufferObject = GLObject(createBuffer(), freeBuffer);
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glBufferData(GL_ARRAY_BUFFER, mSize, nullptr, GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
}

void GLBuffer::upload()
{
    if (!mSystemCopyModified)
        return;

    auto &gl = GLContext::currentContext();
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glBufferSubData(GL_ARRAY_BUFFER, 0, mSize, mData.constData());
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;
}

bool GLBuffer::download(GLContext &gl, bool checkModification)
{
    if (!mDeviceCopyModified)
        return false;

    auto prevData = QByteArray();
    if (checkModification)
        prevData = mData;

    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_ARRAY_BUFFER, 0, mSize, mData.data());
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;

    if (checkModification && prevData == mData) {
        mData = prevData;
        return false;
    }
    return true;
}
