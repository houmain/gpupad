#include <array>
#include "GLProgram.h"
#include "GLTexture.h"
#include "GLBuffer.h"
#include "scripting/ScriptEngine.h"

GLProgram::GLProgram(const Program &program)
    : mItemId(program.id)
{
    mUsedItems += program.id;

    auto shaders = QList<const Shader*>();
    for (const auto &item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;

            shaders.append(shader);

            // headers are prepended to the next shader
            if (shader->shaderType != Shader::ShaderType::Header) {
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
    if (mFailed)
        return false;

    auto freeProgram = [](GLuint program) {
        auto &gl = GLContext::currentContext();
        gl.glDeleteProgram(program);
    };

    auto &gl = GLContext::currentContext();
    auto program = GLObject(gl.glCreateProgram(), freeProgram);

    for (auto &shader : mShaders) {
        if (shader.compile())
            gl.glAttachShader(program, shader.shaderObject());
        else
            mFailed = true;
    }
    if (mFailed)
        return false;

    auto status = GLint{ };
    gl.glLinkProgram(program);
    gl.glGetProgramiv(program, GL_LINK_STATUS, &status);

    auto length = GLint{ };
    gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    auto log = std::vector<char>(static_cast<size_t>(length));
    gl.glGetProgramInfoLog(program, length, nullptr, log.data());
    GLShader::parseLog(log.data(), mLinkMessages, mItemId, { });

    if (status != GL_TRUE) {
        mFailed = true;
        return false;
    }

    auto buffer = std::array<char, 256>();
    auto size = GLint{ };
    auto type = GLenum{ };
    auto nameLength = GLint{ };
    auto uniforms = GLint{ };
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
    for (auto i = 0; i < uniforms; ++i) {
        gl.glGetActiveUniform(program, static_cast<GLuint>(i), buffer.size(),
            &nameLength, &size, &type, buffer.data());
        const auto base = getUniformBaseName(buffer.data());
        mUniformDataTypes[base] = type;
        for (auto i = 0; i < size; i++)
            mUniformsSet[QStringLiteral("%1[%2]").arg(base).arg(i)] = false;
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
        if (!name.startsWith("gl_"))
            mAttributesSet[name] = false;
    }

    if (auto gl40 = gl.v4_0) {
        auto stages = QSet<Shader::ShaderType>();
        for (const auto &shader : mShaders)
            if (shader.type())
                stages += shader.type();

        auto subroutineIndices = std::vector<GLint>();
        for (const auto &stage : qAsConst(stages)) {
            gl40->glGetProgramStageiv(program, stage,
                GL_ACTIVE_SUBROUTINE_UNIFORMS, &uniforms);
            for (auto i = 0u; i < static_cast<GLuint>(uniforms); ++i) {
                gl40->glGetActiveSubroutineUniformName(program, stage, i,
                    buffer.size(), &nameLength, buffer.data());
                auto name = QString(buffer.data());

                auto compatible = GLint{ };
                gl40->glGetActiveSubroutineUniformiv(program, stage, i,
                    GL_NUM_COMPATIBLE_SUBROUTINES, &compatible);

                subroutineIndices.resize(static_cast<size_t>(compatible));
                gl40->glGetActiveSubroutineUniformiv(program, stage, i,
                    GL_COMPATIBLE_SUBROUTINES, subroutineIndices.data());

                auto subroutines = QList<QString>();
                for (auto index : subroutineIndices) {
                    gl40->glGetActiveSubroutineName(program, stage,
                        static_cast<GLuint>(index),
                        buffer.size(), &nameLength, buffer.data());
                    subroutines += QString(buffer.data());
                }
                mSubroutineUniforms[stage] +=
                    SubroutineUniform{ name, subroutines, 0, "" };
                mUniformsSet[name] = false;
            }
        }
    }
    mProgramObject = std::move(program);
    return true;
}

bool GLProgram::bind(MessagePtrSet *callMessages)
{
    if (!link() || !callMessages)
        return false;

    auto &gl = GLContext::currentContext();
    gl.glUseProgram(mProgramObject);
    mCallMessages = callMessages;
    return true;
}

void GLProgram::unbind(ItemId callItemId)
{
    auto &gl = GLContext::currentContext();
    gl.glUseProgram(GL_NONE);

    // inform about not set uniforms
    for (auto &kv : mUniformsSet) {
        if (!kv.second)
            *mCallMessages += MessageList::insert(
                callItemId, MessageType::UnformNotSet, kv.first);
        kv.second = false;
    }
    for (auto &kv : mUniformBlocksSet) {
        if (!kv.second)
            *mCallMessages += MessageList::insert(
                callItemId, MessageType::BlockNotSet, kv.first);
        kv.second = false;
    }
    for (auto &kv : mAttributesSet) {
        if (!kv.second)
            *mCallMessages += MessageList::insert(
                callItemId, MessageType::AttributeNotSet, kv.first);
        kv.second = false;
    }
    mCallMessages = nullptr;
}

QString GLProgram::getUniformBaseName(const QString &name) const
{
    const auto base = QString(name).remove(QRegExp("\\[.*\\]"));
    return base;
}

void GLProgram::uniformSet(const QString &name)
{
    auto it = mUniformsSet.find(name);
    if (it == mUniformsSet.end())
        it = mUniformsSet.find(name + "[0]");
    if (it != mUniformsSet.end())
        it->second = true;
}

void GLProgram::uniformBlockSet(const QString &name)
{
    auto it = mUniformBlocksSet.find(name);
    if (it != mUniformBlocksSet.end())
        it->second = true;
}

int GLProgram::getAttributeLocation(const QString &name) const
{
    if (mAttributesSet.count(name))
        mAttributesSet[name] = true;
    auto &gl = GLContext::currentContext();
    return gl.glGetAttribLocation(mProgramObject, qPrintable(name));
}

template<typename T> T toValue(const QString &);
template<> GLfloat toValue(const QString &string) { return string.toFloat(); }
template<> GLdouble toValue(const QString &string) { return string.toDouble(); }
template<> GLint toValue(const QString &string) { return string.toInt(); }
template<> GLuint toValue(const QString &string) { return string.toUInt(); }

bool GLProgram::apply(const GLUniformBinding &binding, ScriptEngine &scriptEngine)
{
    auto &gl = GLContext::currentContext();
    const auto location = gl.glGetUniformLocation(
        mProgramObject, qPrintable(binding.name));
    const auto values = scriptEngine.evaluateValues(
        binding.values, binding.bindingItemId, *mCallMessages);
    if (location < 0 || values.empty())
        return false;
    uniformSet(binding.name);

    const auto dataType = mUniformDataTypes[getUniformBaseName(binding.name)];

    auto getValues = [&](auto t, auto count) {
        using T = decltype(t);
        auto array = std::array<T, 16>{ };
        if (values.count() == count) {
            auto i = 0u;
            foreach (const QString &value, values)
                 array[i++] = toValue<T>(value);
        }
        else {
            *mCallMessages += MessageList::insert(binding.bindingItemId,
                MessageType::UniformComponentMismatch,
                QString("(%1/%2)").arg(values.count()).arg(count));
        }
        return array;
    };

#define ADD(TYPE, DATATYPE, COUNT, FUNCTION) \
        case TYPE: \
            FUNCTION(location, 1, getValues(DATATYPE(), COUNT).data()); \
            break;

#define ADD_MATRIX(TYPE, DATATYPE, COUNT, FUNCTION) \
        case TYPE: \
            FUNCTION(location, 1, binding.transpose, getValues(DATATYPE(), COUNT).data()); \
            break;

    switch (dataType) {
        ADD(GL_FLOAT, GLfloat, 1, gl.glUniform1fv);
        ADD(GL_FLOAT_VEC2, GLfloat, 2, gl.glUniform2fv);
        ADD(GL_FLOAT_VEC3, GLfloat, 3, gl.glUniform3fv);
        ADD(GL_FLOAT_VEC4, GLfloat, 4, gl.glUniform4fv);
        ADD(GL_DOUBLE, GLdouble, 1, gl.v4_0->glUniform1dv);
        ADD(GL_DOUBLE_VEC2, GLdouble, 2, gl.v4_0->glUniform2dv);
        ADD(GL_DOUBLE_VEC3, GLdouble, 3, gl.v4_0->glUniform3dv);
        ADD(GL_DOUBLE_VEC4, GLdouble, 4, gl.v4_0->glUniform4dv);
        ADD(GL_INT, GLint, 1, gl.glUniform1iv);
        ADD(GL_INT_VEC2, GLint, 2, gl.glUniform2iv);
        ADD(GL_INT_VEC3, GLint, 3, gl.glUniform3iv);
        ADD(GL_INT_VEC4, GLint, 4, gl.glUniform4iv);
        ADD(GL_UNSIGNED_INT, GLuint, 1, gl.glUniform1uiv);
        ADD(GL_UNSIGNED_INT_VEC2, GLuint, 2, gl.glUniform2uiv);
        ADD(GL_UNSIGNED_INT_VEC3, GLuint, 3, gl.glUniform3uiv);
        ADD(GL_UNSIGNED_INT_VEC4, GLuint, 4, gl.glUniform4uiv);
        ADD(GL_BOOL, GLint, 1, gl.glUniform1iv);
        ADD(GL_BOOL_VEC2, GLint, 2, gl.glUniform2iv);
        ADD(GL_BOOL_VEC3, GLint, 3, gl.glUniform3iv);
        ADD(GL_BOOL_VEC4, GLint, 4, gl.glUniform4iv);
        ADD_MATRIX(GL_FLOAT_MAT2, GLfloat, 4, gl.glUniformMatrix2fv);
        ADD_MATRIX(GL_FLOAT_MAT3, GLfloat, 9, gl.glUniformMatrix3fv);
        ADD_MATRIX(GL_FLOAT_MAT4, GLfloat, 16, gl.glUniformMatrix4fv);
        ADD_MATRIX(GL_FLOAT_MAT2x3, GLfloat, 6, gl.glUniformMatrix2x3fv);
        ADD_MATRIX(GL_FLOAT_MAT3x2, GLfloat, 6, gl.glUniformMatrix3x2fv);
        ADD_MATRIX(GL_FLOAT_MAT2x4, GLfloat, 8, gl.glUniformMatrix2x4fv);
        ADD_MATRIX(GL_FLOAT_MAT4x2, GLfloat, 8, gl.glUniformMatrix4x2fv);
        ADD_MATRIX(GL_FLOAT_MAT3x4, GLfloat, 12, gl.glUniformMatrix3x4fv);
        ADD_MATRIX(GL_FLOAT_MAT4x3, GLfloat, 12, gl.glUniformMatrix4x3fv);
        ADD_MATRIX(GL_DOUBLE_MAT2, GLdouble, 4, gl.v4_0->glUniformMatrix2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3, GLdouble, 9, gl.v4_0->glUniformMatrix3dv);
        ADD_MATRIX(GL_DOUBLE_MAT4, GLdouble, 16, gl.v4_0->glUniformMatrix4dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x3, GLdouble, 6, gl.v4_0->glUniformMatrix2x3dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x2, GLdouble, 6, gl.v4_0->glUniformMatrix3x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x4, GLdouble, 8, gl.v4_0->glUniformMatrix2x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x2, GLdouble, 8, gl.v4_0->glUniformMatrix4x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x4, GLdouble, 12, gl.v4_0->glUniformMatrix3x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x3, GLdouble, 12, gl.v4_0->glUniformMatrix4x3dv);
    }
#undef ADD
#undef ADD_MATRIX
    return true;
}

bool GLProgram::apply(const GLSamplerBinding &binding, int unit)
{
    auto &gl = GLContext::currentContext();
    const auto location = gl.glGetUniformLocation(
        mProgramObject, qPrintable(binding.name));
    if (location < 0)
        return false;
    if (!binding.texture)
        return false;
    uniformSet(binding.name);

    float borderColor[] = {
        static_cast<float>(binding.borderColor.redF()),
        static_cast<float>(binding.borderColor.greenF()),
        static_cast<float>(binding.borderColor.blueF()),
        static_cast<float>(binding.borderColor.alphaF())
    };

    auto &texture = *binding.texture;
    const auto target = texture.target();
    gl.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    gl.glBindTexture(target, texture.getReadOnlyTextureId());
    gl.glUniform1i(location, unit);

    switch (target) {
        case QOpenGLTexture::Target1D:
        case QOpenGLTexture::Target1DArray:
        case QOpenGLTexture::Target2D:
        case QOpenGLTexture::Target2DArray:
        case QOpenGLTexture::Target3D:
        case QOpenGLTexture::TargetCubeMap:
        case QOpenGLTexture::TargetCubeMapArray:
        case QOpenGLTexture::TargetRectangle:
            gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, binding.minFilter);
            gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, binding.magFilter);
            gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, binding.wrapModeX);
            gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, binding.wrapModeY);
            gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, binding.wrapModeZ);
            gl.glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
            if (binding.comparisonFunc) {
                gl.glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                gl.glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, binding.comparisonFunc);
            }
            else {
                gl.glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            }
            break;

        default:
            break;
    }
    gl.glActiveTexture(GL_TEXTURE0);
    return true;
}

bool GLProgram::apply(const GLImageBinding &binding, int unit)
{
    auto &gl = GLContext::currentContext();
    const auto location = gl.glGetUniformLocation(
        mProgramObject, qPrintable(binding.name));
    if (location < 0)
        return false;
    uniformSet(binding.name);

    if (!gl.v4_2)
        return false;
    if (!binding.texture)
        return false;

    auto &texture = *binding.texture;
    const auto target = texture.target();
    const auto textureId = texture.getReadWriteTextureId();

    auto formatSupported = GLint();
    gl.v4_2->glGetInternalformativ(target, texture.format(),
        GL_SHADER_IMAGE_LOAD, 1, &formatSupported);
    if (formatSupported == GL_NONE) {
        *mCallMessages += MessageList::insert(
            binding.bindingItemId, MessageType::ImageFormatNotBindable);
    }
    else {
        gl.v4_2->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        gl.v4_2->glBindTexture(target, textureId);
        gl.v4_2->glUniform1i(location, unit);
        gl.v4_2->glBindImageTexture(static_cast<GLuint>(unit),
            textureId, binding.level, binding.layered,
            binding.layer, binding.access, texture.format());
        gl.glActiveTexture(GL_TEXTURE0);
    }
    return true;
}

bool GLProgram::apply(const GLBufferBinding &binding, int unit)
{
    if (!binding.buffer)
        return false;

    auto &gl = GLContext::currentContext();
    auto index = gl.glGetUniformBlockIndex(
        mProgramObject, qPrintable(binding.name));

    if (index != GL_INVALID_INDEX) {
        uniformBlockSet(binding.name);
        auto &buffer = *binding.buffer;
        gl.glUniformBlockBinding(mProgramObject, index,
            static_cast<GLuint>(unit));
        gl.glBindBufferBase(GL_UNIFORM_BUFFER,
            static_cast<GLuint>(unit), buffer.getReadOnlyBufferId());
        return true;
    }

    if (gl.v4_3) {
        index = gl.v4_3->glGetProgramResourceIndex(mProgramObject,
            GL_SHADER_STORAGE_BLOCK, qPrintable(binding.name));
        if (index != GL_INVALID_INDEX) {
            uniformBlockSet(binding.name);
            auto &buffer = *binding.buffer;
            gl.v4_3->glShaderStorageBlockBinding(mProgramObject,
                index, static_cast<GLuint>(unit));
            gl.v4_3->glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                static_cast<GLuint>(unit), buffer.getReadWriteBufferId());
            return true;
        }
    }
    return false;
}

bool GLProgram::apply(const GLSubroutineBinding &binding)
{
    auto bound = false;
    foreach (Shader::ShaderType stage, mSubroutineUniforms.keys())
        if (!binding.type || binding.type == stage)
            for (auto &uniform : mSubroutineUniforms[stage])
                if (uniform.name == binding.name) {
                    uniform.bindingItemId = binding.bindingItemId;
                    uniform.boundSubroutine = binding.subroutine;
                    uniformSet(binding.name);
                    bound = true;
                }
    return bound;
}

void GLProgram::reapplySubroutines()
{
    auto &gl = GLContext::currentContext();
    if (!gl.v4_0)
        return;

    auto subroutineIndices = std::vector<GLuint>();
    foreach (Shader::ShaderType stage, mSubroutineUniforms.keys()) {
        for (const auto &uniform : qAsConst(mSubroutineUniforms[stage])) {
            auto index = gl.v4_0->glGetSubroutineIndex(mProgramObject,
                stage, qPrintable(uniform.boundSubroutine));
            subroutineIndices.push_back(index);
            if (index == GL_INVALID_INDEX && !uniform.boundSubroutine.isEmpty())
                *mCallMessages +=
                    MessageList::insert(uniform.bindingItemId,
                    MessageType::InvalidSubroutine, uniform.boundSubroutine);
        }
        gl.v4_0->glUniformSubroutinesuiv(stage,
            static_cast<GLsizei>(subroutineIndices.size()),
            subroutineIndices.data());
        subroutineIndices.clear();
    }
}
