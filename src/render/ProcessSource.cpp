#include "ProcessSource.h"
#include "ShaderPrintf.h"
#include "Settings.h"
#include "Singletons.h"
#include "FileCache.h"
#include "SynchronizeLogic.h"
#include "opengl/GLShader.h"
#include "vulkan/VKShader.h"
#include "session/SessionModel.h"
#include "scripting/ScriptEngineJavaScript.h"
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

ProcessSource::ProcessSource(QObject *parent) : RenderTask(parent) { }

ProcessSource::~ProcessSource()
{
    releaseResources();
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

bool ProcessSource::initialize()
{
    return true;
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

        const auto &session = Singletons::sessionModel();
        const auto &sessionItem = *session.item<Session>(session.index(0, 0));
        switch (renderer().api()) {
        case RenderAPI::OpenGL:
            mShader.reset(new GLShader(shaderType, shaders, sessionItem));
            break;
        case RenderAPI::Vulkan:
            mShader.reset(new VKShader(shaderType, shaders, sessionItem));
            break;
        }
    }
}

void ProcessSource::render()
{
    auto messages = MessagePtrSet();
    mScriptEngine.reset();
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
                mShader->generateSpirv(printf);
            }
        } else {
            if (mSourceType == SourceType::JavaScript)
                mScriptEngine.reset(new ScriptEngineJavaScript());

            if (mScriptEngine) {
                auto scriptSource = QString();
                Singletons::fileCache().getSource(mFileName, &scriptSource);
                mScriptEngine->validateScript(scriptSource, mFileName,
                    messages);
            }
        }
    }

    if (mShader && !mProcessType.isEmpty()) {
        if (mProcessType == "preprocess") {
            mOutput = removeLineDirectives(mShader->preprocess());
        } else if (mProcessType == "spirv") {
            mOutput = mShader->generateSpirvReadable();
        } else if (mProcessType == "ast") {
            mOutput = mShader->generateGLSLangAST();
        } else if (mProcessType == "assembly") {
            if (renderer().api() == RenderAPI::OpenGL)
                if (auto shader = static_cast<GLShader *>(mShader.get())) {
                    auto printf = RemoveShaderPrintf();
                    if (shader->compile(printf))
                        mOutput = tryGetProgramBinary(*shader);
                }
        }
        if (mOutput.isEmpty())
            mOutput = "not available";
        messages += mShader->resetMessages();
        mShader.reset();
    }
    mMessages = messages;
}

void ProcessSource::finish()
{
    Q_EMIT outputChanged(mOutput);
}

void ProcessSource::release()
{
    Q_ASSERT(!mShader);
}
