#include "GLProgram.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "GLStream.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>
#include <array>

namespace {
    QString getUniformBaseName(QString name)
    {
        static const auto brackets = QRegularExpression("\\[.*\\]");
        name.remove(brackets);
        return name;
    }

    bool operator==(const GLProgram::Interface::BufferBindingPoint &a,
        const GLProgram::Interface::BufferBindingPoint &b)
    {
        return std::tie(a.target, a.index) == std::tie(b.target, b.index);
    }

    template <typename C, typename T>
    bool containsValue(const C &container, const T &value)
    {
        for (const auto &[key, val] : container)
            if (val == value)
                return true;
        return false;
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
        if (type != Shader::ShaderType::Includable)
            mShaders.emplace_back(type, list, session);
}

bool GLProgram::operator==(const GLProgram &rhs) const
{
    return (std::tie(mShaders) == std::tie(rhs.mShaders)
        && !shaderSessionSettingsDiffer(mSession, rhs.mSession));
}

bool GLProgram::link()
{
    if (mProgramObject)
        return true;
    if (mFailed)
        return false;

    if (!compileShaders() || !linkProgram()) {
        mFailed = true;
        return false;
    }
    fillInterface(mProgramObject, mInterface);

    // remove printf binding point from interface
    const auto name = mPrintf.bufferBindingName();
    const auto it = mInterface.bufferBindingPoints.find(name);
    if (it != mInterface.bufferBindingPoints.end()) {
        mPrintfBufferBindingPoint = it->second;
        mInterface.bufferBindingPoints.erase(it);
    }
    return true;
}

bool GLProgram::compileShaders()
{
    for (auto &shader : mShaders)
        if (!shader.compile(mPrintf))
            return false;
    return true;
}

bool GLProgram::linkProgram()
{
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
        GLShader::parseLog(log.data(), mLinkMessages, mItemId, {});
    }
    if (status != GL_TRUE)
        return false;

    mProgramObject = std::move(program);
    return true;
}

void GLProgram::fillInterface(GLuint program, Interface &interface)
{
    auto &gl = GLContext::currentContext();
    auto buffer = std::array<char, 256>();
    auto size = GLint{};
    auto type = GLenum{};
    auto nameLength = GLint{};

    auto uniforms = GLint{};
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
    for (auto i = 0u; i < static_cast<GLuint>(uniforms); ++i) {
        gl.glGetActiveUniform(program, i, static_cast<GLsizei>(buffer.size()),
            &nameLength, &size, &type, buffer.data());

        const auto name = QString(buffer.data());
        const auto location =
            gl.glGetUniformLocation(program, qPrintable(name));

        if (location >= 0) {
            interface.uniforms[name] = {
                .location = location,
                .dataType = type,
                .size = size,
            };
            if (name.endsWith("[0]")) {
                const auto baseName = name.left(name.size() - 3);
                for (auto j = 1; j < size; ++j) {
                    const auto elementName =
                        QString("%1[%2]").arg(baseName).arg(j);
                    const auto location = gl.glGetUniformLocation(program,
                        qPrintable(elementName));
                    Q_ASSERT(location >= 0);
                    interface.uniforms[elementName] = {
                        .location = location,
                        .dataType = type,
                        .size = 1,
                    };
                }
            }
        }

#if GL_VERSION_4_2
        if (auto gl42 = gl.v4_2) {
            auto atomicCounterIndex = 0;
            gl.glGetActiveUniformsiv(program, 1, &i,
                GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX, &atomicCounterIndex);
            if (atomicCounterIndex >= 0) {
                auto bufferBindingIndex = GLint{};
                gl42->glGetActiveAtomicCounterBufferiv(program,
                    atomicCounterIndex, GL_ATOMIC_COUNTER_BUFFER_BINDING,
                    &bufferBindingIndex);
                auto bindingPoint = Interface::BufferBindingPoint{
                    .target = GL_ATOMIC_COUNTER_BUFFER,
                    .index = static_cast<GLuint>(bufferBindingIndex),
                };
                if (!containsValue(interface.bufferBindingPoints, bindingPoint))
                    interface.bufferBindingPoints[name] = bindingPoint;

                // uniform is set by atomic counter buffer
                interface.uniforms.erase(name);
            }
        }
#endif
    }

    auto uniformBlocks = GLint{};
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &uniformBlocks);
    for (auto i = 0u; i < static_cast<GLuint>(uniformBlocks); ++i) {
        gl.glGetActiveUniformBlockName(program, i,
            static_cast<GLsizei>(buffer.size()), &nameLength, buffer.data());
        const auto name = QString(buffer.data());
        const auto uniformBlockIndex =
            gl.glGetUniformBlockIndex(program, qPrintable(name));
        const auto uniformBlockBinding = i;
        gl.glUniformBlockBinding(program, uniformBlockIndex,
            uniformBlockBinding);

        gl.glGetActiveUniformBlockiv(program, i,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniforms);
        auto elements = std::map<QString, Interface::BufferElement>();
        auto uniformIndices = std::vector<GLint>(static_cast<size_t>(uniforms));
        gl.glGetActiveUniformBlockiv(program, i,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, uniformIndices.data());
        for (GLuint index : uniformIndices) {
            gl.glGetActiveUniform(program, index,
                static_cast<GLsizei>(buffer.size()), &nameLength, &size, &type,
                buffer.data());
            const auto name = QString(buffer.data());
            auto offset = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index, GL_UNIFORM_OFFSET,
                &offset);
            auto arrayStride = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index,
                GL_UNIFORM_ARRAY_STRIDE, &arrayStride);
            auto matrixStride = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index,
                GL_UNIFORM_MATRIX_STRIDE, &matrixStride);
            auto isRowMajor = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index,
                GL_UNIFORM_IS_ROW_MAJOR, &isRowMajor);
            elements[name] = {
                .dataType = type,
                .size = size,
                .offset = offset,
                .arrayStride = arrayStride,
                .matrixStride = matrixStride,
                .isRowMajor = (isRowMajor != 0),
            };
        }

        interface.bufferBindingPoints[name] = {
            .target = GL_UNIFORM_BUFFER,
            .index = uniformBlockBinding,
            .elements = std::move(elements),
        };
    }

    auto attributes = GLint{};
    gl.glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributes);
    for (auto i = 0u; i < static_cast<GLuint>(attributes); ++i) {
        gl.glGetActiveAttrib(program, i, static_cast<GLsizei>(buffer.size()),
            &nameLength, &size, &type, buffer.data());
        const auto name = QString(buffer.data());
        interface.attributeLocations[name] =
            gl.glGetAttribLocation(program, qPrintable(name));
    }

    if (auto gl40 = gl.v4_0) {
        auto stages = QSet<Shader::ShaderType>();
        for (const auto &shader : mShaders)
            if (shader.type())
                stages += shader.type();

        auto subroutineIndices = std::vector<GLint>();
        for (const auto &stage : std::as_const(stages)) {
            gl40->glGetProgramStageiv(program, stage,
                GL_ACTIVE_SUBROUTINE_UNIFORMS, &uniforms);
            for (auto i = 0u; i < static_cast<GLuint>(uniforms); ++i) {
                gl40->glGetActiveSubroutineUniformName(program, stage, i,
                    static_cast<GLsizei>(buffer.size()), &nameLength,
                    buffer.data());
                auto name = QString(buffer.data());

                auto compatible = GLint{};
                gl40->glGetActiveSubroutineUniformiv(program, stage, i,
                    GL_NUM_COMPATIBLE_SUBROUTINES, &compatible);

                subroutineIndices.resize(static_cast<size_t>(compatible));
                gl40->glGetActiveSubroutineUniformiv(program, stage, i,
                    GL_COMPATIBLE_SUBROUTINES, subroutineIndices.data());

                auto subroutines = QStringList();
                for (auto index : subroutineIndices) {
                    gl40->glGetActiveSubroutineName(program, stage,
                        static_cast<GLuint>(index),
                        static_cast<GLsizei>(buffer.size()), &nameLength,
                        buffer.data());
                    subroutines += QString(buffer.data());
                }
                interface.stageSubroutines[stage].push_back({
                    .name = name,
                    .subroutines = subroutines,
                });
            }
        }
    }

#if GL_VERSION_4_3
    if (auto gl43 = gl.v4_3) {
        auto maxNameLength = GLint{};
        gl43->glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK,
            GL_MAX_NAME_LENGTH, &maxNameLength);
        auto buffer = std::vector<char>(maxNameLength);

        auto shaderStorageBlocks = GLint{};
        gl43->glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK,
            GL_ACTIVE_RESOURCES, &shaderStorageBlocks);
        for (auto i = 0u; i < static_cast<GLuint>(shaderStorageBlocks); ++i) {
            gl43->glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, i,
                static_cast<GLsizei>(buffer.size()), nullptr, buffer.data());
            const auto name = QString(buffer.data());
            const auto storageBlockIndex = gl.v4_3->glGetProgramResourceIndex(
                program, GL_SHADER_STORAGE_BLOCK, qPrintable(name));
            const auto storageBlockBinding = i;
            gl43->glShaderStorageBlockBinding(program, storageBlockIndex,
                storageBlockBinding);
            interface.bufferBindingPoints[name] = {
                .target = GL_SHADER_STORAGE_BUFFER,
                .index = storageBlockBinding,
            };
        }
    }
#endif
}

bool GLProgram::bind()
{
    if (!link())
        return false;

    auto &gl = GLContext::currentContext();
    gl.glUseProgram(mProgramObject);

    applyPrintfBindings();

    return true;
}

void GLProgram::unbind()
{
    auto &gl = GLContext::currentContext();
    gl.glUseProgram(GL_NONE);

    if (mPrintf.isUsed())
        mPrintfMessages = mPrintf.formatMessages(mItemId);
}

void GLProgram::applyPrintfBindings()
{
    if (!mPrintf.isUsed())
        return;

    mPrintf.clear();

    auto &gl = GLContext::currentContext();
    gl.glBindBufferBase(mPrintfBufferBindingPoint.target,
        mPrintfBufferBindingPoint.index, mPrintf.bufferObject());
}
