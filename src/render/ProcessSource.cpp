#include "ProcessSource.h"
#include "Singletons.h"
#include "FileCache.h"
#include "SynchronizeLogic.h"
#include "opengl/GLProgram.h"
#include "vulkan/VKShader.h"
#include "direct3d/D3DShader.h"
#include "session/SessionModel.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>

namespace {
    QString removeLineDirectives(QString source)
    {
        static const auto regex = QRegularExpression("(\\s*#line[^\n]*)",
            QRegularExpression::MultilineOption);

        for (auto match = regex.match(source); match.hasMatch();
            match = regex.match(source))
            source.remove(match.capturedStart(), match.capturedLength());
        return source;
    }

    const Shader *findShaderInSession(const Shader &shader)
    {
        // also find shader with other shader type
        auto result = std::add_pointer_t<const Shader>();
        Singletons::sessionModel().forEachItem([&](const Item &item) {
            if (auto sessionShader = castItem<Shader>(item))
                if (sessionShader->fileName == shader.fileName)
                    if (!result
                        || sessionShader->shaderType == shader.shaderType)
                        result = sessionShader;
        });
        return result;
    }

    Shader::ShaderType getStage(Shader::ShaderType type)
    {
        using ST = Shader::ShaderType;
        switch (type) {
        case ST::Vertex:
        case ST::Geometry:
        case ST::TessControl:
        case ST::TessEvaluation: return ST::Vertex;
        default:                 return type;
        }
    }

    void getShadersFromSession(Shader &shader, QList<const Shader *> &shaders,
        bool linkingProgram)
    {
        if (auto sessionShader = findShaderInSession(shader)) {
            const auto program = castItem<Program>(sessionShader->parent);
            for (auto item : program->items)
                if (auto child = castItem<Shader>(item)) {
                    if (child == sessionShader) {
                        // copy everything but type from session shader
                        const auto shaderType = shader.shaderType;
                        shader = *child;
                        shader.shaderType = shaderType;
                        shaders.append(&shader);
                    } else {
                        // add other shaders of program with same stage
                        if (child->shaderType == shader.shaderType) {
                            shaders.append(child);
                        } else if (linkingProgram
                            && getStage(child->shaderType)
                                == getStage(shader.shaderType)) {
                            shaders.append(child);
                        }
                    }
                }
        } else {
            shaders.append(&shader);
        }
    }
} // namespace

ProcessSource::ProcessSource(RendererPtr renderer, QObject *parent)
    : RenderTask(std::move(renderer), parent)
{
}

ProcessSource::~ProcessSource()
{
    releaseResources();
    Q_ASSERT(!mShader);
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

void ProcessSource::clearMessages()
{
    mMessages.clear();
}

void ProcessSource::prepare(bool itemsChanged, EvaluationType)
{
    auto shaderType = getShaderType(mSourceType);
    if (shaderType)
        prepareShader(shaderType);
}

void ProcessSource::prepareShader(Shader::ShaderType shaderType)
{
    auto session = Singletons::sessionModel().sessionItem();
    session.shaderLanguage = getShaderLanguage(mSourceType);

    const auto linkingProgram = [&]() {
        if (session.renderer != Session::Renderer::OpenGL)
            return false;
        return (mValidateSource || mProcessType == "programBinary"
            || mProcessType == "json");
    }();

    auto shader = Shader{};
    auto shaders = QList<const Shader *>();
    shader.fileName = mFileName;
    shader.shaderType = shaderType;
    getShadersFromSession(shader, shaders, linkingProgram);

    // define state of hidden session properties
    const auto hasShaderCompiler =
        (session.shaderCompiler != Session::ShaderCompiler::Driver
            || session.renderer == Session::Renderer::Vulkan);
    if (!hasShaderCompiler) {
        setShaderCompilerSetting(session,
            Session::ShaderCompilerSetting::autoMapBindings, true);
        setShaderCompilerSetting(session,
            Session::ShaderCompilerSetting::autoMapLocations, true);
    }

    switch (session.renderer) {
    case Session::Renderer::OpenGL: {
        if (linkingProgram) {
            auto program = Program{};
            for (auto shader : shaders)
                program.items.append(const_cast<Shader *>(shader));
            mGLProgram = std::make_unique<GLProgram>(program, session);
        } else {
            mShader = std::make_unique<GLShader>(shaderType, shaders, session);
        }
        break;
    }

    default:
    case Session::Renderer::Vulkan:
        mShader = std::make_unique<VKShader>(shaderType, shaders, session);
        break;

#if defined(_WIN32)
    case Session::Renderer::Direct3D:
        mShader = std::make_unique<D3DShader>(shaderType, shaders, session);
        break;
#endif
    }
}

void ProcessSource::render()
{
    auto prevMessages = std::exchange(mMessages, {});
    mOutput.clear();

    if (mValidateSource)
        validate();

    if (!mProcessType.isEmpty()) {
        mOutput = process();
        if (!mOutput.isValid())
            mOutput = "not available";
    }

    if (mGLProgram) {
        mMessages += mGLProgram->resetMessages();
        mGLProgram.reset();
    }

    if (mShader) {
        mMessages += mShader->resetMessages();
        mShader.reset();
    }
}

void ProcessSource::validate()
{
    if (mGLProgram) {
        mGLProgram->validate();
    } else if (mShader) {
        mShader->validate();
    } else if (mSourceType == SourceType::JavaScript) {
        const auto basePath = QFileInfo(mFileName).absolutePath();
        auto scriptEngine = ScriptEngine::make(basePath);
        auto scriptSource = QString();
        Singletons::fileCache().getSource(mFileName, &scriptSource);
        scriptEngine->validateScript(scriptSource, mFileName);
        mMessages += scriptEngine->resetMessages();
    }
}

QVariant ProcessSource::process()
{
    if (mProcessType == "preprocess" && mShader)
        return removeLineDirectives(mShader->preprocess());

    if (mProcessType == "glsl" && mShader)
        return mShader->generateGLSL();

    if (mProcessType == "hlsl" && mShader)
        return mShader->generateHLSL();

    if (mProcessType == "spirv" && mShader)
        return mShader->disassemble();

    if (mProcessType == "spirvBinary" && mShader) {
        if (const auto spirv = mShader->compileSpirv(); !spirv.empty())
            return QByteArray(reinterpret_cast<const char *>(spirv.data()),
                spirv.size() * sizeof(uint32_t));
        for (const auto &message : mShader->resetMessages())
            return message->text;
    }

    if (mProcessType == "ast" && mShader)
        return mShader->generateGLSLangAST();

    if (mProcessType == "programBinary" && mGLProgram)
        return mGLProgram->tryGetProgramBinary();

    if (mProcessType == "json") {
        if (mGLProgram && mGLProgram->validate())
            return getJsonString(mGLProgram->reflection());

        if (mShader)
            return getJsonString(mShader->getReflection());
    }
    return {};
}

void ProcessSource::finish()
{
    if (mOutput.isValid())
        Q_EMIT outputChanged(mOutput);
}
