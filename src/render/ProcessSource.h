#pragma once

#include "RenderTask.h"
#include "SourceType.h"
#include "MessageList.h"

class ShaderBase;
class ScriptEngine;

class ProcessSource final : public RenderTask
{
    Q_OBJECT
public:
    explicit ProcessSource(QObject *parent = nullptr);
    ~ProcessSource() override;

    void setFileName(QString fileName);
    void setSourceType(SourceType sourceType);
    void setValidateSource(bool validate);
    void setProcessType(QString processType);
    void clearMessages();

Q_SIGNALS:
    void outputChanged(QString output);

private:
    bool initialize() override;
    void prepare(bool itemsChanged, EvaluationType);
    void render() override;
    void finish() override;
    void release() override;

private:
    QScopedPointer<ShaderBase> mShader;
    QString mFileName;
    SourceType mSourceType{};

    QScopedPointer<ScriptEngine> mScriptEngine;
    MessagePtrSet mMessages;

    bool mValidateSource{};
    QString mProcessType{};
    QString mOutput;
};
