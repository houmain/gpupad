#include "GLProgram.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "GLStream.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>
#include <array>

namespace {
    QString formatNvGpuProgram(QString assembly)
    {
        // indent conditional jumps
        auto indent = 0;
        auto lines = assembly.split('\n');
        for (auto &line : lines) {
            if (line.startsWith("ENDIF") || line.startsWith("ENDREP")
                || line.startsWith("ELSE"))
                indent -= 2;

            const auto lineIndent = indent;

            if (line.startsWith("IF") || line.startsWith("REP")
                || line.startsWith("ELSE"))
                indent += 2;

            line.prepend(QString(lineIndent, ' '));
        }
        return lines.join('\n');
    }
} // namespace

GLProgram::GLProgram(const Program &program, const Session &session)
    : mItemId(program.id)
    , mSession(session)
{
    mUsedItems += program.id;
    mUsedItems += session.id;

    auto shaders = std::map<Shader::ShaderType, QList<const Shader *>>();
    for (const auto &item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;
            shaders[shader->shaderType].append(shader);
        }

    for (const auto &[type, list] : shaders)
        (type == Shader::ShaderType::Includable ? mIncludableShaders : mShaders)
            .emplace_back(type, list, session);
}

bool GLProgram::operator==(const GLProgram &rhs) const
{
    return (std::tie(mShaders, mIncludableShaders)
            == std::tie(rhs.mShaders, rhs.mIncludableShaders)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool GLProgram::validate()
{
    return link(GLContext::currentContext());
}

bool GLProgram::link(GLContext &context)
{
    if (mProgramObject)
        return true;
    if (mFailed)
        return false;

    if (!compileShaders() || !linkProgram()) {
        mFailed = true;
        return false;
    }

    generateReflectionFromProgram(mProgramObject);
    automapDescriptorBindings();
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    return true;
}

bool GLProgram::compileShaders()
{
    if (mSession.shaderCompiler == Session::ShaderCompiler::Driver) {
        for (auto &shader : mShaders)
            if (!shader.compile(mPrintf))
                return false;
    } else {
        auto inputs = std::vector<Spirv::Input>();
        for (auto &shader : mShaders)
            inputs.push_back(shader.getSpirvCompilerInput(mPrintf));

        mStageSpirv = Spirv::compile(mSession, inputs, mItemId, mMessages);
        for (auto &shader : mShaders)
            if (!shader.specialize(mStageSpirv[shader.type()]))
                return false;
    }
    return true;
}

bool GLProgram::linkProgram()
{
    if (mShaders.empty()) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::ProgramHasNoShader);
        return false;
    }

    auto freeProgram = [](GLuint program) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteProgram(program);
    };

    auto &gl = GLContext::currentContext();
    auto program = GLObject(gl.glCreateProgram(), freeProgram);
    for (auto &shader : mShaders)
        gl.glAttachShader(program, shader.shaderObject());

    gl.glLinkProgram(program);
    auto status = GLint{};
    auto length = GLint{};
    gl.glGetProgramiv(program, GL_LINK_STATUS, &status);
    gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        auto log = std::vector<char>(static_cast<size_t>(length));
        gl.glGetProgramInfoLog(program, length, nullptr, log.data());
        GLShader::parseLog(log.data(), mMessages, mItemId, {});
    } else {
        if (status != GL_TRUE)
            mMessages += MessageList::insert(mItemId,
                MessageType::CreatingPipelineFailed);
    }
    if (status != GL_TRUE)
        return false;

    mProgramObject = std::move(program);
    return true;
}

bool GLProgram::bind()
{
    auto &gl = GLContext::currentContext();
    if (!link(gl))
        return false;

    gl.glUseProgram(mProgramObject);
    return true;
}

void GLProgram::unbind()
{
    auto &gl = GLContext::currentContext();
    gl.glUseProgram(GL_NONE);
}

GLBuffer &GLProgram::getDynamicUniformBuffer(const QString &name, int size)
{
    auto it = mDynamicUniformBuffers.find(name);
    if (it == mDynamicUniformBuffers.end())
        it = mDynamicUniformBuffers.emplace(name, size).first;
    return it->second;
}

MessagePtrSet GLProgram::resetMessages()
{
    for (auto &shader : mShaders)
        mMessages += shader.resetMessages();
    return std::exchange(mMessages, {});
}

QString GLProgram::tryGetProgramBinary()
{
    auto &gl = GLContext::currentContext();
    if (!link(gl))
        return {};

    auto length = GLint{};
    gl.glGetProgramiv(mProgramObject, GL_PROGRAM_BINARY_LENGTH, &length);
    if (length == 0)
        return {};

    auto format = GLuint{};
    auto binary = std::string(static_cast<size_t>(length + 1), ' ');
    gl.glGetProgramBinary(mProgramObject, length, nullptr, &format, &binary[0]);

    // only supported on Nvidia so far
    const auto begin = binary.find("!!NV");
    const auto end = binary.rfind("END");
    if (begin == std::string::npos || end == std::string::npos)
        return {};

    return formatNvGpuProgram(
        QString::fromUtf8(&binary[begin], static_cast<int>(end - begin)));
}