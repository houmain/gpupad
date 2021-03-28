#include "ProcessSource.h"
#include "GLShader.h"
#include "scripting/ScriptEngine.h"
#include <QProcess>
#include <QDir>

namespace {
    const char *getGLSLangSourceType(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
            case Shader::ShaderType::Vertex: return "vert";
            case Shader::ShaderType::Fragment: return "frag";
            case Shader::ShaderType::TessellationControl: return "tesc";
            case Shader::ShaderType::TessellationEvaluation: return "tese";
            case Shader::ShaderType::Geometry: return "geom";
            case Shader::ShaderType::Compute: return "comp";
                break;
        }
        return nullptr;
    }

    QString executeGLSLangValidator(QString source, QStringList args, Shader::ShaderType shaderType)
    {
        args += "--stdin";
        if (auto sourceType = getGLSLangSourceType(shaderType)) {
            args += "-S";
            args += sourceType;
        }
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setWorkingDirectory(QDir::temp().path());
        process.start("glslangValidator", args);
        if (!process.waitForStarted())
            return "glslangValidator not found";
        process.write(source.toUtf8());
        process.closeWriteChannel();

        auto data = QByteArray();
        while (process.waitForReadyRead())
            data.append(process.readAll());

        auto result = QString::fromUtf8(data);
        if (result.startsWith("stdin"))
            return result.mid(5);
        return result;
    }

    QString preprocess(QString source)
    {
        const auto args = QStringList{ "-E" };
        return executeGLSLangValidator(source, args, Shader::ShaderType::Vertex);
    }

    QString generateSpirV(QString source, Shader::ShaderType shaderType)
    {
        const auto args = QStringList{
          "-H", "--aml", "--amb",
          "--client", "opengl100",
        };
        return executeGLSLangValidator(source, args, shaderType);
    }

    QString generateAST(QString source, Shader::ShaderType shaderType)
    {
        const auto args = QStringList{
          "-i", "--aml", "--amb",
          "--client", "opengl100",
        };
        return executeGLSLangValidator(source, args, shaderType);
    }

    Shader::ShaderType getShaderType(SourceType sourceType)
    {
        switch (sourceType) {
            case SourceType::None:
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

void ProcessSource::setSource(QString fileName,
        SourceType sourceType)
{
    mFileName = fileName;
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
    if (auto shaderType = getShaderType(mSourceType))
        mNewShader.reset(new GLShader(shaderType, getShadersInSession(mFileName)));
}

void ProcessSource::render()
{
    mShader.reset(mNewShader.take());
    mScriptEngine.reset();
    mOutput.clear();

    if (mValidateSource) {
        if (mShader) {
            mShader->compile();
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
            mOutput = preprocess(mShader->getSource());
        else if (mProcessType == "spirv")
            mOutput = generateSpirV(mShader->getSource(), getShaderType(mSourceType));
        else if (mProcessType == "ast")
            mOutput = generateAST(mShader->getSource(), getShaderType(mSourceType));
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
