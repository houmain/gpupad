#include "ProcessSource.h"
#include "ShaderPrintf.h"
#include "Settings.h"
#include "Singletons.h"
#include "FileCache.h"
#include "SynchronizeLogic.h"
#include "opengl/GLShader.h"
#include "vulkan/VKShader.h"
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

    QList<const Shader *> getShadersInSession(const QString &fileName)
    {
        auto shaders = QList<const Shader *>();
        if (auto currentShader =
                castItem<Shader>(findShaderInSession(fileName)))
            if (auto program = castItem<Program>(currentShader->parent))
                for (auto child : program->items)
                    if (auto shader = castItem<Shader>(child))
                        if (shader->shaderType == currentShader->shaderType)
                            shaders.append(shader);
        return shaders;
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
    if (auto shaderType = getShaderType(mSourceType)) {
        auto shaders = getShadersInSession(mFileName);
        // ensure shader is in list
        auto shader = Shader{};
        if (shaders.empty()) {
            shader.fileName = mFileName;
            shader.language = Shader::Language::GLSL;
            shaders = { &shader };
        }
        // replace shader's type and language
        for (auto &s : shaders)
            if (s->fileName == mFileName) {
                shader = *s;
                shader.shaderType = shaderType;
                shader.language = getShaderLanguage(mSourceType);
                s = &shader;
                break;
            }

        auto session = Singletons::sessionModel().sessionItem();

        // define state of hidden session properties
        const auto hasVulkanRenderer = (session.renderer == "Vulkan");
        const auto hasShaderCompiler =
            (!session.shaderCompiler.isEmpty() || hasVulkanRenderer);
        if (!hasShaderCompiler) {
            session.autoMapBindings = true;
            session.autoMapLocations = true;
        }

        // always target Vulkan when generating JSON, otherwise SpvReflect cannot enumerate uniforms
        if (session.renderer == "Vulkan" || mProcessType == "json") {
            mShader = std::make_unique<VKShader>(shaderType, shaders, session);
        } else {
            mShader = std::make_unique<GLShader>(shaderType, shaders, session);
        }
    }
}

void ProcessSource::render()
{
    auto messages = MessagePtrSet();
    mOutput.clear();

    if (mValidateSource) {
        if (mShader) {
            auto printf = RemoveShaderPrintf();
            if (renderer().api() == RenderAPI::OpenGL) {
                if (auto shader = static_cast<GLShader *>(mShader.get()))
                    if (shader->compile(printf)) {
                        // try to link and if it also succeeds,
                        // output messages from linking to get potential warnings
                        tryGetLinkerWarnings(*shader, messages);
                    }
            } else {
                mShader->compileSpirv(printf);
            }
        } else if (mSourceType == SourceType::JavaScript) {
            const auto basePath = QFileInfo(mFileName).absolutePath();
            auto scriptEngine = ScriptEngine::make(basePath);
            auto scriptSource = QString();
            Singletons::fileCache().getSource(mFileName, &scriptSource);
            scriptEngine->validateScript(scriptSource, mFileName);
            messages += scriptEngine->resetMessages();
        }
    }

    if (mShader && !mProcessType.isEmpty()) {
        if (mProcessType == "preprocess") {
            mOutput = removeLineDirectives(mShader->preprocess());
        } else if (mProcessType == "spirv") {
            mOutput = mShader->generateReadableSpirv();
        } else if (mProcessType == "spirvBinary") {
            mOutput = mShader->generateBinarySpirv();
        } else if (mProcessType == "ast") {
            mOutput = mShader->generateGLSLangAST();
        } else if (mProcessType == "assembly") {
            if (renderer().api() == RenderAPI::OpenGL)
                if (auto shader = static_cast<GLShader *>(mShader.get())) {
                    auto printf = RemoveShaderPrintf();
                    if (shader->compile(printf))
                        mOutput = tryGetProgramBinary(*shader);
                }
        } else if (mProcessType == "json") {
            mOutput = mShader->getJsonInterface();
        }
        if (!mOutput.isValid())
            mOutput = "not available";
    }

    if (mShader) {
        messages += mShader->resetMessages();
        mShader.reset();
    }
    mMessages = messages;
}

void ProcessSource::finish()
{
    if (mOutput.isValid())
        Q_EMIT outputChanged(mOutput);
}
