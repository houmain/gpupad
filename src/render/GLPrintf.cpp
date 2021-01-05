#include "GLPrintf.h"
#include <QVector>
#include <array>
#include <cstring>

namespace {
    template <typename It>
    bool skip(It &it, const It &end,char c)
    {
        if (it != end && *it == c) {
            ++it;
            return true;
        }
        return false;
    }

    template <typename It>
    bool skip(It &it, const It &end, const char* str)
    {
        const auto start = it;
        while (*str) {
            if (it == end || *str != *it) {
                it = start;
                return false;
            }
            ++str;
            ++it;
        }
        return true;
    }

    template <typename It, typename T>
    bool skipUntil(It &it, const It &end, T str)
    {
        for (; it != end; ++it)
            if (skip(it, end, str))
                return true;
        return false;
    }

    template <typename It>
    bool skipLine(It &it, const It &end)
    {
        const auto start = it;
        auto afterEscape = false;
        while (it != end) {
            if (skip(it, end, '\\')) {
                afterEscape = true;
            }
            else {
                if (skip(it, end, "\r\n") ||
                    skip(it, end, "\n\r") ||
                    skip(it, end, '\n')) {
                    if (!afterEscape)
                        return true;
                }
                afterEscape = false;
                ++it;
            }
        }
        it = start;
        return false;
    }

    template <typename It>
    bool skipSpace(It &it, const It &end)
    {
        const auto start = it;
        while (it != end && it->isSpace())
            ++it;

        if (skip(it, end, "\\\n"))
            skipSpace(it, end);

        return (it != start);
    }

    template <typename It>
    bool skipString(It &it, const It &end)
    {
        if (skip(it, end, '"')) {
            while (skipUntil(it, end, '"'))
                if (*std::prev(it) != '\\')
                    break;

            // even handle "a" "b"
            const auto retryStart = it;
            skipSpace(it, end);
            if (!skipString(it, end))
                it = retryStart;

            return true;
        }
        return false;
    }

    template <typename It>
    void skipExpression(It &it, const It &end)
    {
        auto parenthesesLevel = 0;
        while (it != end) {
            if (*it == '(') {
                ++parenthesesLevel;
            }
            else if (*it == ')') {
                --parenthesesLevel;
                if (parenthesesLevel < 0)
                    break;
            }
            else if (parenthesesLevel == 0 && *it == ',') {
                break;
            }
            ++it;
        }
    }

    template <typename It>
    void skipNumber(It &it, const It &end)
    {
        while (it != end && *it >= '0' && *it <= '9')
            ++it;
    }


    bool isPartOfIdentifier(QChar c)
    {
        return (c.isLetterOrNumber() || c == '_');
    }

    template <typename It>
    bool skipUntilIdentifier(It &it, const It &begin, const It &end, const char *identifier)
    {
        while (skipUntil(it, end, identifier)) {
            const auto identifierBegin = std::prev(it, std::strlen(identifier));
            const auto startOfToken = (identifierBegin == begin ||
                                       !isPartOfIdentifier(*std::prev(identifierBegin)));
            const auto endOfToken = (it != end && !isPartOfIdentifier(*it));
            if (startOfToken && endOfToken)
                return true;
        }
        return false;
    }

    template <typename It>
    int countLines(const It &begin, const It &it)
    {
        return std::count(begin, it, '\n');
    }

    bool containsIdentifier(const QString &string, const char *identifier)
    {
        auto it = string.begin();
        return skipUntilIdentifier(it, string.begin(), string.end(), identifier);
    }

    QString blankComments(QString string)
    {
        auto it = string.begin();
        const auto end = string.end();
        while (it != end) {
            if (skipString(it, end))
                continue;

            if (!skip(it, end, '/')) {
                ++it;
                continue;
            }
            const auto commentBegin = it - 1;
            if (skip(it, end, '/')) {
                skipLine(it, end);
            }
            else if (skip(it, end, '*')) {
                skipUntil(it, end, "*/");
            }
            else {
                continue;
            }
            std::replace_if(commentBegin, it,
                [](QChar c) { return !c.isSpace(); },
                ' ');
        }
        return string;
    }

    QString unquoteString(QStringView string) 
    {
        auto it = string.begin();
        const auto end = string.end();
        auto inside = false;
        auto result = QString();
        while (it != end) {
            if (skip(it, end, "\\\\")) {
            }
            else if (skip(it, end, '"')) {
                inside = !inside;
            }
            else {
                if (inside)
                    result.append(*it);
                ++it;
            }
        }
        return result;
    }

    struct PrintfCall
    {
        QStringView statement;
        QStringView formatString;
        QVector<QStringView> arguments;
    };

    QList<PrintfCall> findPrintfCalls(const QString &source)
    {
        auto calls = QList<PrintfCall>();
        auto it = source.begin();
        const auto end = source.end();
        while (skipUntilIdentifier(it, source.begin(), end, "printf")) {
            const auto statementBegin = std::prev(it, 6);
            auto call = PrintfCall{ };
            skipSpace(it, end);
            if (skip(it, end, '(')) {
                skipSpace(it, end);
                const auto formatStringBegin = it;
                if (skipString(it, end)) {
                    call.formatString = QStringView(formatStringBegin, it);
                    skipSpace(it, end);
                    while (skip(it, end, ',')) {
                        const auto argumentBegin = it;
                        skipExpression(it, end);
                        call.arguments.append(QStringView(argumentBegin, it));
                    }
                    skipSpace(it, end);
                    if (skip(it, end, ')')) {
                        call.statement = QStringView(statementBegin, it);
                        calls.append(call);
                        continue;
                    }
                }
                // invalid syntax, fail nicely
                call.statement = QStringView(statementBegin, it);
                call.arguments = { QStringView(formatStringBegin, it) };
                calls.append(call);
            }
        }
        return calls;
    }

    struct BufferHeader { 
        uint32_t offset;
        uint32_t prevBegin; 
    };

    int getComponentCount(uint32_t argumentType)
    {
        return argumentType % 100;
    }

    int isFloatType(uint32_t argumentType)
    {
        return (argumentType / 1000) != 1;
    }

    int getColumnCount(uint32_t argumentType)
    {
        if ((argumentType / 1000) == 3)
            return (argumentType - 3000) / 100;
        return 0;
    }
} // namespace

QString GLPrintf::preamble() 
{
    return QStringLiteral(R"(

#define HAS_PRINTF 1

bool printfEnabled = true;
uint _printfArgumentOffset = 0;
layout(std430) buffer _printfBuffer {
  uint _printfOffset;
  uint _printfPrevBegin;
  uint _printfData[];
};

void _printfBegin(int whichFormatString, int argumentCount) {
  uint offset = atomicAdd(_printfOffset, 3 + argumentCount);
  uint prevBegin = atomicExchange(_printfPrevBegin, offset);
  _printfData[prevBegin] = offset;
  _printfData[offset + 1] = whichFormatString;
  _printfData[offset + 2] = argumentCount;
  _printfArgumentOffset = offset + 3;
}

#define W(N) \
void _printfWrite(uint typeBase, uint v[N]) { \
  uint offset = atomicAdd(_printfOffset, 1 + N); \
  _printfData[_printfArgumentOffset++] = offset; \
  _printfData[offset++] = typeBase + N; \
  for (int i = 0; i < N; ++i) \
    _printfData[offset++] = v[i]; \
}
W(1) W(2) W(3) W(4) W(6) W(8) W(9) W(12) W(16)
#undef W

void _printf(uint u) { _printfWrite(1000, uint[](u)); }
void _printf(uvec2 u) { _printfWrite(1000, uint[](u.x, u.y)); }
void _printf(uvec3 u) { _printfWrite(1000, uint[](u.x, u.y, u.z)); }
void _printf(uvec4 u) { _printfWrite(1000, uint[](u.x, u.y, u.z, u.w)); }
void _printf(float v) { uint u = floatBitsToUint(v); _printfWrite(2000, uint[](u)); }
void _printf(vec2 v) { uvec2 u = floatBitsToUint(v); _printfWrite(2000, uint[](u.x, u.y)); }
void _printf(vec3 v) { uvec3 u = floatBitsToUint(v); _printfWrite(2000, uint[](u.x, u.y, u.z)); }
void _printf(vec4 v) { uvec4 u = floatBitsToUint(v); _printfWrite(2000, uint[](u.x, u.y, u.z, u.w)); }

void _printf(mat2x2 m) {
  uvec2 u0 = floatBitsToUint(m[0]);
  uvec2 u1 = floatBitsToUint(m[1]);
  _printfWrite(3200, uint[](u0[0], u0[1], u1[0], u1[1]));
}
void _printf(mat2x3 m) {
  uvec3 u0 = floatBitsToUint(m[0]);
  uvec3 u1 = floatBitsToUint(m[1]);
  _printfWrite(3300, uint[](u0[0], u0[1], u0[2], u1[0], u1[1], u1[2]));
}
void _printf(mat2x4 m) {
  uvec4 u0 = floatBitsToUint(m[0]);
  uvec4 u1 = floatBitsToUint(m[1]);
  _printfWrite(3400, uint[](u0[0], u0[1], u0[2], u0[3], u1[0], u1[1], u1[2], u1[3]));
}
void _printf(mat3x2 m) {
  uvec2 u0 = floatBitsToUint(m[0]);
  uvec2 u1 = floatBitsToUint(m[1]);
  uvec2 u2 = floatBitsToUint(m[2]);
  _printfWrite(3200, uint[](u0[0], u0[1], u1[0], u1[1], u2[0], u2[1]));
}
void _printf(mat3x3 m) {
  uvec3 u0 = floatBitsToUint(m[0]);
  uvec3 u1 = floatBitsToUint(m[1]);
  uvec3 u2 = floatBitsToUint(m[2]);
  _printfWrite(3300, uint[](u0[0], u0[1], u0[2], u1[0], u1[1], u1[2], u2[0], u2[1], u2[2]));
}
void _printf(mat3x4 m) {
  uvec4 u0 = floatBitsToUint(m[0]);
  uvec4 u1 = floatBitsToUint(m[1]);
  uvec4 u2 = floatBitsToUint(m[2]);
  _printfWrite(3400, uint[](u0[0], u0[1], u0[2], u0[3], u1[0], u1[1], u1[2], u1[3], u2[0], u2[1], u2[2], u2[3]));
}
void _printf(mat4x2 m) {
  uvec2 u0 = floatBitsToUint(m[0]);
  uvec2 u1 = floatBitsToUint(m[1]);
  uvec2 u2 = floatBitsToUint(m[2]);
  uvec2 u3 = floatBitsToUint(m[3]);
  _printfWrite(4200, uint[](u0[0], u0[1], u1[0], u1[1], u2[0], u2[1], u3[0], u3[1]));
}
void _printf(mat4x3 m) {
  uvec3 u0 = floatBitsToUint(m[0]);
  uvec3 u1 = floatBitsToUint(m[1]);
  uvec3 u2 = floatBitsToUint(m[2]);
  uvec3 u3 = floatBitsToUint(m[3]);
  _printfWrite(3300, uint[](u0[0], u0[1], u0[2], u1[0], u1[1], u1[2], u2[0], u2[1], u2[2], u3[0], u3[1], u3[2]));
}
void _printf(mat4x4 m) {
  uvec4 u0 = floatBitsToUint(m[0]);
  uvec4 u1 = floatBitsToUint(m[1]);
  uvec4 u2 = floatBitsToUint(m[2]);
  uvec4 u3 = floatBitsToUint(m[3]);
  _printfWrite(3400, uint[](u0[0], u0[1], u0[2], u0[3], u1[0], u1[1], u1[2], u1[3], u2[0], u2[1], u2[2], u2[3], u3[0], u3[1], u3[2], u3[3]));
}

void _printf(int v) { _printf(uint(v)); }
void _printf(ivec2 v) { _printf(uvec2(v)); }
void _printf(ivec3 v) { _printf(uvec3(v)); }
void _printf(ivec4 v) { _printf(uvec4(v)); }
void _printf(bool v) { _printf(uint(v)); }
void _printf(bvec2 v) { _printf(uvec2(v)); }
void _printf(bvec3 v) { _printf(uvec3(v)); }
void _printf(bvec4 v) { _printf(uvec4(v)); }
void _printf(double v) { _printf(float(v)); }
void _printf(dvec2 v) { _printf(vec2(v)); }
void _printf(dvec3 v) { _printf(vec3(v)); }
void _printf(dvec4 v) { _printf(vec4(v)); }
void _printf(dmat2x2 m) { _printf(mat2x2(m)); }
void _printf(dmat2x3 m) { _printf(mat2x3(m)); }
void _printf(dmat2x4 m) { _printf(mat2x4(m)); }
void _printf(dmat3x2 m) { _printf(mat3x2(m)); }
void _printf(dmat3x3 m) { _printf(mat3x3(m)); }
void _printf(dmat3x4 m) { _printf(mat3x4(m)); }
void _printf(dmat4x2 m) { _printf(mat4x2(m)); }
void _printf(dmat4x3 m) { _printf(mat4x3(m)); }
void _printf(dmat4x4 m) { _printf(mat4x4(m)); }

)");
}

QString GLPrintf::patchSource(const QString &fileName, const QString &source)
{
    const auto sourceWithoutComments = blankComments(source);
    const auto calls = findPrintfCalls(sourceWithoutComments);
    if (calls.isEmpty()) {
        if (!mIsUsed)
            mIsUsed = containsIdentifier(source, "HAS_PRINTF") ||
                      containsIdentifier(source, "printfEnabled");
        return source;
    }
    mIsUsed = true;

    auto prevOffset = 0;
    auto patchedSource = QString();
    for (const auto &call : calls) {
        const auto offset = std::distance(sourceWithoutComments.begin(), call.statement.begin());
        patchedSource += QString(source.data() + prevOffset, offset - prevOffset);
        patchedSource += QStringLiteral("(printfEnabled ? _printfBegin(%1, %2)")
            .arg(mFormatStrings.size()).arg(call.arguments.size());
        for (const auto &argument : call.arguments)
            patchedSource += QStringLiteral(", _printf(%1)").arg(argument);
        patchedSource += ", 1 : 0)";
        patchedSource += QString(countLines(call.statement.begin(), call.statement.end()), '\n');
        auto formatString = parseFormatString(call.formatString);
        formatString.fileName = fileName;
        formatString.line = countLines(sourceWithoutComments.begin(), call.statement.begin());
        mFormatStrings.append(formatString);
        prevOffset = offset + call.statement.size();
    }
    patchedSource += QString(source.data() + prevOffset, source.size() - prevOffset);
    return patchedSource;
}

auto GLPrintf::parseFormatString(QStringView string_) -> ParsedFormatString
{
    auto parsed = ParsedFormatString{ };

    const auto string = unquoteString(string_);
    auto it = string.begin();
    const auto end = string.end();
    auto textBegin = it;
    while (skipUntil(it, end, '%')) {
        if (skip(it, end, '%'))
            continue;

        const auto formatBegin = std::prev(it);
        parsed.text.append(QStringView(textBegin, formatBegin).toString());

        auto valid = false;
        while (it != end && std::strchr("-+ #", it->toLatin1()))
            ++it;
        skipNumber(it, end);
        if (skip(it, end, '.'))
            skipNumber(it, end);
        if (it != end) {
            valid = std::strchr("diufFeEgGxXoaA", it->toLatin1());
            ++it;
        }
        parsed.argumentFormats.append(valid ?
            QStringView(formatBegin, it).toString() : "%%");
        textBegin = it;
    }
    parsed.text.append(QStringView(textBegin, end).toString());
    return parsed;
}

QString GLPrintf::formatMessage(const ParsedFormatString &format,
        const QList<Argument>& arguments)
{
    Q_ASSERT(!format.text.empty());
    Q_ASSERT(format.text.size() == format.argumentFormats.size() + 1);

    auto buffer = std::array<char, 64>();
    const auto formatUInt = [&](const QString &format, uint32_t value) {
        snprintf(buffer.data(), buffer.size(), format.toLatin1().constData(), value);
        return buffer.data();
    };
    const auto formatFloat = [&](const QString &format, uint32_t value) {
        union { uint32_t u; float f; } unionCast { value };
        snprintf(buffer.data(), buffer.size(), 
            format.toLatin1().constData(), unionCast.f);
        return buffer.data();
    };

    auto string = format.text.front();
    for (auto i = 0; i < format.argumentFormats.size(); ++i) {
        const auto &argumentFormat = format.argumentFormats[i];
        if (i < arguments.size()) {
            const auto &argument = arguments[i];
            const auto columnCount = getColumnCount(argument.type);
            if (argument.values.size() > 1)
                string += "(";
            auto j = 0;
            for (const auto &value : argument.values) {
                if (j > 0)
                    string += (columnCount && j % columnCount == 0 ? "/" : ", ");
                string += (isFloatType(argument.type) ? 
                    formatFloat(argumentFormat, value) : 
                    formatUInt(argumentFormat, value));
                ++j;
            }
            if (argument.values.size() > 1)
                string += ")";
        }
        string += format.text[i + 1];
    }
    return string;
}

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
            const auto argumentComponents = getComponentCount(argumentType);
            auto argument = Argument{ argumentType, { } };
            for (auto j = 0; j < argumentComponents; ++j)
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
        messages += MessageList::insert(callItemId, MessageType::ShaderWarning,
            "Too many printf calls. Please filter threads by setting printfEnabled.");
#endif // GL_VERSION_4_3

    return messages;
}
