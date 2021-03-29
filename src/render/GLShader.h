#ifndef GLSHADER_H
#define GLSHADER_H

#include "GLItem.h"
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
    Shader::ShaderType mType;
    GLObject mShaderObject;
};

#endif // GLSHADER_H
