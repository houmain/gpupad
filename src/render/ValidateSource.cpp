#include "ValidateSource.h"
#include "GLShader.h"
#include "scripting/ScriptEngine.h"
#include <QProcess>

namespace {
    QString generateSpirv(QString source, SourceType sourceType)
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
} // namespace

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

void ValidateSource::setAssembleSource(bool active)
{
    mAssembleSource = active;
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
    mAssembly.clear();
}

void ValidateSource::render()
{
    if (mNewShader) {
        mScriptEngine.reset();
        mShader.reset(mNewShader.take());
        mShader->compile();

        if (mAssembleSource) {
            auto source = QString();
            Singletons::fileCache().getSource(mFileName, &source);
            mAssembly = generateSpirv(source, mSourceType);

            if (mAssembly.isEmpty())
                mAssembly = mShader->getAssembly();
        }
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
    emit assemblyChanged(mAssembly);
}

void ValidateSource::release()
{
    mShader.reset();
}
