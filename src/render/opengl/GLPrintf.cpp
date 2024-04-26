
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

    // clear header in buffer, data[0] is already reserved for
    // head of linked list and prevBegin points to it
    auto header = BufferHeader{ 1, 0 };
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
    gl.glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(header), &header);
#endif // GL_VERSION_4_3
}

MessagePtrSet GLPrintf::formatMessages(ItemId callItemId)
{
    auto messages = MessagePtrSet();

#if GL_VERSION_4_3
    auto &gl = GLContext::currentContext();
    auto header = BufferHeader{ };
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferObject);
    gl.glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(header), &header);
    const auto count = (header.offset > maxBufferValues ? maxBufferValues : header.offset);
    const auto lastBegin = header.prevBegin;
    if (count <= 1)
        return { };

    auto data = std::vector<uint32_t>(count);
    gl.glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
        sizeof(BufferHeader), count * sizeof(uint32_t), data.data());
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, GL_NONE);

    auto readOutside = false;
    const auto read = [&](auto offset) -> uint32_t { 
        if (offset < count)
            return data[offset];
        readOutside = true;
        return { };
    };

    for (auto offset = data[0];;) {
        const auto lastMessage = (offset == lastBegin);
        const auto nextBegin = read(offset++);
        const auto &formatString = mFormatStrings[read(offset++)];
        const auto argumentCount = read(offset++);

        auto arguments = QList<Argument>();
        for (auto i = 0u; i < argumentCount; i++) {
            auto argumentOffset = read(offset++);
            const auto argumentType = read(argumentOffset++);
            const auto argumentComponents = argumentType % 100;
            auto argument = Argument{ argumentType, { } };
            for (auto j = 0u; j < argumentComponents; ++j)
                argument.values.append(read(argumentOffset++));
            arguments.append(argument);
        }
        if (readOutside)
            break;

        messages += MessageList::insert(formatString.fileName, formatString.line,
            MessageType::ShaderInfo, formatMessage(formatString, arguments), false);

        if (lastMessage)
            break;

        offset = nextBegin;
    }

    if (readOutside)
        messages += MessageList::insert(callItemId, MessageType::TooManyPrintfCalls);
#endif // GL_VERSION_4_3

    return messages;
}
