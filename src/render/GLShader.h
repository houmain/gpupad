#ifndef GLSHADER_H
#define GLSHADER_H

#include "GLPrintf.h"

class GLShader
{
public:
    static void parseLog(const QString &log,
        MessagePtrSet &messages, ItemId itemId,
        const QStringList& fileNames);

    GLShader(Shader::ShaderType type, const QList<const Shader*> &shaders);
    bool operator==(const GLShader &rhs) const;
    Shader::ShaderType type() const { return mType; }
    Shader::Language language() const { return mLanguage; }
    const QStringList &sources() const { return mSources; }
    const QStringList &fileNames() const { return mFileNames; }
    const QString &entryPoint() const { return mEntryPoint; }
    MessagePtrSet resetMessages() { return std::exchange(mMessages, {}); }

    QStringList getPatchedSources(MessagePtrSet &messages, 
        QStringList &usedFileNames, GLPrintf *printf = nullptr) const;
    bool compile(GLPrintf *printf = nullptr, bool failSilently = false);
    GLuint shaderObject() const { return mShaderObject; }
    QString getAssembly();

private:
    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    Shader::ShaderType mType{ };
    Shader::Language mLanguage{ };
    QString mEntryPoint;
    GLObject mShaderObject;
    QStringList mPatchedSources;
};

#endif // GLSHADER_H
