#ifndef GLSHADER_H
#define GLSHADER_H

#include "GLPrintf.h"

class GLShader
{
public:
    static void parseLog(const QString &log,
        MessagePtrSet &messages, ItemId itemId,
        QStringList fileNames);

    GLShader(Shader::ShaderType type,
        const QList<const Shader*> &shaders,
        const QList<const Shader*> &includables = { });
    bool operator==(const GLShader &rhs) const;
    Shader::ShaderType type() const { return mType; }
    Shader::Language language() const { return mLanguage; }
    const QStringList &sources() const { return mSources; }
    const QStringList &fileNames() const { return mFileNames; }
    const QString &entryPoint() const { return mEntryPoint; }

    QString getSource() const;
    bool compile(GLPrintf *printf = nullptr, bool silent = false);
    GLuint shaderObject() const { return mShaderObject; }
    QString getAssembly();

private:
    QStringList getPatchedSources(GLPrintf *printf);

    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QStringList mFileNames;
    QStringList mSources;
    QStringList mIncludableSources;
    Shader::ShaderType mType{ };
    Shader::Language mLanguage{ };
    QString mEntryPoint;
    GLObject mShaderObject;
};

#endif // GLSHADER_H
