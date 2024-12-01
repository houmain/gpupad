#include "GLShader.h"

void GLShader::parseLog(const QString &log, MessagePtrSet &messages,
    ItemId itemId, const QStringList &fileNames)
{
    // Mesa:    0:13(2): error: `gl_Positin' undeclared
    // NVidia:  0(13) : error C1008: undefined variable "gl_Positin"
    // Linker:  error: struct type mismatch between shaders for uniform
    // Linker:  Error Duplicate location 0 for uniform uModel

    static const auto split = QRegularExpression(
        "("
        "(\\d+)" // 2. source index
        "(:(\\d+))?" // 4. [line]
        "\\((\\d+)\\)" // 5. line or column
        "\\s*:\\s*"
        ")?"
        "([^:]+:|Error)\\s*" // 6. severity/code
        "(.+)"); // 7. text

    const auto lines = log.split('\n');
    for (const auto &line : lines) {
        const auto match = split.match(line);
        const auto sourceIndex = match.captured(2).toInt();
        const auto lineNumber = (!match.captured(4).isEmpty()
                ? match.captured(4).toInt()
                : match.captured(5).toInt());
        const auto severity = match.captured(6);
        const auto text = match.captured(7).trimmed();
        if (text.isEmpty())
            continue;

        auto messageType = MessageType::ShaderWarning;
        if (severity.contains("Info", Qt::CaseInsensitive))
            messageType = MessageType::ShaderInfo;
        if (severity.contains("Error", Qt::CaseInsensitive))
            messageType = MessageType::ShaderError;

        if (sourceIndex < fileNames.size())
            messages += MessageList::insert(fileNames[sourceIndex], lineNumber,
                messageType, text);
        else
            messages += MessageList::insert(itemId, messageType, text);
    }
}

GLShader::GLShader(Shader::ShaderType type,
    const QList<const Shader *> &shaders, const Session &session)
    : ShaderBase(type, shaders, session)
{
}

bool GLShader::compile(ShaderPrintf &printf)
{
    if (!GLContext::currentContext()) {
        mMessages += MessageList::insert(mItemId,
            MessageType::OpenGLVersionNotAvailable, "3.3");
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
        mMessages +=
            MessageList::insert(mItemId, MessageType::UnsupportedShaderType);
        return false;
    }

    auto usedFileNames = QStringList();
    if (mSession.shaderCompiler.isEmpty()) {
        if (mLanguage != Shader::Language::GLSL) {
            mMessages += MessageList::insert(mItemId,
                MessageType::OpenGLRendererRequiresGLSL);
            return {};
        }
        auto patchedSources = getPatchedSourcesGLSL(printf, &usedFileNames);
        if (patchedSources.isEmpty())
            return false;

        auto sources = std::vector<std::string>();
        for (const QString &source : std::as_const(patchedSources))
            sources.push_back(qUtf8Printable(source));

        auto sourcePointers = std::vector<const char *>();
        for (const auto &source : sources)
            sourcePointers.push_back(source.data());
        gl.glShaderSource(shader, static_cast<GLsizei>(sourcePointers.size()),
            sourcePointers.data(), nullptr);

        gl.glCompileShader(shader);

    } else {
        // TODO:
        const auto shiftBindingsInSet0 = 0;
        const auto spirv = generateSpirv(printf, shiftBindingsInSet0);
        if (!spirv)
            return false;

        void (*glSpecializeShader)(GLuint, const GLchar *, GLuint,
            const GLuint *, const GLuint *);
        glSpecializeShader = reinterpret_cast<decltype(glSpecializeShader)>(
            gl.getProcAddress("glSpecializeShader"));
        if (!glSpecializeShader) {
            mMessages += MessageList::insert(mItemId,
                MessageType::OpenGLVersionNotAvailable, "4.6");
            return false;
        }
        const auto shaderObject = static_cast<GLuint>(shader);
        gl.v4_2->glShaderBinary(1, &shaderObject,
            GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, spirv.spirv().data(),
            static_cast<GLsizei>(spirv.spirv().size() * sizeof(uint32_t)));

        glSpecializeShader(shader, qPrintable(mEntryPoint), 0, 0, 0);

        // clear error state
        glGetError();
    }

    auto length = GLint{};
    gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        auto log = std::vector<char>(static_cast<size_t>(length));
        gl.glGetShaderInfoLog(shader, length, nullptr, log.data());
        GLShader::parseLog(log.data(), mMessages, mItemId, usedFileNames);
    }
    auto status = GLint{};
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
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
    for (auto &line : lines) {
        if (line.startsWith("ENDIF") || line.startsWith("ENDREP")
            || line.startsWith("ELSE"))
            indent -= 2;

        const auto lineIndent = indent;

        if (line.startsWith("IF") || line.startsWith("REP")
            || line.startsWith("ELSE"))
            indent += 2;

        line.prepend(QString(lineIndent, ' '));
    }
    return lines.join('\n');
}

QStringList GLShader::preprocessorDefinitions() const
{
    auto definitions = ShaderBase::preprocessorDefinitions();
    definitions.append("GPUPAD_OPENGL 1");
    return definitions;
}

QString tryGetProgramBinary(const GLShader &shader)
{
    auto assembly = QString();
    auto &gl = GLContext::currentContext();
    if (gl.v4_2 && shader.shaderObject()) {
        auto program = gl.glCreateProgram();
        gl.glAttachShader(program, shader.shaderObject());
        gl.glLinkProgram(program);

        auto length = GLint{};
        gl.glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
        if (length > 0) {
            auto format = GLuint{};
            auto binary = std::string(static_cast<size_t>(length + 1), ' ');
            gl.v4_2->glGetProgramBinary(program, length, nullptr, &format,
                &binary[0]);

            // only supported on Nvidia so far
            const auto begin = binary.find("!!NV");
            const auto end = binary.rfind("END");
            if (begin != std::string::npos && end != std::string::npos)
                assembly = formatNvGpuProgram(QString::fromUtf8(&binary[begin],
                    static_cast<int>(end - begin)));
        }
        gl.glDeleteProgram(program);
    }
    return assembly;
}

void tryGetLinkerWarnings(const GLShader &shader, MessagePtrSet &messages)
{
    auto &gl = GLContext::currentContext();
    auto program = gl.glCreateProgram();
    gl.glAttachShader(program, shader.shaderObject());
    gl.glLinkProgram(program);
    auto status = GLint{};
    gl.glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) {
        auto length = GLint{};
        gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            auto log = std::vector<char>(static_cast<size_t>(length));
            gl.glGetProgramInfoLog(program, length, nullptr, log.data());
            GLShader::parseLog(log.data(), messages, shader.itemId(),
                shader.fileNames());
        }
    }
    gl.glDeleteProgram(program);
}