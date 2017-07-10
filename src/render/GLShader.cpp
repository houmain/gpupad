#include "GLShader.h"
#include "editors/SourceEditor.h"

void GLShader::parseLog(const QString &log,
        MessagePtrSet &messages, ItemId itemId,
        QList<QString> fileNames)
{
    // Mesa:    0:13(2): error: `gl_Positin' undeclared
    // NVidia:  0(13) : error C1008: undefined variable "gl_Positin"

    static const auto split = QRegExp(
        "("
            "(\\d+)"              // 2. source index
            "(:(\\d+))?"          // 4. [line]
            "\\((\\d+)\\)"        // 5. line or column
            "\\s*:(\\s*[^:]+):\\s?" // 6. severity/code
        ")?"
        "([^\\n]+)");  // 7. text

    auto pos = 0;
    while ((pos = split.indexIn(log, pos)) != -1) {
        const auto sourceIndex = split.cap(2).toInt();
        const auto line = (!split.cap(4).isNull() ?
          split.cap(4).toInt() : split.cap(5).toInt());
        const auto severity = split.cap(6);
        const auto text = split.cap(7);

        auto messageType = MessageType::ShaderWarning;
        if (severity.contains("Info", Qt::CaseInsensitive))
            messageType = MessageType::ShaderInfo;
        if (severity.contains("Error", Qt::CaseInsensitive))
            messageType = MessageType::ShaderError;

        if (sourceIndex < fileNames.size())
            messages += Singletons::messageList().insert(
                fileNames[sourceIndex], line, messageType, text);
        else
            messages += Singletons::messageList().insert(
                itemId, messageType, text);

        pos += split.matchedLength();
    }
}

GLShader::GLShader(const QList<const Shader*> &shaders)
{
    auto sourceIndex = 0;
    foreach (const Shader* shader, shaders) {
        auto source = QString();
        if (!Singletons::fileCache().getSource(shader->fileName, &source))
            mMessages += Singletons::messageList().insert(shader->id,
                MessageType::LoadingFileFailed, shader->fileName);

        if (sourceIndex)
            source = QString("#line 0 %1\n").arg(sourceIndex) + source;
        sourceIndex++;

        mSources += source + "\n";
        mFileNames += shader->fileName;
        mItemId = shader->id;
        mType = shader->type;
    }
}

bool GLShader::operator==(const GLShader &rhs) const
{
    return std::tie(mSources, mType) ==
           std::tie(rhs.mSources, rhs.mType);
}

bool GLShader::compile()
{
    if (mShaderObject)
        return true;

    auto freeShader = [](GLuint shaderObject) {
        auto& gl = GLContext::currentContext();
        gl.glDeleteShader(shaderObject);
    };

    auto& gl = GLContext::currentContext();
    auto shader = GLObject(gl.glCreateShader(mType), freeShader);
    if (!shader) {
        mMessages += Singletons::messageList().insert(mItemId,
            MessageType::UnsupportedShaderType);
        return false;
    }

    auto sources = std::vector<std::string>();
    foreach (const QString &source, mSources)
        sources.push_back(source.toUtf8().data());
    auto pointers = std::vector<const char*>();
    for (const auto& source : sources)
        pointers.push_back(source.data());
    gl.glShaderSource(shader, static_cast<GLsizei>(pointers.size()),
        pointers.data(), 0);

    auto status = GLint{ };
    gl.glCompileShader(shader);
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    auto length = GLint{ };
    gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    auto log = std::vector<char>(static_cast<size_t>(length));
    gl.glGetShaderInfoLog(shader, length, NULL, log.data());
    GLShader::parseLog(log.data(), mMessages, mItemId, mFileNames);

    if (status != GL_TRUE)
        return false;

    mShaderObject = std::move(shader);
    return true;
}
