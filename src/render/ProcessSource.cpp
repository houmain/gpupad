#include "ProcessSource.h"
#include "GLShader.h"
#include "GLPrintf.h"
#include "scripting/ScriptEngine.h"
#include "glslang.h"

namespace {
    Shader::ShaderType getShaderType(SourceType sourceType)
    {
        switch (sourceType) {
            case SourceType::PlainText:
            case SourceType::JavaScript:
                break;

            case SourceType::VertexShader:
                return Shader::ShaderType::Vertex;
            case SourceType::FragmentShader:
                return Shader::ShaderType::Fragment;
            case SourceType::GeometryShader:
                return Shader::ShaderType::Geometry;
            case SourceType::TessellationControl:
                return Shader::ShaderType::TessellationControl;
            case SourceType::TessellationEvaluation:
                return Shader::ShaderType::TessellationEvaluation;
            case SourceType::ComputeShader:
                return Shader::ShaderType::Compute;
        }
        return { };
    }

    const Item *findShaderInSession(const QString &fileName)
    {
        auto shader = std::add_pointer_t<const Shader>();
        Singletons::sessionModel().forEachItem([&](const Item &item) {
            if (auto s = castItem<Shader>(item))
                if (s->fileName == fileName)
                    shader = s;
        });
        return shader;
    }

    QList<const Shader*> getShadersInSession(const QString &fileName)
    {
        auto shaders = QList<const Shader*>();
        if (auto currentShader = castItem<Shader>(findShaderInSession(fileName)))
            if (auto program = castItem<Program>(currentShader->parent))
                for (auto child : program->items)
                    if (auto shader = castItem<Shader>(child))
                        if (shader->shaderType == currentShader->shaderType)
                            shaders.append(shader);
        return shaders;
    }
} // namespace

ProcessSource::ProcessSource(QObject *parent) : RenderTask(parent)
{
}

ProcessSource::~ProcessSource()
{
    releaseResources();
}

QSet<ItemId> ProcessSource::usedItems() const
{
    return { };
}

void ProcessSource::setFileName(QString fileName)
{
    mFileName = fileName;
}

void ProcessSource::setSourceType(SourceType sourceType)
{
    mSourceType = sourceType;
}

void ProcessSource::setValidateSource(bool validate)
{
    mValidateSource = validate;
}

void ProcessSource::setProcessType(QString processType)
{
    mProcessType = processType;
}

void ProcessSource::prepare(bool, EvaluationType)
{
    mMessages.clear();

    if (auto shaderType = getShaderType(mSourceType)) {
        auto shaders = getShadersInSession(mFileName);
        auto shader = Shader{ };
        if (shaders.empty()) {
            shader.fileName = mFileName;
            shader.shaderType = shaderType;
            shaders.append(&shader);
        }
        mNewShader.reset(new GLShader(shaderType, shaders));
    }
}

void ProcessSource::render()
{
    mShader.reset(mNewShader.take());
    mScriptEngine.reset();
    mOutput.clear();

    if (mValidateSource) {
        if (mShader) {
            auto glPrintf = GLPrintf();
            mShader->compile(&glPrintf);
        }
        else if (mSourceType == SourceType::JavaScript) {
            auto scriptSource = QString();
            Singletons::fileCache().getSource(mFileName, &scriptSource);
            scriptSource = "if (false) {" + scriptSource + "}";
            mScriptEngine.reset(new ScriptEngine());
            mScriptEngine->evaluateScript(scriptSource, mFileName, mMessages);
        }
    }

    if (mShader) {
        if (mProcessType == "preprocess")
            mOutput = glslang::preprocess(mShader->getSource());
        else if (mProcessType == "spirv")
            mOutput = glslang::generateSpirV(mShader->getSource(), getShaderType(mSourceType));
        else if (mProcessType == "ast")
            mOutput = glslang::generateAST(mShader->getSource(), getShaderType(mSourceType));
        else if (mProcessType == "assembly")
            mOutput = mShader->getAssembly();
    }
}

void ProcessSource::finish()
{
    Q_EMIT outputChanged(mOutput);
}

void ProcessSource::release()
{
    mShader.reset();
}
