#include "ValidateSource.h"
#include "GLShader.h"
#include "scripting/ScriptEngine.h"

ValidateSource::ValidateSource(QObject *parent) : RenderTask(parent)
{
}

ValidateSource::~ValidateSource()
{
    releaseResources();
}

QSet<ItemId> ValidateSource::usedItems() const
{
    return { };
}

void ValidateSource::setSource(QString fileName,
        SourceEditor::SourceType sourceType)
{
    mFileName = fileName;
    mSourceType = sourceType;
}

void ValidateSource::prepare(bool itemsChanged, bool manualEvaluation)
{
    Q_UNUSED(itemsChanged);
    Q_UNUSED(manualEvaluation);

    auto shaderType = Shader::ShaderType{ };
    switch (mSourceType) {
        case SourceEditor::None:
        case SourceEditor::PlainText:
            return;

        case SourceEditor::JavaScript:
            Singletons::fileCache().getSource(mFileName, &mScriptSource);
            mScriptSource = "if (false) {" + mScriptSource + "}";
            return;

        case SourceEditor::VertexShader:
            shaderType = Shader::ShaderType::Vertex;
            break;
        case SourceEditor::FragmentShader:
            shaderType = Shader::ShaderType::Fragment;
            break;
        case SourceEditor::GeometryShader:
            shaderType = Shader::ShaderType::Geometry;
            break;
        case SourceEditor::TesselationControl:
            shaderType = Shader::ShaderType::TessellationControl;
            break;
        case SourceEditor::TesselationEvaluation:
            shaderType = Shader::ShaderType::TessellationEvaluation;
            break;
        case SourceEditor::ComputeShader:
            shaderType = Shader::ShaderType::Compute;
            break;
    }

    Shader shader;
    shader.fileName = mFileName;
    shader.shaderType = shaderType;
    mNewShader.reset(new GLShader({ &shader }));
}

void ValidateSource::render()
{
    if (mNewShader) {
        mScriptEngine.reset();
        mShader.reset(mNewShader.take());
        mShader->compile();
    }
    else {
        mShader.reset();
        mScriptEngine.reset(new ScriptEngine({
            ScriptEngine::Script{
                mFileName, std::exchange(mScriptSource, "")
            }
        }));
    }
}

void ValidateSource::finish(bool steadyEvaluation)
{
    Q_UNUSED(steadyEvaluation);
}

void ValidateSource::release()
{
    mShader.reset();
}
