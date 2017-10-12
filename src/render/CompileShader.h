#ifndef COMPILESHADER_H
#define COMPILESHADER_H

#include "RenderTask.h"
#include "session/Item.h"
#include "GLShader.h"

class CompileShader : public RenderTask
{
    Q_OBJECT
public:
    explicit CompileShader(QObject *parent = nullptr);

    QSet<ItemId> usedItems() const override;

private:
    void prepare(bool itemsChanged, bool manualEvaluation) override;
    void render() override;
    void finish(bool steadyEvaluation) override;
    void release() override;

    QScopedPointer<GLShader> mShader;
};

#endif // COMPILESHADER_H
