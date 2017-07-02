#ifndef GLPROGRAM_H
#define GLPROGRAM_H

#include "GLShader.h"

class GLTexture;
class GLBuffer;
class ScriptEngine;

struct GLUniformBinding
{
    ItemId bindingItemId;
    QString name;
    Binding::Type type;
    QVariantList values;
};

struct GLSamplerBinding
{
    ItemId bindingItemId;
    ItemId samplerItemId;
    QString name;
    int arrayIndex;
    GLTexture *texture;
    Sampler::Filter minFilter;
    Sampler::Filter magFilter;
    Sampler::WrapMode wrapModeX;
    Sampler::WrapMode wrapModeY;
    Sampler::WrapMode wrapModeZ;
};

struct GLImageBinding
{
    ItemId bindingItemId;
    QString name;
    int arrayIndex;
    GLTexture *texture;
    int level;
    bool layered;
    int layer;
    GLenum access;
};

struct GLBufferBinding
{
    ItemId bindingItemId;
    QString name;
    int arrayIndex;
    GLBuffer *buffer;
};

class GLProgram
{
public:
    explicit GLProgram(const Program &program);
    bool operator==(const GLProgram &rhs) const;

    bool bind();
    void unbind();
    int getAttributeLocation(const QString &name) const;
    bool apply(const GLUniformBinding &binding, ScriptEngine &scriptEngine);
    bool apply(const GLSamplerBinding &binding, int unit);
    bool apply(const GLImageBinding &binding, int unit);
    bool apply(const GLBufferBinding &binding, int unit);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    bool link();
    int getUniformLocation(const QString &name) const;
    GLenum getUniformDataType(const QString &name) const;

    ItemId mItemId{ };
    QSet<ItemId> mUsedItems;
    MessagePtrList mMessages;
    std::vector<GLShader> mShaders;
    QMap<QString, GLenum> mUniformDataTypes;
    GLObject mProgramObject;
};

#endif // GLPROGRAM_H
