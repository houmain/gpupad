#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include "SourceType.h"
#include "GLShader.h"
#include "scripting/ScriptEngine.h"

class GLProcessSource
{
public:
    void prepare();
    void render();
    void release();

    void setFileName(QString fileName);
    void setSourceType(SourceType sourceType);
    void setValidateSource(bool validate);
    void setProcessType(QString processType);
    void clearMessages();
    const QString &output() const { return mOutput; }

private:
    QScopedPointer<GLShader> mShader;
    QString mFileName;
    SourceType mSourceType{ };

    QScopedPointer<ScriptEngine> mScriptEngine;
    MessagePtrSet mMessages;

    bool mValidateSource{ };
    QString mProcessType{ };
    QString mOutput;
};
