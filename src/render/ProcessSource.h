#pragma once

#include "RenderTask.h"
#include "SourceType.h"
#include "MessageList.h"
#include <QVariant>

class ShaderBase;
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

private:
    std::unique_ptr<ShaderBase> mShader;
    QString mFileName;
    SourceType mSourceType{};
    MessagePtrSet mMessages;
    bool mValidateSource{};
    QString mProcessType{};
    QVariant mOutput;
};
