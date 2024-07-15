
#include "ShaderBase.h"
#include "Spirv.h"
#include "Singletons.h"
#include "FileCache.h"
#include "FileDialog.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>

namespace {
    QString getAbsolutePath(const QString &currentFile, 
        const QString &relative, const QString &includePaths)
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
        static const auto regex = QRegularExpression("(#version[^\n]*\n?)",
            QRegularExpression::MultilineOption);

        for (auto match = regex.match(*source); match.hasMatch(); match = regex.match(*source)) {
            *maxVersion = qMax(*maxVersion, match.captured().trimmed());
            source->remove(match.capturedStart(), match.capturedLength());
            return true;
        }
        return false;
    }
        
    void removeExtensions(QString *source, QString *extensions)
    {
        static const auto regex = QRegularExpression("(#extension[^\n\r]*)([\n\r])+",
            QRegularExpression::MultilineOption);

        for (auto match = regex.match(*source); match.hasMatch(); match = regex.match(*source)) {
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

    QString substituteIncludes(QString source, const QString &fileName, 
        QStringList &usedFileNames, ItemId itemId, MessagePtrSet &messages, 
        const QString &includePaths, 
        QString *maxVersion = nullptr, 
        QString *extensions = nullptr,
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
        const auto versionRemoved = (maxVersion ? 
            removeVersion(&source, maxVersion) : false);
        if (extensions)
            removeExtensions(&source, extensions);

        auto linesInserted = (versionRemoved ? -1 : 0);
        static const auto regex = QRegularExpression(R"(#include([^\n]*))");
        for (auto match = regex.match(source); match.hasMatch(); match = regex.match(source)) {
            source.remove(match.capturedStart(), match.capturedLength());
            auto include = match.captured(1).trimmed();
            if ((include.startsWith('<') && include.endsWith('>')) ||
                (include.startsWith('"') && include.endsWith('"'))) {
                include = include.mid(1, include.size() - 2);

                auto includeFileName = getAbsolutePath(fileName, include, includePaths);
                auto includeSource = QString();
                if (Singletons::fileCache().getSource(includeFileName, &includeSource)) {
                    const auto lineNo = countLines(source, match.capturedStart());
                    const auto includableSource = QString("%1\n#line %2 %3\n")
                        .arg(substituteIncludes(includeSource, includeFileName, 
                            usedFileNames, itemId, messages, 
                            includePaths, maxVersion, extensions, recursionDepth))
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
        return QString("#line %1 %2\n").arg(versionRemoved ? 2 : 1).arg(fileNo) + source;
    }

    void appendLines(QString &dest, const QString &source) 
    {
        if (source.isEmpty())
            return;
        if (!dest.isEmpty())
            dest += "\n";
        dest += source;
    }
} // namespace


ShaderBase::ShaderBase(Shader::ShaderType type, const QList<const Shader*> &shaders,
    const QString &preamble, const QString &includePaths)
{
    Q_ASSERT(!shaders.isEmpty());
    mType = type;
    mItemId = shaders.front()->id;
    mPreamble = preamble;
    mIncludePaths = includePaths;

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

    if (mEntryPoint.isEmpty())
        mEntryPoint = "main";
}

bool ShaderBase::operator==(const ShaderBase &rhs) const
{
    return std::tie(mType, mSources, mFileNames, mLanguage, 
                    mEntryPoint, mPreamble, mIncludePaths, mPatchedSources) ==
           std::tie(rhs.mType, rhs.mSources, rhs.mFileNames, rhs.mLanguage, 
                    rhs.mEntryPoint, rhs.mPreamble, rhs.mIncludePaths, rhs.mPatchedSources);
}

QStringList ShaderBase::getPatchedSources(MessagePtrSet &messages, 
    QStringList &usedFileNames, ShaderPrintf *printf) const
{
    if (mLanguage == Shader::Language::HLSL)
        return getPatchedSourcesHLSL(messages, usedFileNames, printf);

    return getPatchedSourcesGLSL(messages, usedFileNames, printf);
}

QStringList ShaderBase::getPatchedSourcesGLSL(MessagePtrSet &messages, 
    QStringList &usedFileNames, ShaderPrintf *printf) const
{
    if (mSources.isEmpty())
        return { };

    auto maxVersion = QString();
    auto sources = QStringList();
    auto extensions = QString();
    for (auto i = 0; i < mSources.size(); ++i)
        sources += substituteIncludes(mSources[i], 
            mFileNames[i], usedFileNames, mItemId, messages, 
            mIncludePaths, &maxVersion, &extensions);

    if (mLanguage != Shader::Language::GLSL) {
        messages += MessageList::insert(mItemId, MessageType::OpenGLRendererRequiresGLSL);
        return { };
    }

    if (printf) {
        for (auto i = 0; i < sources.size(); ++i)
            sources[i] = printf->patchSource(mType, 
                mFileNames[i], sources[i]);
    
        if (printf->isUsed(mType)) {
            sources.front().prepend(ShaderPrintf::preambleGLSL());
            maxVersion = std::max(maxVersion, ShaderPrintf::requiredVersionGLSL());
        }
    }

    if (!mPreamble.isEmpty())
        sources.front().prepend("#line 1 0\n" + mPreamble + "\n");

    sources.front().prepend("#define GPUPAD 1\n");

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

QStringList ShaderBase::getPatchedSourcesHLSL(MessagePtrSet &messages, 
    QStringList &usedFileNames, ShaderPrintf *printf) const
{
    if (mSources.isEmpty())
        return { };

    auto sources = QStringList();
    for (auto i = 0; i < mSources.size(); ++i)
        sources += substituteIncludes(mSources[i], 
            mFileNames[i], usedFileNames, mItemId, messages, mIncludePaths);

    if (printf) {
        for (auto i = 0; i < sources.size(); ++i)
            sources[i] = printf->patchSource(mType, 
                mFileNames[i], sources[i]);
    
        if (printf->isUsed(mType))
            sources.front().prepend(ShaderPrintf::preambleHLSL());
    }

    if (!mPreamble.isEmpty())
        sources.front().prepend("#line 1 0\n" + mPreamble + "\n");

    sources.front().prepend("#define GPUPAD 1\n");
    return sources;
}
