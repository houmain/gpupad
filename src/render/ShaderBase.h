#pragma once

#include "ShaderPrintf.h"

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

    QStringList getPatchedSources(MessagePtrSet &messages,
        ShaderPrintf &printf, QStringList *usedFileNames = nullptr) const;
    QStringList getPatchedSourcesGLSL(MessagePtrSet &messages,
        ShaderPrintf &printf, QStringList *usedFileNames = nullptr) const;
    QStringList getPatchedSourcesHLSL(MessagePtrSet &messages,
        ShaderPrintf &printf, QStringList *usedFileNames = nullptr) const;

protected:
    virtual QStringList preprocessorDefinitions() const;

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    QString mPreamble;
    QString mIncludePaths;
    Shader::ShaderType mType{};
    Shader::Language mLanguage{};
    Session mSession{};
    QString mEntryPoint;
    QStringList mPatchedSources;
};

bool shaderSessionSettingsDiffer(const Session &a, const Session &b);