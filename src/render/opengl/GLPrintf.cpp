
#include "GLPrintf.h"

void GLPrintf::clear()
{
#if GL_VERSION_4_3
    auto& gl = GLContext::currentContext();
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
        gl.glBufferData(GL_SHADER_STORAGE_BUFFER,
            maxBufferValues * sizeof(uint32_t) + sizeof(BufferHeader),
            nullptr, GL_STREAM_READ);
    }

    auto header = initializeHeader();
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
    gl.glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(header), &header);
#endif // GL_VERSION_4_3
}

MessagePtrSet GLPrintf::formatMessages(ItemId callItemId)
{
#if GL_VERSION_4_3
    auto &gl = GLContext::currentContext();
    auto header = BufferHeader{ };
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(header), &header);
    const auto count = (header.offset > maxBufferValues ? maxBufferValues : header.offset);
    if (count <= 1)
        return { };

    auto data = std::vector<uint32_t>(count);
    gl.glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
        sizeof(BufferHeader), count * sizeof(uint32_t), data.data());
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, GL_NONE);

    return ShaderPrintf::formatMessages(callItemId, header, data);
#else
    return { };
#endif // GL_VERSION_4_3
}
