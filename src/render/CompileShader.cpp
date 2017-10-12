#include "CompileShader.h"

CompileShader::CompileShader(QObject *parent) : RenderTask(parent)
{
}

QSet<ItemId> CompileShader::usedItems() const
{
    return { };
}

void CompileShader::prepare(bool itemsChanged, bool manualEvaluation)
{
    Q_UNUSED(itemsChanged);
    Q_UNUSED(manualEvaluation);

    Shader shader;
    shader.fileName = "TODO";
    shader.shaderType = Shader::ShaderType::Fragment;

    mShader.reset(new GLShader({ &shader }));
}

void CompileShader::render()
{
    mShader->compile();
}

void CompileShader::finish(bool steadyEvaluation)
{
    Q_UNUSED(steadyEvaluation);
}

void CompileShader::release()
{
    mShader.reset();
}
