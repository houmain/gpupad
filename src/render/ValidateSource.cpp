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
        SourceType sourceType)
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
        case SourceType::None:
        case SourceType::PlainText:
            return;

        case SourceType::JavaScript:
            Singletons::fileCache().getSource(mFileName, &mScriptSource);
            mScriptSource = "if (false) {" + mScriptSource + "}";
            return;

        case SourceType::VertexShader:
            shaderType = Shader::ShaderType::Vertex;
            break;
        case SourceType::FragmentShader:
            shaderType = Shader::ShaderType::Fragment;
            break;
        case SourceType::GeometryShader:
            shaderType = Shader::ShaderType::Geometry;
            break;
        case SourceType::TesselationControl:
            shaderType = Shader::ShaderType::TessellationControl;
            break;
        case SourceType::TesselationEvaluation:
            shaderType = Shader::ShaderType::TessellationEvaluation;
            break;
        case SourceType::ComputeShader:
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
