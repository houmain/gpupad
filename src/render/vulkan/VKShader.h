#pragma once

#include "VKItem.h"
#include "render/spirvCross.h"

class VKShader
{
public:
    VKShader(Shader::ShaderType type, const QList<const Shader*> &shaders,
        const QString &preamble, const QString &includePaths);
    bool operator==(const VKShader &rhs) const;
    ItemId itemId() const { return mItemId; }
    Shader::ShaderType type() const { return mType; }
    Shader::Language language() const { return mLanguage; }
    const QStringList &sources() const { return mSources; }
    const QStringList &fileNames() const { return mFileNames; }
    const QString &entryPoint() const { return mEntryPoint; }
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    bool compile(KDGpu::Device &device);
    const spirvCross::Interface &getInterface() const { return mInterface; }
    KDGpu::ShaderStage getShaderStage() const;

private:
    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    QString mPreamble;
    QString mIncludePaths;
    Shader::ShaderType mType{ };
    Shader::Language mLanguage{ };
    QString mEntryPoint;
    KDGpu::ShaderModule mShaderModule;
    QStringList mPatchedSources;
    spirvCross::Interface mInterface;
};
