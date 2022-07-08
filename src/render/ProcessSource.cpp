#include "ProcessSource.h"
#include "GLShader.h"
#include "GLPrintf.h"
#include "scripting/ScriptEngineJavaScript.h"
#include "scripting/ScriptEngineLua.h"
#include "Singletons.h"
#include "Settings.h"
#include "session/SessionModel.h"
#include "glslang.h"

namespace {
    QString removeLineDirectives(QString source)
    {
        static const auto regex = QRegularExpression("(\\s*#line[^\n]*)",
            QRegularExpression::MultilineOption);

        for (auto match = regex.match(source); match.hasMatch(); match = regex.match(source))
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

void ProcessSource::prepare(bool, EvaluationType)
{
    auto shaderPreamble = Singletons::settings().shaderPreamble();
    auto shaderIncludePaths = Singletons::settings().shaderIncludePaths();

    if (auto shaderType = getShaderType(mSourceType)) {
        auto shaders = getShadersInSession(mFileName);
        auto shader = Shader{ };
        if (shaders.size() <= 1) {
            shader.fileName = mFileName;
            shader.shaderType = shaderType;
            shader.language = getShaderLanguage(mSourceType);
            shaders = { &shader };
        }
        mShader.reset(new GLShader(shaderType, shaders, 
            shaderPreamble, shaderIncludePaths));
    }
}

void ProcessSource::render()
{
    auto messages = MessagePtrSet();
    mScriptEngine.reset();
    mOutput.clear();

    if (mValidateSource) {
        if (mShader) {
            auto glPrintf = GLPrintf();
            mShader->compile(&glPrintf);
        }
        else {
            if (mSourceType == SourceType::JavaScript)
                mScriptEngine.reset(new ScriptEngineJavaScript());
            else if (mSourceType == SourceType::Lua)
                mScriptEngine.reset(new ScriptEngineLua());

            if (mScriptEngine) {
                  auto scriptSource = QString();
                  Singletons::fileCache().getSource(mFileName, &scriptSource);
                  mScriptEngine->validateScript(scriptSource, mFileName, messages);
            }
        }
    }

    if (mShader) {
        auto usedFileNames = QStringList();
        auto glPrintf = GLPrintf();
        const auto source = mShader->getPatchedSources(
            messages, usedFileNames, &glPrintf).join("\n");

        if (mProcessType == "preprocess") {
            mOutput = removeLineDirectives(glslang::preprocess(source, messages));
        }
        else if (mProcessType == "spirv") {
            mOutput = glslang::generateSpirV(source,
                getShaderType(mSourceType), messages);
        }
        else if (mProcessType == "ast") {
            mOutput = glslang::generateAST(source,
                getShaderType(mSourceType), messages);
        }
        else if (mProcessType == "assembly") {
            mOutput = mShader->getAssembly();
        }
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
