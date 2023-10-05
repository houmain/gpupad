#include "GLProcessSource.h"
#include "GLPrintf.h"
#include "scripting/ScriptEngineJavaScript.h"
#include "scripting/ScriptEngineLua.h"
#include "Singletons.h"
#include "Settings.h"
#include "SynchronizeLogic.h"
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

    void tryToGetLinkerWarnings(GLShader &shader, MessagePtrSet &messages) 
    {
        auto &gl = GLContext::currentContext();
        auto program = gl.glCreateProgram();
        gl.glAttachShader(program, shader.shaderObject());
        gl.glLinkProgram(program);
        auto status = GLint{ };
        gl.glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status == GL_TRUE) {
            auto length = GLint{ };
            gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            auto log = std::vector<char>(static_cast<size_t>(length));
            gl.glGetProgramInfoLog(program, length, nullptr, log.data());
            GLShader::parseLog(log.data(), messages, shader.itemId(), shader.fileNames());
        }
        gl.glDeleteProgram(program);
    }
} // namespace

void GLProcessSource::setFileName(QString fileName)
{
    mFileName = fileName;
}

void GLProcessSource::setSourceType(SourceType sourceType)
{
    mSourceType = sourceType;
}

void GLProcessSource::setValidateSource(bool validate)
{
    mValidateSource = validate;
}

void GLProcessSource::setProcessType(QString processType)
{
    mProcessType = processType;
}

void GLProcessSource::clearMessages()
{
    mMessages.clear();
}

void GLProcessSource::prepare()
{
    const auto shaderPreamble = QString{ Singletons::settings().shaderPreamble() + "\n" +
        Singletons::synchronizeLogic().sessionShaderPreamble() };
    const auto shaderIncludePaths = QString{ Singletons::settings().shaderIncludePaths() + "\n" +
        Singletons::synchronizeLogic().sessionShaderIncludePaths() };

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
        mShader.reset(new GLShader(shaderType, shaders, 
            shaderPreamble, shaderIncludePaths));
    }
}

void GLProcessSource::render()
{
    auto messages = MessagePtrSet();
    mScriptEngine.reset();
    mOutput.clear();

    if (mValidateSource) {
        if (mShader) {
            auto glPrintf = GLPrintf();
            if (mShader->compile(&glPrintf)) {
                // try to link and if it also succeeds,
                // output messages from linking to get potential warnings
                tryToGetLinkerWarnings(*mShader, messages);
            }
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

void GLProcessSource::release()
{
    Q_ASSERT(!mShader);
}
