#ifndef GLSHADER_H
#define GLSHADER_H

#include "GLItem.h"

class GLShader
{
public:
    static void parseLog(const QString &log,
        MessagePtrSet &messages, ItemId itemId,
        QList<QString> fileNames);

    explicit GLShader(const QList<const Shader*> &shaders);
    bool operator==(const GLShader &rhs) const;
    Shader::Type type() const { return mType; }

    bool compile();
    GLuint shaderObject() const { return mShaderObject; }

private:
    ItemId mItemId{ };
    MessagePtrSet mMessages;
    QList<QString> mFileNames;
    QList<QString> mSources;
    Shader::Type mType;
    GLObject mShaderObject;
};

#endif // GLSHADER_H
