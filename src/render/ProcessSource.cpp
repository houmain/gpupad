#include "ProcessSource.h"
#include "GLShader.h"
#include "scripting/ScriptEngine.h"
#include <QProcess>
#include <QDir>

namespace {
    const char *getGLSLangSourceType(SourceType sourceType)
    {
        switch (sourceType) {
            case SourceType::VertexShader: return "vert";
            case SourceType::FragmentShader: return "frag";
            case SourceType::TesselationControl: return "tesc";
            case SourceType::TesselationEvaluation: return "tese";
            case SourceType::GeometryShader: return "geom";
            case SourceType::ComputeShader: return "comp";
            default: return nullptr;
        }
    }

    QString executeGLSLangValidator(QString source, QStringList args)
    {
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

        return QString::fromUtf8(data);
    }

    QString preprocess(QString source)
    {
        const auto args = QStringList{ "-E", "--stdin", "-S", "vert" };
        return executeGLSLangValidator(source, args);
    }

    QString generateSpirV(QString source, SourceType sourceType)
    {
        const auto type = getGLSLangSourceType(sourceType);
        if (!type)
            return "";

        const auto args = QStringList{
          "-H", "--aml", "--amb",
          "--client", "opengl100",
          "--stdin", "-S", type
        };
        return executeGLSLangValidator(source, args).replace("stdin", "");
    }

    QString generateAST(QString source, SourceType sourceType)
    {
        const auto type = getGLSLangSourceType(sourceType);
        if (!type)
            return "";

        const auto args = QStringList{
          "-i", "--aml", "--amb",
          "--client", "opengl100",
          "--stdin", "-S", type
        };
        return executeGLSLangValidator(source, args).replace("stdin", "");
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
            case SourceType::TesselationControl:
                return Shader::ShaderType::TessellationControl;
            case SourceType::TesselationEvaluation:
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

    QList<const Shader*> getHeadersInSession(const QString &fileName)
    {
        auto shaders = QList<const Shader*>();
        if (auto shader = findShaderInSession(fileName))
            if (auto program = castItem<Program>(shader->parent)) {
                auto index = program->items.indexOf(const_cast<Item*>(shader));
                for (auto i = index - 1; i >= 0; --i)
                    if (auto header = castItem<Shader>(program->items[i])) {
                        if (header->shaderType != Shader::ShaderType::Header)
                            break;
                        shaders.prepend(header);
                    }
            }
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
    if (auto shaderType = getShaderType(mSourceType)) {
        auto shaders = getHeadersInSession(mFileName);
        auto shader = Shader();
        shader.fileName = mFileName;
        shader.shaderType = shaderType;
        shaders.append(&shader);
        mNewShader.reset(new GLShader(shaders));
    }
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
            mScriptEngine->evaluateScript(scriptSource, mFileName);
        }
    }

    if (mShader) {
        if (mProcessType == "preprocess")
            mOutput = preprocess(mShader->getSource());
        else if (mProcessType == "spirv")
            mOutput = generateSpirV(mShader->getSource(), mSourceType);
        else if (mProcessType == "ast")
            mOutput = generateAST(mShader->getSource(), mSourceType);
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
