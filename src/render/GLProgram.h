#ifndef GLPROGRAM_H
#define GLPROGRAM_H

#include "GLShader.h"
#include <map>

class GLTexture;
class GLBuffer;
class ScriptEngine;

struct GLUniformBinding
{
    ItemId bindingItemId;
    QString name;
    Binding::Type type;
    QStringList fields;
};

struct GLSamplerBinding
{
    ItemId bindingItemId;
    QString name;
    GLTexture *texture;
    Binding::Filter minFilter;
    Binding::Filter magFilter;
    Binding::WrapMode wrapModeX;
    Binding::WrapMode wrapModeY;
    Binding::WrapMode wrapModeZ;
    QColor borderColor;
    Binding::ComparisonFunc comparisonFunc;
};

struct GLImageBinding
{
    ItemId bindingItemId;
    QString name;
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
    GLBuffer *buffer;
};

class GLProgram
{
public:
    explicit GLProgram(const Program &program);
    bool operator==(const GLProgram &rhs) const;

    bool bind();
    void unbind();
    const QList<QString> &attributes() const { return mAttributes; }
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
    MessagePtrSet mLinkMessages;
    std::vector<GLShader> mShaders;
    QList<QString> mAttributes;
    QMap<QString, GLenum> mUniformDataTypes;
    GLObject mProgramObject;

    MessagePtrSet mUniformsMessages;
    MessagePtrSet mPrevUniformsMessages;
    std::map<QString, bool> mUniformsSet;
    std::map<QString, bool> mUniformBlocksSet;
};

QString getUniformName(QString base, int arrayIndex);


#endif // GLPROGRAM_H
