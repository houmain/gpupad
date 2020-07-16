#ifndef PROCESSSOURCE_H
#define PROCESSSOURCE_H

#include "RenderTask.h"
#include "SourceType.h"
#include "session/Item.h"

class GLShader;
class ScriptEngine;

class ProcessSource final : public RenderTask
{
    Q_OBJECT
public:
    explicit ProcessSource(QObject *parent = nullptr);
    ~ProcessSource() override;

    void setSource(QString fileName, SourceType sourceType);
    void setValidateSource(bool validate);
    void setProcessType(QString processType);
    QSet<ItemId> usedItems() const override;

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

    bool mValidateSource{ };
    QString mProcessType{ };
    QString mOutput;
};

#endif // PROCESSSOURCE_H
