#ifndef GLPROGRAM_H
#define GLPROGRAM_H

#include "GLShader.h"
#include "scripting/ScriptEngine.h"
#include <map>

class GLTexture;
class GLBuffer;

struct GLUniformBinding
{
    ItemId bindingItemId;
    QString name;
    Binding::BindingType type;
    QStringList values;
    bool transpose;
};

struct GLSamplerBinding
{
    ItemId bindingItemId;
    QString name;
    GLTexture *texture;
    Binding::Filter minFilter;
    Binding::Filter magFilter;
    bool anisotropic;
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
    int layer;
    GLenum access;
    Binding::ImageFormat format;
};

struct GLBufferBinding
{
    ItemId bindingItemId;
    QString name;
    GLBuffer *buffer;
    QString offset;
    QString rowCount;
    int stride;
    bool readonly;
};

struct GLSubroutineBinding
{
    ItemId bindingItemId;
    QString name;
    QString subroutine;
    Shader::ShaderType type;
};

class GLProgram
{
public:
    GLProgram(const Program &program, const QString &shaderPreamble, const QString &shaderIncludePaths);
    bool operator==(const GLProgram &rhs) const;

    bool link();
    bool bind(MessagePtrSet *callMessages);
    void unbind(ItemId callItemId);
    int getAttributeLocation(const QString &name) const;
    bool apply(const GLUniformBinding &binding, 
        ScriptEngine &scriptEngine);
    bool apply(const GLSamplerBinding &binding, int unit);
    bool apply(const GLImageBinding &binding, int unit);
    bool apply(const GLBufferBinding &binding, 
        ScriptEngine &scriptEngine);
    bool applyPrintfBindings();
    bool apply(const GLSubroutineBinding &subroutine);
    void reapplySubroutines();
    bool allBuffersBound() const;
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct SubroutineUniform
    {
        QString name;
        QStringList subroutines;
        ItemId bindingItemId;
        QString boundSubroutine;
    };

    bool linkShaders(GLuint program);
    bool linkProgram(GLuint program);
    QString getUniformBaseName(const QString &name) const;
    void uniformSet(const QString &name);
    void bufferSet(const QString &name);

    ItemId mItemId{ };
    QSet<ItemId> mUsedItems;
    MessagePtrSet mLinkMessages;
    std::vector<GLShader> mShaders;
    QMap<Shader::ShaderType, QList<SubroutineUniform>> mSubroutineUniforms;
    QMap<QString, GLint> mAttributeLocations;
    QMap<QString, std::tuple<int, GLenum, GLint>> mActiveUniforms;
    QMap<QString, std::pair<GLenum, GLint>> mBufferBindingPoints;
    QMap<QString, GLObject> mTextureBufferObjects;
    GLObject mProgramObject;
    bool mFailed{ };
    MessagePtrSet *mCallMessages{ };
    std::map<QString, bool> mUniformsSet;
    std::map<QString, bool> mBuffersSet;
    mutable std::map<QString, bool> mAttributesSet;
    GLPrintf mPrintf;
    MessagePtrSet mPrintfMessages;
};

#endif // GLPROGRAM_H
