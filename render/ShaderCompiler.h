#ifndef SHADERCOMPILER_H
#define SHADERCOMPILER_H

#include "session/Item.h"
#include "MessageList.h"
#include "RenderTask.h"

class ShaderCompiler : public RenderTask
{
public:
    explicit ShaderCompiler(QString fileName, QObject *parent = nullptr);
    ~ShaderCompiler();

    const QString &fileName() const { return mFileName; }

private:
    void prepare() override;
    void render(QOpenGLContext &glContext) override;
    void finish() override { }
    void release(QOpenGLContext &) override { }

    QString mFileName;
    Shader::Type mShaderType;
    QString mSource;
    MessageList mMessages;
};

#endif // SHADERCOMPILER_H
