#include "GLShader.h"
#include "glslang.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>

namespace {
    QString getAbsolutePath(const QString &currentFile, const QString &relative)
    {
        return QDir::toNativeSeparators(
            QFileInfo(currentFile).dir().absoluteFilePath(relative));
    }

    void removeVersion(QString *source, QString *maxVersion)
    {
        static const auto regex = QRegularExpression("(#version[^\n]*\n?)",
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

    QString substituteIncludes(QString source, const QString &fileName, 
        QStringList &usedFileNames, ItemId itemId, MessagePtrSet &messages, 
        int recursionDepth = 0)
    {
        if (!usedFileNames.contains(fileName)) {
            usedFileNames.append(fileName);
        }
        else if (recursionDepth++ > 3) {
            messages += MessageList::insert(
                itemId, MessageType::RecursiveInclude, fileName);
            return { };
        }
        const auto fileNo = usedFileNames.indexOf(fileName);

        auto linesInserted = 0;
        static const auto regex = QRegularExpression(R"(#include([^\n]*))");
        for (auto match = regex.match(source); match.hasMatch(); match = regex.match(source)) {
            source.remove(match.capturedStart(), match.capturedLength());
            auto include = match.captured(1).trimmed();
            if ((include.startsWith('<') && include.endsWith('>')) ||
                (include.startsWith('"') && include.endsWith('"'))) {
                include = include.mid(1, include.size() - 2);

                auto includeFileName = getAbsolutePath(fileName, include);
                auto includeSource = QString();
                if (Singletons::fileCache().getSource(includeFileName, &includeSource)) {
                    const auto lineNo = countLines(source, match.capturedStart());
                    const auto includableSource = QString("%1\n#line %2 %3\n")
                        .arg(substituteIncludes(includeSource, includeFileName, 
                            usedFileNames, itemId, messages, recursionDepth))
                        .arg(lineNo - linesInserted)
                        .arg(fileNo);
                    source.insert(match.capturedStart(), includableSource); 
                    linesInserted += countLines(includableSource) - 1;
                }
                else {
                    messages += MessageList::insert(itemId,
                        MessageType::IncludableNotFound, include);
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
        const QStringList &fileNames)
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

    const auto lines = log.split('\n');
    for (const auto &line : lines) {
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

GLShader::GLShader(Shader::ShaderType type, const QList<const Shader*> &shaders)
{
    Q_ASSERT(!shaders.isEmpty());
    mType = type;
    mItemId = shaders.front()->id;

    for (const Shader *shader : shaders) {
        auto source = QString();
        if (!Singletons::fileCache().getSource(shader->fileName, &source))
            mMessages += MessageList::insert(shader->id,
                MessageType::LoadingFileFailed, shader->fileName);

        mFileNames += shader->fileName;
        mSources += source + "\n";
        mLanguage = shader->language;
        mEntryPoint = shader->entryPoint;
    }
}

bool GLShader::operator==(const GLShader &rhs) const
{
    return std::tie(mType, mSources, mFileNames, mLanguage, mEntryPoint, mPatchedSources) ==
           std::tie(rhs.mType, rhs.mSources, rhs.mFileNames, rhs.mLanguage, rhs.mEntryPoint, rhs.mPatchedSources);
}

bool GLShader::compile(GLPrintf *printf, bool failSilently)
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

    auto usedFileNames = QStringList();
    mPatchedSources = getPatchedSources(mMessages, usedFileNames, printf);
    if (mPatchedSources.isEmpty())
        return false;

    auto sources = std::vector<std::string>();
    for (const QString &source : qAsConst(mPatchedSources))
        sources.push_back(qUtf8Printable(source));
    auto sourcePointers = std::vector<const char*>();
    for (const auto &source : sources)
        sourcePointers.push_back(source.data());
    gl.glShaderSource(shader, static_cast<GLsizei>(sourcePointers.size()),
        sourcePointers.data(), nullptr);

    gl.glCompileShader(shader);

    auto status = GLint{ };
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (!failSilently) {
        auto length = GLint{ };
        gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::vector<char>(static_cast<size_t>(length));
        gl.glGetShaderInfoLog(shader, length, nullptr, log.data());
        GLShader::parseLog(log.data(), mMessages, mItemId, usedFileNames);
    }
    if (status != GL_TRUE)
        return false;

    mShaderObject = std::move(shader);
    return true;
}

QStringList GLShader::getPatchedSources(MessagePtrSet &messages, 
    QStringList &usedFileNames, GLPrintf *printf) const
{
    if (mSources.isEmpty())
        return { };

    auto sources = QStringList();
    for (auto i = 0; i < mSources.size(); ++i)
        sources += substituteIncludes(mSources[i], 
            mFileNames[i], usedFileNames, mItemId, messages);

    if (mLanguage != Shader::Language::GLSL) {
        for (auto i = 0; i < mSources.size(); ++i) {
            auto source = glslang::generateGLSL(sources[i], mType, mLanguage,
                mEntryPoint, mFileNames[i], mItemId, messages);
            if (source.isEmpty())
                return { };
            sources[i] = source;
        }
    }

    auto maxVersion = QString();
    for (auto &source : sources)
        removeVersion(&source, &maxVersion);

    if (printf) {
        for (auto i = 0; i < sources.size(); ++i)
            sources[i] = printf->patchSource(mType, mFileNames[i], sources[i]);
    
        if (printf->isUsed(mType)) {
            sources.front().prepend(GLPrintf::preamble());
            maxVersion = std::max(maxVersion, GLPrintf::requiredVersion());
        }
    }

    sources.front().prepend("#define GPUPAD 1\n");

    if (maxVersion.isEmpty())
        maxVersion = "#version 450";
    sources.front().prepend(maxVersion + "\n");

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
