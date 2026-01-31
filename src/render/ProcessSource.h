#pragma once

#include "RenderTask.h"
#include "MessageList.h"
#include "session/Item.h"

class ShaderBase;
class GLProgram;
using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;

class ProcessSource final : public RenderTask
{
    Q_OBJECT
public:
    explicit ProcessSource(RendererPtr renderer, QObject *parent = nullptr);
    ~ProcessSource() override;

    void setFileName(QString fileName);
    void setSourceType(SourceType sourceType);
    void setValidateSource(bool validate);
    void setProcessType(QString processType);
    void clearMessages();

Q_SIGNALS:
    void outputChanged(QVariant output);

private:
    void prepare(bool itemsChanged, EvaluationType) override;
    void render() override;
    void finish() override;
    void prepareShader(Shader::ShaderType shaderType);
    void validate();
    QVariant process();

    std::unique_ptr<ShaderBase> mShader;
    std::unique_ptr<GLProgram> mGLProgram;
    QString mFileName;
    SourceType mSourceType{};
    MessagePtrSet mMessages;
    bool mValidateSource{};
    QString mProcessType{};
    QVariant mOutput;
};
