#pragma once

#include "ShaderPrintf.h"

class ShaderBase
{
public:
    ShaderBase(Shader::ShaderType type, const QList<const Shader*> &shaders,
        const QString &preamble, const QString &includePaths);
    bool operator==(const ShaderBase &rhs) const;
    ItemId itemId() const { return mItemId; }
    Shader::ShaderType type() const { return mType; }
    Shader::Language language() const { return mLanguage; }
    const QStringList &sources() const { return mSources; }
    const QStringList &fileNames() const { return mFileNames; }
    const QString &entryPoint() const { return mEntryPoint; }
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    QStringList getPatchedSources(MessagePtrSet &messages, 
        QStringList &usedFileNames, ShaderPrintf *printf = nullptr) const;

protected:
    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    QString mPreamble;
    QString mIncludePaths;
    Shader::ShaderType mType{ };
    Shader::Language mLanguage{ };
    QString mEntryPoint;
    QStringList mPatchedSources;
};
