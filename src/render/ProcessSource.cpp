#include "ProcessSource.h"
#include "GLShader.h"
#include "scripting/ScriptEngine.h"
#include <QProcess>

namespace {
    QString glslangValidator(QString source, QStringList args)
    {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start("glslangValidator", args);
        if (!process.waitForStarted())
            return "";
        process.write(source.toUtf8());
        process.closeWriteChannel();

        auto data = QByteArray();
        while (process.waitForReadyRead())
            data.append(process.readAll());

        return QString::fromUtf8(data);
    }

    QString preprocess(QString source)
    {
        const auto args = QStringList{ "-E", "--stdin", "-S", "vert" };
        return glslangValidator(source, args);
    }

    QString generateSpirV(QString source, SourceType sourceType)
    {
        const auto type = [&]() -> const char* {
            switch (sourceType) {
                case SourceType::VertexShader: return "vert";
                case SourceType::FragmentShader: return "frag";
                case SourceType::TesselationControl: return "tesc";
                case SourceType::TesselationEvaluation: return "tese";
                case SourceType::GeometryShader: return "geom";
                case SourceType::ComputeShader: return "comp";
                default: return nullptr;
            }
        }();
        if (!type)
            return "";

        const auto args = QStringList{
          "-H", "--aml", "--amb",
          "--client", "opengl100",
          "--stdin", "-S", type
        };
        return glslangValidator(source, args).replace("stdin", "");
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

void ProcessSource::prepare(bool itemsChanged, bool manualEvaluation)
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
    mOutput.clear();
}

void ProcessSource::render()
{
    if (mNewShader) {
        mScriptEngine.reset();
        mShader.reset(mNewShader.take());

        if (mValidateSource)
            mShader->compile();

        auto source = QString();
        Singletons::fileCache().getSource(mFileName, &source);
        if (mProcessType == "preprocess")
            mOutput = preprocess(source);
        else if (mProcessType == "spirv")
            mOutput = generateSpirV(source, mSourceType);
        else if (mProcessType == "assembly")
            mOutput = mShader->getAssembly();
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

void ProcessSource::finish(bool steadyEvaluation)
{
    Q_UNUSED(steadyEvaluation);
    emit outputChanged(mOutput);
}

void ProcessSource::release()
{
    mShader.reset();
}
