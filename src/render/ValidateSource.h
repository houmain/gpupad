#ifndef VALIDATESOURCE_H
#define VALIDATESOURCE_H

#include "RenderTask.h"
#include "SourceType.h"
#include "session/Item.h"

class GLShader;
class ScriptEngine;

class ValidateSource : public RenderTask
{
    Q_OBJECT
public:
    explicit ValidateSource(QObject *parent = nullptr);
    ~ValidateSource() override;

    void setSource(QString fileName, SourceType sourceType);
    void setAssembleSource(bool active);
    QSet<ItemId> usedItems() const override;

signals:
    void assemblyChanged(QString assembly);

private:
    void prepare(bool itemsChanged, bool manualEvaluation) override;
    void render() override;
    void finish(bool steadyEvaluation) override;
    void release() override;

    QScopedPointer<GLShader> mNewShader;
    QScopedPointer<GLShader> mShader;
    QString mFileName;
    SourceType mSourceType{ };

    QString mScriptSource;
    QScopedPointer<ScriptEngine> mScriptEngine;

    bool mAssembleSource{ };
    QString mAssembly;
};

#endif // VALIDATESOURCE_H
