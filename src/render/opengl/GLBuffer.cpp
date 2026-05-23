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

void GLBuffer::clear(GLContext &gl)
{
    auto data = uint8_t();
    gl.glBindBuffer(GL_ARRAY_BUFFER, getReadWriteBufferId(gl));
    gl.glClearBufferData(GL_ARRAY_BUFFER, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
        &data);
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
}

void GLBuffer::copy(GLContext &gl, GLBuffer &source)
{
    gl.glBindBuffer(GL_COPY_READ_BUFFER, source.getReadOnlyBufferId(gl));
    gl.glBindBuffer(GL_COPY_WRITE_BUFFER, getReadWriteBufferId(gl));
    gl.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0,
        std::min(source.mSize, mSize));
    gl.glBindBuffer(GL_COPY_READ_BUFFER, GL_NONE);
    gl.glBindBuffer(GL_COPY_WRITE_BUFFER, GL_NONE);
}

bool GLBuffer::swap(GLBuffer &other)
{
    if (!BufferBase::swap(other))
        return false;
    std::swap(mBufferObject, other.mBufferObject);
    return true;
}

GLuint GLBuffer::getReadOnlyBufferId(GLContext &gl)
{
    reload();
    createBuffer(gl);
    upload(gl);
    return mBufferObject;
}

GLuint GLBuffer::getReadWriteBufferId(GLContext &gl)
{
    reload();
    createBuffer(gl);
    upload(gl);
    mDeviceCopyModified = true;
    return mBufferObject;
}

void GLBuffer::bindIndexedRange(GLContext &gl, GLenum target, int index,
    int offset, int size, bool readonly)
{
    const auto bufferObject =
        (readonly ? getReadOnlyBufferId(gl) : getReadWriteBufferId(gl));
    if (size <= 0) {
        gl.glBindBufferBase(target, index, bufferObject);
    } else {
        gl.glBindBufferRange(target, index, bufferObject, offset, size);
    }
}

void GLBuffer::bindReadOnly(GLContext &gl, GLenum target)
{
    gl.glBindBuffer(target, getReadOnlyBufferId(gl));
}

void GLBuffer::unbind(GLContext &gl, GLenum target)
{
    gl.glBindBuffer(target, GL_NONE);
}

void GLBuffer::reload()
{
    auto prevData = mData;
    if (!mFileName.isEmpty())
        if (!Singletons::fileCache().getBinary(mFileName, &mData))
            if (!FileDialog::isEmptyOrUntitled(mFileName))
                mMessages.insert(mItemId, MessageType::LoadingFileFailed,
                    mFileName);

    if (mSize > mData.size())
        mData.append(QByteArray(mSize - mData.size(), 0));

    mSystemCopyModified |= !mData.isSharedWith(prevData);
}

void GLBuffer::createBuffer(GLContext &gl)
{
    if (mBufferObject)
        return;

    auto createBuffer = [&]() {
        auto buffer = GLuint{};
        gl.glGenBuffers(1, &buffer);
        return buffer;
    };
    auto freeBuffer = [](GLContext &gl, GLuint buffer) {
        gl.glDeleteBuffers(1, &buffer);
    };

    mBufferObject = GLObject(&gl, createBuffer(), freeBuffer);
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glBufferData(GL_ARRAY_BUFFER, mSize, nullptr, GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
}

void GLBuffer::upload(GLContext &gl)
{
    if (!mSystemCopyModified)
        return;

    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glBufferSubData(GL_ARRAY_BUFFER, 0, mSize, mData.constData());
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;
}

void GLBuffer::beginDownload(GLContext &gl, bool checkModification)
{
    if (!mDeviceCopyModified)
        return;

    auto prevData = QByteArray();
    if (checkModification)
        prevData = mData;

    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_ARRAY_BUFFER, 0, mSize, mData.data());
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    mSystemCopyModified = mDeviceCopyModified = false;

    if (checkModification && prevData == mData) {
        mData = prevData;
        return;
    }
    mDownloaded = true;
}

bool GLBuffer::finishDownload()
{
    return std::exchange(mDownloaded, false);
}
