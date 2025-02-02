
#include "ShaderBase.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "Spirv.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {
    QString getAbsolutePath(const QString &currentFile, const QString &relative,
        const QString &includePaths)
    {
        const auto workdir = QFileInfo(currentFile).dir();
        auto absolute = workdir.absoluteFilePath(relative);
        if (!QFile::exists(absolute))
            for (auto path : includePaths.split('\n'))
                if (path = path.trimmed(); !path.isEmpty()) {
                    path = workdir.absoluteFilePath(path) + '/' + relative;
                    if (QFile::exists(path)) {
                        absolute = path;
                        break;
                    }
                }
        return toNativeCanonicalFilePath(absolute);
    }

    bool removeVersion(QString *source, QString *maxVersion)
    {
        static const auto regex = QRegularExpression("(^\\s*#version[^\n]*\n?)",
            QRegularExpression::MultilineOption);

        for (auto match = regex.match(*source); match.hasMatch();
             match = regex.match(*source)) {
            *maxVersion = qMax(*maxVersion, match.captured().trimmed());
            source->remove(match.capturedStart(), match.capturedLength());
            return true;
        }
        return false;
    }

    void removeExtensions(QString *source, QString *extensions)
    {
        static const auto regex =
            QRegularExpression("(^\\s*#extension[^\n\r]*)([\n\r])+",
                QRegularExpression::MultilineOption);

        for (auto match = regex.match(*source); match.hasMatch();
             match = regex.match(*source)) {
            *extensions += match.captured();
            source->remove(match.capturedStart(1), match.capturedLength(1));
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

    void insertAfterExtensions(QString &source, const QString &text)
    {
        // find first line which is not a comment, directive or empty
        static const auto regex = QRegularExpression("^(?!.*#|.*\\/|\\s+).*$",
            QRegularExpression::MultilineOption);
        const auto match = regex.match(source);
        const auto position = (match.hasMatch() ? match.capturedStart() : 0);
        const auto lineNo = countLines(source, position);
        source.insert(position, text + QString("#line %1 0\n").arg(lineNo));
    }

    QString substituteIncludes(QString source, const QString &fileName,
        QStringList *usedFileNames, ItemId itemId, MessagePtrSet &messages,
        const QString &includePaths, QString *maxVersion = nullptr,
        QString *extensions = nullptr, int recursionDepth = 0)
    {
        if (usedFileNames && !usedFileNames->contains(fileName)) {
            usedFileNames->append(fileName);
        } else if (recursionDepth++ > 3) {
            messages += MessageList::insert(itemId,
                MessageType::RecursiveInclude, fileName);
            return {};
        }
        const auto fileNo =
            (usedFileNames ? usedFileNames->indexOf(fileName) : 0);
        const auto versionRemoved =
            (maxVersion ? removeVersion(&source, maxVersion) : false);
        if (extensions)
            removeExtensions(&source, extensions);

        auto linesInserted = (versionRemoved ? -1 : 0);
        static const auto regex = QRegularExpression(R"(#include([^\n]*))");
        for (auto match = regex.match(source); match.hasMatch();
             match = regex.match(source)) {
            source.remove(match.capturedStart(), match.capturedLength());
            auto include = match.captured(1).trimmed();
            if ((include.startsWith('<') && include.endsWith('>'))
                || (include.startsWith('"') && include.endsWith('"'))) {
                include = include.mid(1, include.size() - 2);

                auto includeFileName =
                    getAbsolutePath(fileName, include, includePaths);
                auto includeSource = QString();
                if (Singletons::fileCache().getSource(includeFileName,
                        &includeSource)) {
                    const auto lineNo =
                        countLines(source, match.capturedStart());
                    const auto includableSource =
                        QString("%1\n#line %2 %3\n")
                            .arg(substituteIncludes(includeSource,
                                includeFileName, usedFileNames, itemId,
                                messages, includePaths, maxVersion, extensions,
                                recursionDepth))
                            .arg(lineNo - linesInserted)
                            .arg(fileNo);
                    source.insert(match.capturedStart(), includableSource);
                    linesInserted += countLines(includableSource) - 1;
                } else {
                    messages += MessageList::insert(itemId,
                        MessageType::IncludableNotFound, include);
                }
            } else {
                messages += MessageList::insert(itemId,
                    MessageType::InvalidIncludeDirective, fileName);
            }
        }
        return QString("#line %1 %2\n").arg(versionRemoved ? 2 : 1).arg(fileNo)
            + source;
    }

    void appendLines(QString &dest, const QString &source)
    {
        if (source.isEmpty())
            return;
        if (!dest.isEmpty())
            dest += "\n";
        dest += source;
    }

    QString completeFunctionName(const QString &source, const QString &prefix)
    {
        for (auto begin = source.indexOf(prefix); begin > 0;
             begin = source.indexOf(prefix, begin + 1)) {
            // must be beginning of word
            if (!source[begin - 1].isSpace())
                continue;

            // must not follow struct
            if (source.mid(std::max(static_cast<int>(begin) - 10, 0), 10)
                    .contains("struct"))
                continue;

            // complete word
            auto it = begin;
            while (it < source.size()
                && (source[it].isLetterOrNumber() || source[it] == '_'))
                ++it;

            // ( must follow
            if (it == source.size() || source[it] != '(')
                continue;

            return source.mid(begin, it - begin);
        }
        return {};
    }

    QString findHLSLEntryPoint(Shader::ShaderType shaderType,
        const QString &source)
    {
        const auto prefix = [&]() {
            switch (shaderType) {
            default:
            case Shader::ShaderType::Fragment:       return "PS";
            case Shader::ShaderType::Vertex:         return "VS";
            case Shader::ShaderType::Geometry:       return "GS";
            case Shader::ShaderType::TessControl:    return "HS";
            case Shader::ShaderType::TessEvaluation: return "DS";
            case Shader::ShaderType::Compute:        return "CS";
            }
        }();
        return completeFunctionName(source, prefix);
    }

    QString getEntryPoint(const QString &entryPoint, Shader::Language language,
        Shader::ShaderType shaderType, const QString &source)
    {
        if (!entryPoint.isEmpty())
            return entryPoint;

        if (language == Shader::Language::HLSL)
            if (auto found = findHLSLEntryPoint(shaderType, source);
                !found.isEmpty())
                return found;

        return "main";
    }
} // namespace

bool shaderSessionSettingsDiffer(const Session &a, const Session &b)
{
    const auto shaderCompilerSettings = [](const Session &a) {
        return std::tie(a.autoMapBindings, a.autoMapLocations,
            a.autoSampledTextures, a.vulkanRulesRelaxed, a.spirvVersion);
    };
    const auto hasShaderCompiler =
        (!a.shaderCompiler.isEmpty() || a.renderer == "Vulkan");
    if (hasShaderCompiler
        && shaderCompilerSettings(a) != shaderCompilerSettings(b))
        return true;

    const auto common = [](const Session &a) {
        return std::tie(a.renderer, a.shaderCompiler, a.shaderPreamble,
            a.shaderIncludePaths);
    };
    return common(a) != common(b);
}

ShaderBase::ShaderBase(Shader::ShaderType type,
    const QList<const Shader *> &shaders, const Session &session)
    : mSession(session)
{
    Q_ASSERT(!shaders.isEmpty());
    mType = type;
    mItemId = shaders.front()->id;
    mPreamble = session.shaderPreamble;
    mIncludePaths = session.shaderIncludePaths;

    for (const Shader *shader : shaders) {
        auto source = QString();
        if (!Singletons::fileCache().getSource(shader->fileName, &source))
            mMessages += MessageList::insert(shader->id,
                MessageType::LoadingFileFailed, shader->fileName);

        mFileNames += shader->fileName;
        mSources += source + "\n";
        mLanguage = shader->language;
        if (!shader->entryPoint.isEmpty())
            mEntryPoint = shader->entryPoint;
        appendLines(mPreamble, shader->preamble);
        appendLines(mIncludePaths, shader->includePaths);
    }

    mEntryPoint =
        getEntryPoint(mEntryPoint, mLanguage, mType, mSources.first());
}

bool ShaderBase::operator==(const ShaderBase &rhs) const
{
    // TODO: check included files for modifications
    if (shaderSessionSettingsDiffer(mSession, rhs.mSession))
        return false;

    const auto tie = [](const ShaderBase &a) {
        return std::tie(a.mType, a.mSources, a.mFileNames, a.mLanguage,
            a.mEntryPoint, a.mPreamble, a.mIncludePaths);
    };
    return tie(*this) == tie(rhs);
}

QStringList ShaderBase::preprocessorDefinitions() const
{
    return { "GPUPAD 1" };
}

QStringList ShaderBase::getPatchedSources(ShaderPrintf &printf,
    QStringList *usedFileNames)
{
    return (mLanguage == Shader::Language::HLSL
            ? getPatchedSourcesHLSL(printf, usedFileNames)
            : getPatchedSourcesGLSL(printf, usedFileNames));
}

QStringList ShaderBase::getPatchedSourcesGLSL(ShaderPrintf &printf,
    QStringList *usedFileNames)
{
    if (mSources.isEmpty())
        return {};

    auto maxVersion = QString();
    auto sources = QStringList();
    auto extensions = QString();
    for (auto i = 0; i < mSources.size(); ++i)
        sources += substituteIncludes(mSources[i], mFileNames[i], usedFileNames,
            mItemId, mMessages, mIncludePaths, &maxVersion,
            (i > 0 ? &extensions : nullptr));

    for (auto i = 0; i < sources.size(); ++i)
        sources[i] = printf.patchSource(mType, mFileNames[i], sources[i]);

    if (printf.isUsed(mType)) {
        insertAfterExtensions(sources.front(), ShaderPrintf::preambleGLSL());
        maxVersion = std::max(maxVersion, ShaderPrintf::requiredVersionGLSL());
    }

    if (!mPreamble.isEmpty())
        sources.front().prepend("#line 1 0\n" + mPreamble + "\n");

    for (const auto &definition : preprocessorDefinitions())
        sources.front().prepend("#define " + definition + "\n");

    sources.front().prepend(extensions);

    if (maxVersion.isEmpty()) {
        maxVersion = "#version 450";
        for (const auto &source : sources)
            if (source.contains("gl_FragColor")) {
                maxVersion = "#version 120";
                break;
            }
    }
    sources.front().prepend(maxVersion + "\n");
    return sources;
}

QStringList ShaderBase::getPatchedSourcesHLSL(ShaderPrintf &printf,
    QStringList *usedFileNames)
{
    if (mSources.isEmpty())
        return {};

    auto sources = QStringList();
    for (auto i = 0; i < mSources.size(); ++i)
        sources += substituteIncludes(mSources[i], mFileNames[i], usedFileNames,
            mItemId, mMessages, mIncludePaths);

    for (auto i = 0; i < sources.size(); ++i)
        sources[i] = printf.patchSource(mType, mFileNames[i], sources[i]);

    if (printf.isUsed(mType))
        sources.front().prepend(ShaderPrintf::preambleHLSL());

    if (!mPreamble.isEmpty())
        sources.front().prepend("#line 1 0\n" + mPreamble + "\n");

    for (const auto &definition : preprocessorDefinitions())
        sources.front().prepend("#define " + definition + "\n");

    return sources;
}

Spirv::Input ShaderBase::getSpirvCompilerInput(ShaderPrintf &printf)
{
    auto usedFileNames = QStringList();
    auto patchedSources = getPatchedSources(printf, &usedFileNames);
    return Spirv::Input{
        mLanguage,
        mType,
        patchedSources,
        usedFileNames,
        mEntryPoint,
        mItemId,
    };
}

Spirv ShaderBase::compileSpirv(ShaderPrintf &printf)
{
    auto input = getSpirvCompilerInput(printf);
    auto stages = Spirv::compile(mSession, { input }, mItemId, mMessages);
    return stages[mType];
}

QString ShaderBase::preprocess()
{
    auto usedFileNames = QStringList();
    auto printf = RemoveShaderPrintf();
    auto patchedSources = getPatchedSources(printf, &usedFileNames);
    return Spirv::preprocess(mSession, mLanguage, mType, patchedSources,
        usedFileNames, mEntryPoint, mItemId, mMessages);
}

QString ShaderBase::generateReadableSpirv()
{
    auto printf = RemoveShaderPrintf();
    return Spirv::disassemble(compileSpirv(printf));
}

QString ShaderBase::generateGLSLangAST()
{
    auto usedFileNames = QStringList();
    auto printf = RemoveShaderPrintf();
    auto patchedSources = getPatchedSources(printf, &usedFileNames);
    return Spirv::generateAST(mSession, mLanguage, mType, patchedSources,
        usedFileNames, mEntryPoint, mItemId, mMessages);
}

QString ShaderBase::getJsonInterface()
{
    auto printf = RemoveShaderPrintf();
    const auto spirv = compileSpirv(printf);
    const auto interface = Spirv::Interface(spirv.spirv());
    return getJsonString(*interface);
}
