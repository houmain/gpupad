#include "ProcessSource.h"
#include "scripting/ScriptEngineJavaScript.h"
#include "Singletons.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
#include "session/SessionModel.h"
#include "FileCache.h"
#include "ShaderBase.h"
#include "glslang.h"
#include <QRegularExpression>

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

ProcessSource::ProcessSource(QObject *parent)
    : RenderTask(parent)
{
}

ProcessSource::~ProcessSource() = default;

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

void ProcessSource::prepare(bool itemsChanged, EvaluationType evaluationType)
{
    const auto shaderPreamble = QString{ 
        Singletons::settings().shaderPreamble() + "\n" +
        Singletons::synchronizeLogic().sessionShaderPreamble()
    };
    const auto shaderIncludePaths = QString{ 
        Singletons::settings().shaderIncludePaths() + "\n" +
        Singletons::synchronizeLogic().sessionShaderIncludePaths() 
    };

    if (auto shaderType = getShaderType(mSourceType)) {
        auto shaders = getShadersInSession(mFileName);
        // ensure shader is in list
        auto shader = Shader{ };
        if (shaders.empty()) {
            shader.fileName = mFileName;
            shader.language = Shader::Language::GLSL;
            shaders = { &shader };
        }
        // replace shader's type and language
        for (auto& s : shaders)
            if (s->fileName == mFileName) {
                shader = *s;
                shader.shaderType = shaderType;
                shader.language = getShaderLanguage(mSourceType);
                s = &shader;
                break;
            }
        //mShader.reset(new GLShader(shaderType, shaders, 
        //    shaderPreamble, shaderIncludePaths));
    }

    if (mSourceType == SourceType::JavaScript)
        Singletons::fileCache().getSource(mFileName, &mScriptSource);
}

void ProcessSource::render()
{
    auto messages = MessagePtrSet();
    mOutput.clear();

    if (mValidateSource) {
        if (mSourceType == SourceType::JavaScript) {
            auto scriptEngine = ScriptEngineJavaScript();
            scriptEngine.validateScript(mScriptSource, mFileName, messages);
        }
        if (!mShaders.isEmpty()) {
            renderer().validateShaderProgram(mProgram, messages);
        }
    }

    //if (mShader) {
    //    auto usedFileNames = QStringList();
    //    auto printf = GLPrintf();
    //    const auto source = mShader->getPatchedSources(
    //        messages, usedFileNames, &printf).join("\n");
    //
    //    if (mProcessType == "preprocess") {
    //        mOutput = removeLineDirectives(glslang::preprocess(source, messages));
    //    }
    //    else if (mProcessType == "spirv") {
    //        mOutput = glslang::generateSpirV(source,
    //            getShaderType(mSourceType), messages);
    //    }
    //    else if (mProcessType == "ast") {
    //        mOutput = glslang::generateAST(source,
    //            getShaderType(mSourceType), messages);
    //    }
    //    else if (mProcessType == "assembly") {
    //        mOutput = mShader->getAssembly();
    //    }
    //    messages += mShader->resetMessages();
    //    mShader.reset();
    //}
    mMessages = messages;
}

void ProcessSource::finish()
{
    Q_EMIT outputChanged(mOutput);
}

void ProcessSource::release()
{
    //Q_ASSERT(!mShader);
}
