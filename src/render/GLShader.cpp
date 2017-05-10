#include "GLShader.h"
#include "editors/SourceEditor.h"

void GLShader::parseLog(const QString &log, MessageList &messages)
{
    // Mesa:    0:13(2): error: `gl_Positin' undeclared
    // NVidia:  0(13) : error C1008: undefined variable "gl_Positin"

    static const auto split = QRegExp(
        "("
            "(\\d+)"              // 2. file
            "(:(\\d+))?"          // 4. [line]
            "\\((\\d+)\\)"        // 5. line or column
            "\\s*:\\s*[^:]+:\\s*" // severity/code
        ")?"
        "([^\\n]+)");  // 6. text

    auto pos = 0;
    while ((pos = split.indexIn(log, pos)) != -1) {
        const auto file = split.cap(2).toInt();
        const auto line = (!split.cap(4).isNull() ?
          split.cap(4).toInt() : split.cap(5).toInt());
        const auto column = 0;
        const auto text = split.cap(6);

        // TODO: jump to header
        Q_UNUSED(file);

        messages.insert(MessageType::Error, text, line, column);

        pos += split.matchedLength();
    }
}

GLShader::GLShader(QString fileName, Shader::Type type, QString source)
    : mFileName(fileName)
    , mType(type)
    , mSource(source)
{
}

GLShader::GLShader(PrepareContext &context, QString header, const Shader &shader)
    : mItemId(shader.id)
    , mFileName(shader.fileName)
    , mType(shader.type)
{
    if (!header.isEmpty())
        mSource = header + "\n#line 1\n";

    auto source = QString();
    if (!Singletons::fileCache().getSource(mFileName, &source)) {
        context.messages.setContext(mItemId);
        context.messages.insert(MessageType::LoadingFileFailed, mFileName);
    }
    mSource += source;
}

bool GLShader::operator==(const GLShader &rhs) const
{
    return std::tie(mType, mSource) ==
           std::tie(rhs.mType, rhs.mSource);
}

bool GLShader::compile(RenderContext &context)
{
    if (mShaderObject)
        return true;

    if (mSource.trimmed().isEmpty())
        return false;

    auto freeShader = [](GLuint shaderObject) {
        auto& gl = *QOpenGLContext::currentContext()->functions();
        gl.glDeleteShader(shaderObject);
    };

    auto shader = GLObject(context.glCreateShader(mType), freeShader);
    if (!shader) {
        context.messages.setContext(mItemId);
        context.messages.insert(MessageType::UnsupportedShaderType);
        return false;
    }

    auto utf8 = mSource.toUtf8();
    auto cstr = utf8.constData();
    context.glShaderSource(shader, 1, &cstr, 0);
    context.glCompileShader(shader);

    auto status = GLint{ };
    context.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        auto length = GLint{ };
        context.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::vector<char>(static_cast<size_t>(length));
        context.glGetShaderInfoLog(shader, length, NULL, log.data());
        context.messages.setContext(mFileName);
        GLShader::parseLog(log.data(), context.messages);
        return false;
    }

    mShaderObject = std::move(shader);
    return true;
}
