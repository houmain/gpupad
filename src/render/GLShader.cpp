#include "GLShader.h"

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
            messages += MessageList::insert(
                fileNames[sourceIndex], line, messageType, text);
        else
            messages += MessageList::insert(
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
            mMessages += MessageList::insert(shader->id,
                MessageType::LoadingFileFailed, shader->fileName);

        if (sourceIndex)
            source = QString("#line 1 %1\n").arg(sourceIndex) + source;
        sourceIndex++;

        mSources += source + "\n";
        mFileNames += shader->fileName;
        mItemId = shader->id;
        mType = shader->shaderType;
    }
}

bool GLShader::operator==(const GLShader &rhs) const
{
    return std::tie(mSources, mType) ==
           std::tie(rhs.mSources, rhs.mType);
}

QString GLShader::getSource() const
{
    auto result = QString();
    for (const auto &source : mSources)
        result += source + "\n";
    return result;
}

bool GLShader::compile(bool silent)
{
    if (!GLContext::currentContext()) {
        mMessages += MessageList::insert(
            0, MessageType::OpenGLVersionNotAvailable, "3.3");
        return false;
    }

    if (mShaderObject)
        return true;

    auto freeShader = [](GLuint shaderObject) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteShader(shaderObject);
    };

    auto &gl = GLContext::currentContext();
    auto shader = GLObject(gl.glCreateShader(mType), freeShader);
    if (!shader) {
        mMessages += MessageList::insert(mItemId,
            MessageType::UnsupportedShaderType);
        return false;
    }

    auto sources = std::vector<std::string>();
    foreach (const QString &source, mSources)
        sources.push_back(source.toUtf8().data());

    // workaround: to prevent unesthetic "unexpected end" error,
    // ensure shader is not empty
    if (sources.empty())
        sources.emplace_back();
    sources.back() += "\n struct XXX_gpupad { float a; };\n";

    auto pointers = std::vector<const char*>();
    for (const auto &source : sources)
        pointers.push_back(source.data());
    gl.glShaderSource(shader, static_cast<GLsizei>(pointers.size()),
        pointers.data(), nullptr);

    auto status = GLint{ };
    gl.glCompileShader(shader);
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (!silent) {
        auto length = GLint{ };
        gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::vector<char>(static_cast<size_t>(length));
        gl.glGetShaderInfoLog(shader, length, nullptr, log.data());
        GLShader::parseLog(log.data(), mMessages, mItemId, mFileNames);
    }
    if (status != GL_TRUE)
        return false;

    mShaderObject = std::move(shader);
    return true;
}

QString formatNvGpuProgram(QString assembly)
{
    // indent conditional jumps
    auto indent = 0;
    auto lines = assembly.split('\n');
    for (auto& line : lines) {
        if (line.startsWith("ENDIF") ||
                line.startsWith("ENDREP") ||
                line.startsWith("ELSE"))
            indent -= 2;

        const auto lineIndent = indent;

        if (line.startsWith("IF") ||
                line.startsWith("REP") ||
                line.startsWith("ELSE"))
            indent += 2;

        line.prepend(QString(lineIndent, ' '));
    }
    return lines.join('\n');
}

QString GLShader::getAssembly() {
    compile(true);

    auto assembly = QString();
    auto &gl = GLContext::currentContext();
    if (gl.v4_2 && mShaderObject) {
        auto program = gl.glCreateProgram();
        gl.glAttachShader(program, mShaderObject);
        gl.glLinkProgram(program);

        auto length = GLint{ };
        gl.glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
        if (length > 0) {
            auto format = GLuint{ };
            auto binary = std::string(static_cast<size_t>(length + 1), ' ');
            gl.v4_2->glGetProgramBinary(program, length,
                nullptr, &format, &binary[0]);

            // only supported on Nvidia so far
            const auto begin = binary.find("!!NV");
            const auto end = binary.rfind("END");
            if (begin != std::string::npos && end != std::string::npos)
                assembly = formatNvGpuProgram(QString::fromUtf8(
                    &binary[begin], static_cast<int>(end - begin)));
        }
        gl.glDeleteProgram(program);
    }
    return assembly;
}
