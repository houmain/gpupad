#include "GLBuffer.h"

int getBufferSize(const Buffer &buffer,
    ScriptEngine &scriptEngine, MessagePtrSet &messages)
{
    auto size = 1;
    for (const Item* item : buffer.items) {
        const auto &block = *static_cast<const Block*>(item);
        const auto blockOffset = scriptEngine.evaluateInt(block.offset, block.id, messages);
        const auto blockRowCount = scriptEngine.evaluateInt(block.rowCount, block.id, messages);
        size = std::max(size,
            blockOffset + blockRowCount * getBlockStride(block));
        
        block.evaluatedOffset = blockOffset;
        block.evaluatedRowCount = blockRowCount;
    }
    return size;
}

GLBuffer::GLBuffer(const Buffer &buffer,
      ScriptEngine &scriptEngine, MessagePtrSet &messages)
    : mItemId(buffer.id)
    , mFileName(buffer.fileName)
    , mSize(getBufferSize(buffer, scriptEngine, messages))
{
    mUsedItems += buffer.id;
    for (const auto item : buffer.items)
        if (auto block = static_cast<const Block*>(item)) {
            mUsedItems += block->id;
            for (const auto item : block->items)
                if (auto field = static_cast<const Block*>(item))
                    mUsedItems += field->id;
        }
}

void GLBuffer::updateUntitledFilename(const GLBuffer &rhs)
{
    if (mSize == rhs.mSize &&
        FileDialog::isEmptyOrUntitled(mFileName) &&
        FileDialog::isEmptyOrUntitled(rhs.mFileName))
        mFileName = rhs.mFileName;
}

bool GLBuffer::operator==(const GLBuffer &rhs) const
{
    return std::tie(mFileName, mSize) ==
           std::tie(rhs.mFileName, rhs.mSize);
}

void GLBuffer::clear()
{
    auto &gl = GLContext::currentContext();
    if (auto gl43 = check(gl.v4_3, mItemId, mMessages)) {
        auto data = uint8_t();
        gl.glBindBuffer(GL_ARRAY_BUFFER, getReadWriteBufferId());
        gl43->glClearBufferData(GL_ARRAY_BUFFER, GL_R8,
            GL_RED, GL_UNSIGNED_BYTE, &data);
        gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    }
}

void GLBuffer::copy(GLBuffer &source)
{
    auto &gl = GLContext::currentContext();
    gl.glBindBuffer(GL_COPY_READ_BUFFER, source.getReadOnlyBufferId());
    gl.glBindBuffer(GL_COPY_WRITE_BUFFER, getReadWriteBufferId());
    gl.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
        0, 0, std::min(source.mSize, mSize));
    gl.glBindBuffer(GL_COPY_READ_BUFFER, GL_NONE);
    gl.glBindBuffer(GL_COPY_WRITE_BUFFER, GL_NONE);
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

void GLBuffer::bindIndexedRange(GLenum target, int index, int offset, int size, bool readonly)
{
    const auto bufferObject = (readonly ?
        getReadOnlyBufferId() : getReadWriteBufferId());
    auto &gl = GLContext::currentContext();
    if (size <= 0) {
        gl.glBindBufferBase(target, index, bufferObject);
    }
    else {
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
    mMessages.clear();

    auto prevData = mData;
    if (!mFileName.isEmpty())
        if (!Singletons::fileCache().getBinary(mFileName, &mData))
            if (!FileDialog::isEmptyOrUntitled(mFileName))
                mMessages += MessageList::insert(
                    mItemId, MessageType::LoadingFileFailed, mFileName);

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

bool GLBuffer::download()
{
    if (!mDeviceCopyModified)
        return false;

    const auto prevData = mData;
    auto &gl = GLContext::currentContext();
    gl.glBindBuffer(GL_ARRAY_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_ARRAY_BUFFER, 0, mSize, mData.data());
    gl.glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);

    if (prevData == mData) {
        mData = prevData;
        return false;
    }

    mSystemCopyModified = mDeviceCopyModified = false;
    return true;
}
