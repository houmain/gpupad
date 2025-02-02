#pragma once

#include "ShaderPrintf.h"
#include "Spirv.h"

class ShaderBase
{
public:
    ShaderBase(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);
    bool operator==(const ShaderBase &rhs) const;
    ItemId itemId() const { return mItemId; }
    Shader::ShaderType type() const { return mType; }
    Shader::Language language() const { return mLanguage; }
    const QStringList &sources() const { return mSources; }
    const QStringList &fileNames() const { return mFileNames; }
    const QString &entryPoint() const { return mEntryPoint; }
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }
    Spirv::Input getSpirvCompilerInput(ShaderPrintf &printf);
    Spirv compileSpirv(ShaderPrintf &printf);
    QString preprocess();
    QString generateReadableSpirv();
    QString generateGLSLangAST();
    QString getJsonInterface();

protected:
    virtual QStringList preprocessorDefinitions() const;
    QStringList getPatchedSources(ShaderPrintf &printf,
        QStringList *usedFileNames);
    QStringList getPatchedSourcesGLSL(ShaderPrintf &printf,
        QStringList *usedFileNames);
    QStringList getPatchedSourcesHLSL(ShaderPrintf &printf,
        QStringList *usedFileNames);

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    QString mPreamble;
    QString mIncludePaths;
    Shader::ShaderType mType{};
    Shader::Language mLanguage{};
    QString mEntryPoint;
    Session mSession{};
};

bool shaderSessionSettingsDiffer(const Session &a, const Session &b);