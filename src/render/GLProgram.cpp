#include <array>
#include "GLProgram.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "ScriptEngine.h"

GLProgram::GLProgram(const Program &program)
    : mItemId(program.id)
{
    mUsedItems += program.id;

    auto shaders = QList<const Shader*>();
    for (const auto& item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;

            shaders.append(shader);

            // headers are prepended to the next shader
            if (shader->type != Shader::Header) {
                mShaders.emplace_back(shaders);
                shaders.clear();
            }
        }
}

bool GLProgram::operator==(const GLProgram &rhs) const
{
    return (mShaders == rhs.mShaders);
}

bool GLProgram::link()
{
    if (mProgramObject)
        return true;

    auto freeProgram = [](GLuint program) {
        auto& gl = GLContext::currentContext();
        gl.glDeleteProgram(program);
    };

    auto& gl = GLContext::currentContext();
    auto program = GLObject(gl.glCreateProgram(), freeProgram);

    auto compilingShadersFailed = false;
    for (auto& shader : mShaders) {
        if (shader.compile())
            gl.glAttachShader(program, shader.shaderObject());
        else
            compilingShadersFailed = true;
    }
    if (compilingShadersFailed)
        return false;

    auto status = GLint{ };
    gl.glLinkProgram(program);
    gl.glGetProgramiv(program, GL_LINK_STATUS, &status);

    auto length = GLint{ };
    gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    auto log = std::vector<char>(static_cast<size_t>(length));
    gl.glGetProgramInfoLog(program, length, NULL, log.data());
    GLShader::parseLog(log.data(), mLinkMessages, mItemId, { });

    if (status != GL_TRUE)
        return false;

    auto buffer = std::array<char, 256>();
    auto size = GLint{ };
    auto type = GLenum{ };
    auto nameLength = GLint{ };

    auto uniforms = GLint{ };
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
    for (auto i = 0; i < uniforms; ++i) {
        gl.glGetActiveUniform(program, static_cast<GLuint>(i), buffer.size(),
            &nameLength, &size, &type, buffer.data());
        auto name = QString(buffer.data()).remove(QRegExp("\\[0\\]"));
        mUniformDataTypes[name] = type;
        mUniformsSet[name] = false;
    }

    auto uniformBlocks = GLint{ };
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &uniformBlocks);
    for (auto i = 0; i < uniformBlocks; ++i) {
        gl.glGetActiveUniformBlockName(program, static_cast<GLuint>(i),
            buffer.size(), &nameLength, buffer.data());
        auto name = QString(buffer.data());
        mUniformBlocksSet[name] = false;
    }

    auto attributes = GLint{ };
    gl.glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributes);
    for (auto i = 0; i < attributes; ++i) {
        gl.glGetActiveAttrib(program, static_cast<GLuint>(i), buffer.size(),
            &nameLength, &size, &type, buffer.data());
        auto name = QString(buffer.data());
        mAttributes.append(name);
    }

    mProgramObject = std::move(program);
    return true;
}

bool GLProgram::bind()
{
    if (!link())
        return false;

    auto& gl = GLContext::currentContext();
    gl.glUseProgram(mProgramObject);
    return true;
}

void GLProgram::unbind()
{
    auto& gl = GLContext::currentContext();
    gl.glUseProgram(GL_NONE);

    // inform about not set uniforms
    auto messages = MessagePtrSet();
    for (auto& kv : mUniformsSet) {
        if (!kv.second)
            messages += Singletons::messageList().insert(
                mItemId, MessageType::UnformNotSet, kv.first);
        kv.second = false;
    }
    for (auto& kv : mUniformBlocksSet) {
        if (!kv.second)
            messages += Singletons::messageList().insert(
                mItemId, MessageType::UnformNotSet, kv.first);
        kv.second = false;
    }
    mNotSetUniformsMessages = messages;
}

int GLProgram::getUniformLocation(const QString &name) const
{
    auto& gl = GLContext::currentContext();
    return gl.glGetUniformLocation(mProgramObject, qPrintable(name));
}

GLenum GLProgram::getUniformDataType(const QString &name) const
{
    return mUniformDataTypes[name];
}

int GLProgram::getAttributeLocation(const QString &name) const
{
    auto& gl = GLContext::currentContext();
    return gl.glGetAttribLocation(mProgramObject, qPrintable(name));
}

bool GLProgram::apply(const GLUniformBinding &uniform, ScriptEngine &scriptEngine)
{
    auto& gl = GLContext::currentContext();
    const auto transpose = false;
    const auto dataType = getUniformDataType(uniform.name);

    for (auto i = 0; i < uniform.values.size(); ++i) {
        auto name = uniform.name;
        if (i > 0)
            name += QStringLiteral("[%1]").arg(i);
        const auto location = getUniformLocation(name);
        if (location < 0)
            return false;
        mUniformsSet[uniform.name] = true;

        auto floats = std::array<GLfloat, 16>();
        auto ints = std::array<GLint, 16>();
        auto uints = std::array<GLuint, 16>();
        auto doubles = std::array<GLdouble, 16>();
        auto j = 0u;
        foreach (QString field, scriptEngine.evalValue(
                uniform.values[i].toStringList(), uniform.bindingItemId)) {
            floats.at(j) = field.toFloat();
            ints.at(j) = field.toInt();
            uints.at(j) = field.toUInt();
            doubles.at(j) = field.toDouble();
            ++j;
        }

#define ADD(TYPE, VALUES, FUNCTION) \
            case TYPE: \
                FUNCTION(location, 1, VALUES.data()); \
                break;

#define ADD_MATRIX(TYPE, VALUES, FUNCTION) \
            case TYPE: \
                FUNCTION(location, 1, transpose, VALUES.data()); \
                break;

        switch (dataType) {
            ADD(GL_FLOAT, floats, gl.glUniform1fv);
            ADD(GL_FLOAT_VEC2, floats, gl.glUniform2fv);
            ADD(GL_FLOAT_VEC3, floats, gl.glUniform3fv);
            ADD(GL_FLOAT_VEC4, floats, gl.glUniform4fv);
            ADD(GL_DOUBLE, doubles, gl.v4_0->glUniform1dv);
            ADD(GL_DOUBLE_VEC2, doubles, gl.v4_0->glUniform2dv);
            ADD(GL_DOUBLE_VEC3, doubles, gl.v4_0->glUniform3dv);
            ADD(GL_DOUBLE_VEC4, doubles, gl.v4_0->glUniform4dv);
            ADD(GL_INT, ints, gl.glUniform1iv);
            ADD(GL_INT_VEC2, ints, gl.glUniform2iv);
            ADD(GL_INT_VEC3, ints, gl.glUniform3iv);
            ADD(GL_INT_VEC4, ints, gl.glUniform4iv);
            ADD(GL_UNSIGNED_INT, uints, gl.glUniform1uiv);
            ADD(GL_UNSIGNED_INT_VEC2, uints, gl.glUniform2uiv);
            ADD(GL_UNSIGNED_INT_VEC3, uints, gl.glUniform3uiv);
            ADD(GL_UNSIGNED_INT_VEC4, uints, gl.glUniform4uiv);
            ADD(GL_BOOL, ints, gl.glUniform1iv);
            ADD(GL_BOOL_VEC2, ints, gl.glUniform2iv);
            ADD(GL_BOOL_VEC3, ints, gl.glUniform3iv);
            ADD(GL_BOOL_VEC4, ints, gl.glUniform4iv);
            ADD_MATRIX(GL_FLOAT_MAT2, floats, gl.glUniformMatrix2fv);
            ADD_MATRIX(GL_FLOAT_MAT3, floats, gl.glUniformMatrix3fv);
            ADD_MATRIX(GL_FLOAT_MAT4, floats, gl.glUniformMatrix4fv);
            ADD_MATRIX(GL_FLOAT_MAT2x3, floats, gl.glUniformMatrix2x3fv);
            ADD_MATRIX(GL_FLOAT_MAT2x4, floats, gl.glUniformMatrix2x4fv);
            ADD_MATRIX(GL_FLOAT_MAT3x2, floats, gl.glUniformMatrix3x2fv);
            ADD_MATRIX(GL_FLOAT_MAT3x4, floats, gl.glUniformMatrix3x4fv);
            ADD_MATRIX(GL_FLOAT_MAT4x2, floats, gl.glUniformMatrix4x2fv);
            ADD_MATRIX(GL_FLOAT_MAT4x3, floats, gl.glUniformMatrix4x3fv);
            ADD_MATRIX(GL_DOUBLE_MAT2, doubles, gl.v4_0->glUniformMatrix2dv);
            ADD_MATRIX(GL_DOUBLE_MAT3, doubles, gl.v4_0->glUniformMatrix3dv);
            ADD_MATRIX(GL_DOUBLE_MAT4, doubles, gl.v4_0->glUniformMatrix4dv);
            ADD_MATRIX(GL_DOUBLE_MAT2x3, doubles, gl.v4_0->glUniformMatrix2x3dv);
            ADD_MATRIX(GL_DOUBLE_MAT2x4, doubles, gl.v4_0->glUniformMatrix2x4dv);
            ADD_MATRIX(GL_DOUBLE_MAT3x2, doubles, gl.v4_0->glUniformMatrix3x2dv);
            ADD_MATRIX(GL_DOUBLE_MAT3x4, doubles, gl.v4_0->glUniformMatrix3x4dv);
            ADD_MATRIX(GL_DOUBLE_MAT4x2, doubles, gl.v4_0->glUniformMatrix4x2dv);
            ADD_MATRIX(GL_DOUBLE_MAT4x3, doubles, gl.v4_0->glUniformMatrix4x3dv);
        }
    }
#undef ADD
#undef ADD_MATRIX
    return true;
}

bool GLProgram::apply(const GLSamplerBinding &binding, int unit)
{
    auto location = getUniformLocation(binding.name);
    if (location < 0)
        return false;

    mUniformsSet[binding.name] = true;

    if (!binding.texture)
        return false;

    auto& gl = GLContext::currentContext();
    auto& texture = *binding.texture;
    const auto target = texture.target();
    gl.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    gl.glBindTexture(target, texture.getReadOnlyTextureId());
    gl.glUniform1i(location + binding.arrayIndex, unit);
    gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, binding.minFilter);
    gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, binding.magFilter);
    gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, binding.wrapModeX);
    gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, binding.wrapModeY);
    gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, binding.wrapModeZ);
    return true;
}

bool GLProgram::apply(const GLImageBinding &binding, int unit)
{
    auto location = getUniformLocation(binding.name);
    if (location < 0)
        return false;

    mUniformsSet[binding.name] = true;

    auto& gl = GLContext::currentContext();
    if (!gl.v4_2)
        return false;
    if (!binding.texture)
        return false;

    auto &texture = *binding.texture;
    const auto target = texture.target();
    const auto textureId = texture.getReadWriteTextureId();
    gl.v4_2->glActiveTexture(GL_TEXTURE0 + unit);
    gl.v4_2->glBindTexture(target, textureId);
    gl.v4_2->glUniform1i(location + binding.arrayIndex, unit);
    gl.v4_2->glBindImageTexture(unit, textureId, binding.level,
        binding.layered, binding.layer, binding.access, texture.format());
    return true;
}

bool GLProgram::apply(const GLBufferBinding &binding, int unit)
{
    auto& gl = GLContext::currentContext();
    auto index = gl.glGetUniformBlockIndex(
        mProgramObject, qPrintable(binding.name));

    if (index != GL_INVALID_INDEX) {
        mUniformBlocksSet[binding.name] = true;
        auto &buffer = *binding.buffer;
        gl.glUniformBlockBinding(mProgramObject, index, unit);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, unit,
            buffer.getReadOnlyBufferId());
        return true;
    }

    if (gl.v4_3) {
        index = gl.v4_3->glGetProgramResourceIndex(mProgramObject,
            GL_SHADER_STORAGE_BLOCK, qPrintable(binding.name));
        if (index != GL_INVALID_INDEX) {
            mUniformBlocksSet[binding.name] = true;
            auto &buffer = *binding.buffer;
            gl.v4_3->glShaderStorageBlockBinding(mProgramObject, index, unit);
            gl.v4_3->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, unit,
                buffer.getReadWriteBufferId());
            return true;
        }
    }
    return false;
}
