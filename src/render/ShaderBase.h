#pragma once

#include "PrintfBase.h"
#include "Spirv.h"
#include "Reflection.h"

class ShaderBase
{
public:
    ShaderBase(Shader::ShaderType type, const QList<const Shader *> &shaders,
        const Session &session);
    virtual ~ShaderBase() = default;

    bool operator==(const ShaderBase &rhs) const;
    ItemId itemId() const { return mItemId; }
    Shader::ShaderType type() const { return mType; }
    const QStringList &sources() const { return mSources; }
    const QStringList &fileNames() const { return mFileNames; }
    const QString &entryPoint() const { return mEntryPoint; }
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }
    Spirv::Input getSpirvCompilerInput(PrintfBase &printf);
    virtual bool validate();
    virtual Reflection getReflection();
    Spirv compileSpirv();
    QString preprocess();
    QString generateGLSL();
    QString generateHLSL();
    QString disassemble();
    QString generateGLSLangAST();

protected:
    virtual QStringList preprocessorDefinitions() const;
    Spirv compileSpirv(PrintfBase &printf);
    QStringList getPatchedSources(PrintfBase &printf,
        QStringList *usedFileNames);
    QStringList getPatchedSourcesGLSL(PrintfBase &printf,
        QStringList *usedFileNames);
    QStringList getPatchedSourcesHLSL(PrintfBase &printf,
        QStringList *usedFileNames = nullptr);

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    QString mPreamble;
    QString mIncludePaths;
    Shader::ShaderType mType{};
    QString mEntryPoint;
    Session mSession{};
};

QString resolveIncludePath(const QString &currentFile, const QString &relative,
    const QString &includePaths);
bool shaderSessionSettingsDiffer(const Session &a, const Session &b);
