#include "GLShader.h"
#include "glslang.h"
#include <QRegularExpression>

namespace {
    void removeVersion(QString *source, QString *maxVersion, bool removeNewline)
    {
        const auto regex = QRegularExpression(
            (removeNewline ? "(#version[^\n]*\n?)" : "(#version[^\n]*)"),
            QRegularExpression::MultilineOption);

        for (auto match = regex.match(*source); match.hasMatch(); match = regex.match(*source)) {
            *maxVersion = qMax(*maxVersion, match.captured().trimmed());
            source->remove(match.capturedStart(), match.capturedLength());
        }
    }

    int countLines(const QString &source, int offset = -1) 
    {
        auto end = source.size();
        if (offset >= 0)
            end = qMin(source.size(), offset);
        auto line = 1;
        for (auto i = 0; i < end; ++i)
            if (source[i] == '\n')
                ++line;
        return line;
    }

    QString substituteIncludes(QString source, int fileNo, int sourceFileCount,
        const QStringList &includableSources, const QStringList &includableFileNames,
        ItemId itemId, MessagePtrSet &messages) 
    {
        auto linesInserted = 0;
        const auto regex = QRegularExpression(R"(#include([^\n]*))");
        for (auto match = regex.match(source); match.hasMatch(); match = regex.match(source)) {
            source.remove(match.capturedStart(), match.capturedLength());
            auto fileName = match.captured(1).trimmed();
            if ((fileName.startsWith('<') && fileName.endsWith('>')) ||
                (fileName.startsWith('"') && fileName.endsWith('"'))) {
                fileName = fileName.mid(1, fileName.size() - 2);

                if (const auto index = includableFileNames.indexOf(fileName); index >= 0) {
                    const auto lineNo = countLines(source, match.capturedStart());
                    const auto includableSource = QString("%1\n#line %2 %3\n")
                        .arg(substituteIncludes(
                            includableSources[index], sourceFileCount + index, sourceFileCount, 
                            includableSources, includableFileNames, itemId, messages))
                        .arg(lineNo - linesInserted)
                        .arg(fileNo);
                    source.insert(match.capturedStart(), includableSource); 
                    linesInserted += countLines(includableSource) - 1;
                }
                else {
                    messages += MessageList::insert(itemId,
                        MessageType::IncludableNotFound, fileName);
                }
            }
            else {
                messages += MessageList::insert(itemId,
                    MessageType::InvalidIncludeDirective, fileName);
            }
        }
        return QString("#line 1 %1\n").arg(fileNo) + source;
    }
} // namespace

void GLShader::parseLog(const QString &log,
        MessagePtrSet &messages, ItemId itemId,
        QStringList fileNames)
{
    // Mesa:    0:13(2): error: `gl_Positin' undeclared
    // NVidia:  0(13) : error C1008: undefined variable "gl_Positin"
    // Linker:  error: struct type mismatch between shaders for uniform

    static const auto split = QRegularExpression(
        "("
            "(\\d+)"              // 2. source index
            "(:(\\d+))?"          // 4. [line]
            "\\((\\d+)\\)"        // 5. line or column
            "\\s*:\\s*"
        ")?"
        "([^:]+):\\s*" // 6. severity/code
        "(.+)");  // 7. text

    for (const auto &line : log.split('\n')) {
        const auto match = split.match(line);
        const auto sourceIndex = match.captured(2).toInt();
        const auto lineNumber = (!match.captured(4).isEmpty() ?
            match.captured(4).toInt() : match.captured(5).toInt());
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
            messages += MessageList::insert(
                fileNames[sourceIndex], lineNumber, messageType, text);
        else
            messages += MessageList::insert(
                itemId, messageType, text);
    }
}

GLShader::GLShader(Shader::ShaderType type, 
    const QList<const Shader*> &shaders,
    const QList<const Shader*> &includables)
{
    Q_ASSERT(!shaders.isEmpty());
    mType = type;
    mItemId = shaders.front()->id;

    const auto add = [&](const Shader& shader, bool isIncludable) {
        auto source = QString();
        if (!Singletons::fileCache().getSource(shader.fileName, &source))
            mMessages += MessageList::insert(shader.id,
                MessageType::LoadingFileFailed, shader.fileName);

        mFileNames += (FileDialog::isEmptyOrUntitled(shader.fileName) ? shader.name : shader.fileName);
        (isIncludable ? mIncludableSources : mSources) += source + "\n";
        mLanguage = shader.language;
        mEntryPoint = shader.entryPoint;
    };

    for (const Shader *shader : shaders)
        add(*shader, false);
    for (const Shader *shader : includables)
        add(*shader, true);
}

bool GLShader::operator==(const GLShader &rhs) const
{
    return std::tie(mType, mSources, mIncludableSources, mFileNames, mLanguage, mEntryPoint) ==
           std::tie(rhs.mType, rhs.mSources, rhs.mIncludableSources, rhs.mFileNames, rhs.mLanguage, rhs.mEntryPoint);
}

QString GLShader::getSource() const
{
    auto result = QString();
    for (const auto &source : mSources)
        result += source + "\n";
    return result;
}

bool GLShader::compile(GLPrintf* printf, bool silent)
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
    for (const QString &source : getPatchedSources(printf))
        sources.push_back(qUtf8Printable(source));
    if (sources.empty())
        return false;
    auto sourcePointers = std::vector<const char*>();
    for (const auto &source : sources)
        sourcePointers.push_back(source.data());
    gl.glShaderSource(shader, static_cast<GLsizei>(sourcePointers.size()),
        sourcePointers.data(), nullptr);

    gl.glCompileShader(shader);

    auto status = GLint{ };
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

QStringList GLShader::getPatchedSources(GLPrintf *printf)
{
    Q_ASSERT(!mSources.isEmpty());
    
    auto includableFileNames = QStringList();
    for (auto i = mSources.size(); i < mFileNames.size(); ++i)
        includableFileNames += QFileInfo(mFileNames[i]).fileName();

    auto sources = QStringList();
    for (auto i = 0; i < mSources.size(); ++i)
        sources += substituteIncludes(mSources[i], i, mSources.size(), 
            mIncludableSources, includableFileNames, mItemId, mMessages);

    if (mLanguage != Shader::Language::GLSL)
        for (auto i = 0; i < mSources.size(); ++i) {
            auto source = glslang::generateGLSL(sources[i], mType, mLanguage,
                mEntryPoint, mFileNames[i], mItemId, mMessages);
            if (source.isEmpty())
                return { };
            sources[i] = source;
        }

    auto maxVersion = QString();
    for (auto i = 0; i < sources.size(); ++i)
        removeVersion(&sources[i], &maxVersion, (i > 0));

    if (printf) {
        for (auto i = 0; i < sources.size(); ++i)
            sources[i] = printf->patchSource(mType, mFileNames[i], sources[i]);
    
        if (printf->isUsed(mType)) {
            sources.front().prepend(GLPrintf::preamble());
            maxVersion = std::max(maxVersion, GLPrintf::requiredVersion());
        }
    }

    sources.front().prepend("#define GPUPAD 1\n");
    sources.front().prepend(maxVersion + "\n");

    // workaround: to prevent unesthetic "unexpected end" error,
    // ensure shader is not empty
    sources.back().append("\nstruct XXX_gpupad { float a; };\n");
    return sources;
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

QString GLShader::getAssembly()
{
    if (!compile(nullptr, true))
        return { };

    auto assembly = QString("not supported");
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
