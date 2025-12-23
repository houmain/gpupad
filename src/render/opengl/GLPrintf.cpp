
#include "GLPrintf.h"

void GLPrintf::clear()
{
#if GL_VERSION_4_3
    auto &gl = GLContext::currentContext();
    const auto createBuffer = [&]() {
        auto buffer = GLuint{};
        gl.glGenBuffers(1, &buffer);
        return buffer;
    };
    const auto freeBuffer = [](GLuint buffer) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteBuffers(1, &buffer);
    };

    if (!mBufferObject) {
        mBufferObject = GLObject(createBuffer(), freeBuffer);
        gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
        gl.glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr,
            GL_STREAM_READ);
    }

    auto header = initializeHeader();
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
    gl.glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(header), &header);
#endif // GL_VERSION_4_3
}

void GLPrintf::beginDownload(GLContext &gl)
{
#if GL_VERSION_4_3
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(mHeader),
        &mHeader);
    const auto count =
        (mHeader.offset > maxBufferValues ? maxBufferValues : mHeader.offset);

    mData.resize(count);
    if (!count)
        return;

    gl.glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(BufferHeader),
        count * sizeof(uint32_t), mData.data());
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, GL_NONE);
#endif // GL_VERSION_4_3
}

MessagePtrSet GLPrintf::finishDownload(ItemId callItemId)
{
    return ShaderPrintf::formatMessages(callItemId, mHeader, mData);
}
