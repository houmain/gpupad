#ifndef PROCESSSOURCE_H
#define PROCESSSOURCE_H

#include "RenderTask.h"
#include "MessageList.h"
#include "session/Item.h"
#include "SourceType.h"

class GLShader;
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

Q_SIGNALS:
    void outputChanged(QString output);

private:
    void prepare(bool itemsChanged, EvaluationType) override;
    void render() override;
    void finish() override;
    void release() override;

    QScopedPointer<GLShader> mNewShader;
    QScopedPointer<GLShader> mShader;
    QString mFileName;
    SourceType mSourceType{ };

    QScopedPointer<ScriptEngine> mScriptEngine;
    MessagePtrSet mMessages;

    bool mValidateSource{ };
    QString mProcessType{ };
    QString mOutput;
};

#endif // PROCESSSOURCE_H
