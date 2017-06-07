#ifndef GLPROGRAM_H
#define GLPROGRAM_H

#include "GLShader.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "ScriptEngine.h"

class GLProgram
{
public:
    void initialize(PrepareContext &context, const Program &program);
    void addTextureBinding(PrepareContext &context, const Texture &texture,
        QString name = "", int arrayIndex = 0);
    void addSamplerBinding(PrepareContext &context, const Sampler &sampler,
        QString name = "", int arrayIndex = 0);
    void addBufferBinding(PrepareContext &context, const Buffer &buffer,
        QString name = "", int arrayIndex = 0);
    void addBinding(PrepareContext &context, const Binding &uniform);
    void addScript(PrepareContext &context, const Script &script);

    void cache(RenderContext &context, GLProgram &&update);
    bool bind(RenderContext &context);
    void unbind(RenderContext &context);
    int getAttributeLocation(RenderContext &context, const QString &name) const;
    QList<std::pair<QString, QImage>> getModifiedImages(RenderContext &context);
    QList<std::pair<QString, QByteArray>> getModifiedBuffers(
        RenderContext &context);

private:
    struct GLUniform {
        ItemId itemId;
        QString name;
        Binding::Type type;
        QVariantList values;
    };

    struct GLSamplerBinding {
        ItemId itemId;
        QString name;
        int arrayIndex;
        int textureIndex;
        Sampler::Filter minFilter;
        Sampler::Filter magFilter;
        Sampler::WrapMode wrapModeX;
        Sampler::WrapMode wrapModeY;
        Sampler::WrapMode wrapModeZ;
    };

    struct GLImageBinding {
        QString name;
        int arrayIndex;
        int textureIndex;
        int level;
        bool layered;
        int layer;
        GLenum access;
    };

    struct GLBufferBinding {
        QString name;
        int arrayIndex;
        int bufferIndex;
    };

    bool link(RenderContext &context);
    void applySamplerBinding(RenderContext &context,
        const GLSamplerBinding &binding, int unit);
    void applyImageBinding(RenderContext &context,
        const GLImageBinding &binding, int unit);
    void applyBufferBinding(RenderContext &context,
        const GLBufferBinding &binding, int unit);
    void applyUniform(RenderContext &context, const GLUniform &uniform);
    int getUniformLocation(RenderContext &context, const QString &name) const;
    GLenum getUniformDataType(const QString &name) const;

    ItemId mItemId{ };
    ScriptEngine mScriptEngine;
    std::vector<GLShader> mShaders;
    std::vector<GLTexture> mTextures;
    std::vector<GLBuffer> mBuffers;
    std::vector<GLUniform> mUniforms;
    std::vector<GLSamplerBinding> mSamplerBindings;
    std::vector<GLImageBinding> mImageBindings;
    std::vector<GLBufferBinding> mBufferBindings;
    QMap<QString, GLenum> mUniformDataTypes;
    GLObject mProgramObject;
};

#endif // GLPROGRAM_H
