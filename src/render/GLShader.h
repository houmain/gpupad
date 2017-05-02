#ifndef GLSHADER_H
#define GLSHADER_H

#include "PrepareContext.h"
#include "RenderContext.h"

class GLShader
{
public:
    static void parseLog(const QString &log, MessageList &messages);

    ItemId itemId() const { return mItemId; }
    GLShader(QString fileName, Shader::Type type, QString source);
    GLShader(PrepareContext &context, QString header, const Shader &shader);
    bool operator==(const GLShader &rhs) const;

    QString source() const { return mSource; }

    bool compile(RenderContext &context);
    GLuint shaderObject() const { return mShaderObject; }

private:
    ItemId mItemId;
    QString mFileName;
    Shader::Type mType;
    QString mSource;
    GLObject mShaderObject;
};

#endif // GLSHADER_H
