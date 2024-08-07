#include "GLProgram.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>
#include <array>

GLProgram::GLProgram(const Program &program, const QString &shaderPreamble,
    const QString &shaderIncludePaths)
    : mItemId(program.id)
{
    mUsedItems += program.id;

    auto shaders = std::map<Shader::ShaderType, QList<const Shader *>>();
    for (const auto &item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            mUsedItems += shader->id;
            shaders[shader->shaderType].append(shader);
        }

    for (const auto &[type, list] : shaders)
        if (type != Shader::ShaderType::Includable)
            mShaders.emplace_back(type, list, shaderPreamble,
                shaderIncludePaths);
}

bool GLProgram::operator==(const GLProgram &rhs) const
{
    return (std::tie(mShaders) == std::tie(rhs.mShaders));
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
    if (!linkShaders(program)) {
        mFailed = true;
        return false;
    }
    mProgramObject = std::move(program);
    return true;
}

bool GLProgram::linkShaders(GLuint program)
{
    auto &gl = GLContext::currentContext();
    for (auto &shader : mShaders) {
        if (!shader.compile(&mPrintf))
            return false;
        gl.glAttachShader(program, shader.shaderObject());
    }

    if (!linkProgram(program))
        return false;

    auto buffer = std::array<char, 256>();
    auto size = GLint{};
    auto type = GLenum{};
    auto nameLength = GLint{};
    auto uniforms = GLint{};
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
    for (auto i = 0; i < uniforms; ++i) {
        gl.glGetActiveUniform(program, static_cast<GLuint>(i),
            static_cast<GLsizei>(buffer.size()), &nameLength, &size, &type,
            buffer.data());
        const auto name = QString(buffer.data());
        const auto baseName = getUniformBaseName(name);
        mActiveUniforms[baseName] = {
            gl.glGetUniformLocation(program, qPrintable(baseName)), type, size
        };
        mUniformsSet[baseName] = false;
        for (auto j = 0; j < size; ++j) {
            const auto elementName =
                QStringLiteral("%1[%2]").arg(baseName).arg(j);
            if (auto location =
                    gl.glGetUniformLocation(program, qPrintable(elementName));
                location >= 0)
                mActiveUniforms[elementName] = { location, type, 1 };
        }

#if GL_VERSION_4_2
        if (auto gl42 = gl.v4_2) {
            auto atomicCounterIndex = 0;
            auto indices = static_cast<GLuint>(i);
            gl.glGetActiveUniformsiv(program, 1, &indices,
                GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX, &atomicCounterIndex);
            if (atomicCounterIndex >= 0) {
                auto bindingPoint =
                    std::pair<GLenum, GLint>{ GL_ATOMIC_COUNTER_BUFFER, 0 };
                gl42->glGetActiveAtomicCounterBufferiv(program,
                    atomicCounterIndex, GL_ATOMIC_COUNTER_BUFFER_BINDING,
                    &bindingPoint.second);
                if (!mBufferBindingPoints.values().contains(bindingPoint)) {
                    mBufferBindingPoints[name] = bindingPoint;
                    mBuffersSet[name] = false;
                }
                mUniformsSet.erase(baseName);
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
        mBufferBindingPoints[name] = { GL_UNIFORM_BUFFER, uniformBlockBinding };
        mBuffersSet[name] = false;

        // remove block's uniforms from list of uniforms to set
        gl.glGetActiveUniformBlockiv(program, i,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniforms);
        auto uniformIndices = std::vector<GLint>(static_cast<size_t>(uniforms));
        gl.glGetActiveUniformBlockiv(program, i,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, uniformIndices.data());
        for (auto index : uniformIndices) {
            gl.glGetActiveUniform(program, static_cast<GLuint>(index),
                static_cast<GLsizei>(buffer.size()), &nameLength, &size, &type,
                buffer.data());
            const auto name = QString(buffer.data());
            mUniformsSet.erase(getUniformBaseName(name));
        }
    }

    auto attributes = GLint{};
    gl.glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributes);
    for (auto i = 0; i < attributes; ++i) {
        gl.glGetActiveAttrib(program, static_cast<GLuint>(i),
            static_cast<GLsizei>(buffer.size()), &nameLength, &size, &type,
            buffer.data());
        const auto name = QString(buffer.data());
        mAttributeLocations[name] =
            gl.glGetAttribLocation(program, qPrintable(name));
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
                mSubroutineUniforms[stage] +=
                    SubroutineUniform{ name, subroutines, 0, "" };
                mUniformsSet[name] = false;
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
            mBufferBindingPoints[name] = { GL_SHADER_STORAGE_BUFFER,
                storageBlockBinding };
            mBuffersSet[name] = false;
        }
    }
#endif
    return true;
}

bool GLProgram::linkProgram(GLuint program)
{
    auto &gl = GLContext::currentContext();
    gl.glLinkProgram(program);
    auto status = GLint{};
    auto length = GLint{};
    gl.glGetProgramiv(program, GL_LINK_STATUS, &status);
    gl.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    auto log = std::vector<char>(static_cast<size_t>(length));
    gl.glGetProgramInfoLog(program, length, nullptr, log.data());
    GLShader::parseLog(log.data(), mLinkMessages, mItemId, {});
    return (status == GL_TRUE);
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

    // warn about not set uniforms
    for (auto &kv : mUniformsSet)
        if (!std::exchange(kv.second, false))
            *mCallMessages += MessageList::insert(callItemId,
                MessageType::UniformNotSet, kv.first);

    for (auto &kv : mBuffersSet)
        if (!std::exchange(kv.second, false))
            *mCallMessages += MessageList::insert(callItemId,
                MessageType::BufferNotSet, kv.first);

    for (auto &kv : mAttributesSet)
        if (!std::exchange(kv.second, false))
            *mCallMessages += MessageList::insert(callItemId,
                MessageType::AttributeNotSet, kv.first);

    if (mPrintf.isUsed())
        mPrintfMessages = mPrintf.formatMessages(mItemId);

    mCallMessages = nullptr;
}

QString GLProgram::getUniformBaseName(const QString &name) const
{
    static const auto regex = QRegularExpression("\\[.*\\]");
    return QString(name).remove(regex);
}

void GLProgram::uniformSet(const QString &name)
{
    auto it = mUniformsSet.find(name);
    if (it == mUniformsSet.end())
        it = mUniformsSet.find(name + "[0]");
    if (it != mUniformsSet.end())
        it->second = true;
}

void GLProgram::bufferSet(const QString &name)
{
    auto it = mBuffersSet.find(name);
    if (it != mBuffersSet.end())
        it->second = true;
}

int GLProgram::getAttributeLocation(const QString &name) const
{
    auto it = mAttributeLocations.find(name);
    if (it == mAttributeLocations.end())
        return -1;
    mAttributesSet[name] = true;
    return it.value();
}

template <typename T>
std::vector<T> getValues(ScriptEngine &scriptEngine,
    const QStringList &expressions, ItemId itemId, int count,
    MessagePtrSet &messages)
{
    const auto values =
        scriptEngine.evaluateValues(expressions, itemId, messages);
    if (count != values.count())
        if (count != 3 || values.count() != 4)
            messages += MessageList::insert(itemId,
                MessageType::UniformComponentMismatch,
                QString("(%1/%2)").arg(values.count()).arg(count));

    auto results = std::vector<T>();
    results.reserve(count);
    for (const auto &value : values)
        results.push_back(static_cast<T>(value));
    return results;
}

bool GLProgram::apply(const GLUniformBinding &binding,
    ScriptEngine &scriptEngine)
{
    auto &gl = GLContext::currentContext();
    const auto baseName = getUniformBaseName(binding.name);
    const auto [location, dataType, size] =
        mActiveUniforms[mActiveUniforms.contains(binding.name) ? binding.name
                                                               : baseName];
    uniformSet(baseName);
    if (!dataType || location < 0)
        return false;

    switch (dataType) {
#define ADD(TYPE, DATATYPE, COUNT, FUNCTION)                         \
    case TYPE:                                                       \
        FUNCTION(location, size,                                     \
            getValues<DATATYPE>(scriptEngine, binding.values,        \
                binding.bindingItemId, COUNT * size, *mCallMessages) \
                .data());                                            \
        break

#define ADD_MATRIX(TYPE, DATATYPE, COUNT, FUNCTION)                  \
    case TYPE:                                                       \
        FUNCTION(location, size, binding.transpose,                  \
            getValues<DATATYPE>(scriptEngine, binding.values,        \
                binding.bindingItemId, COUNT * size, *mCallMessages) \
                .data());                                            \
        break

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
        ADD_MATRIX(GL_DOUBLE_MAT2x3, GLdouble, 6,
            gl.v4_0->glUniformMatrix2x3dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x2, GLdouble, 6,
            gl.v4_0->glUniformMatrix3x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT2x4, GLdouble, 8,
            gl.v4_0->glUniformMatrix2x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x2, GLdouble, 8,
            gl.v4_0->glUniformMatrix4x2dv);
        ADD_MATRIX(GL_DOUBLE_MAT3x4, GLdouble, 12,
            gl.v4_0->glUniformMatrix3x4dv);
        ADD_MATRIX(GL_DOUBLE_MAT4x3, GLdouble, 12,
            gl.v4_0->glUniformMatrix4x3dv);
#undef ADD
#undef ADD_MATRIX
    }
    return true;
}

bool GLProgram::apply(const GLSamplerBinding &binding, int unit)
{
    auto &gl = GLContext::currentContext();
    if (!mActiveUniforms.contains(binding.name))
        return false;
    const auto [location, dataType, size] =
        mActiveUniforms[(mActiveUniforms.contains(binding.name)
                ? binding.name
                : getUniformBaseName(binding.name))];
    if (!binding.texture)
        return false;

    float borderColor[] = { static_cast<float>(binding.borderColor.redF()),
        static_cast<float>(binding.borderColor.greenF()),
        static_cast<float>(binding.borderColor.blueF()),
        static_cast<float>(binding.borderColor.alphaF()) };

    auto &texture = *binding.texture;
    const auto target = texture.target();
    gl.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    texture.updateMipmaps();
    gl.glBindTexture(target, texture.getReadOnlyTextureId());
    if (location >= 0)
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
        if (binding.minFilter != Binding::Filter::Nearest) {
            auto anisotropy = 1.0f;
            if (binding.anisotropic)
                gl.glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
            gl.glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                anisotropy);
        }
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, binding.wrapModeX);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, binding.wrapModeY);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, binding.wrapModeZ);
        gl.glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
        if (binding.comparisonFunc) {
            gl.glTexParameteri(target, GL_TEXTURE_COMPARE_MODE,
                GL_COMPARE_REF_TO_TEXTURE);
            gl.glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC,
                binding.comparisonFunc);
        } else {
            gl.glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
        break;

    default: break;
    }
    uniformSet(binding.name);
    return true;
}

bool GLProgram::apply(const GLImageBinding &binding, int unit)
{
    auto &gl = GLContext::currentContext();
    const auto location =
        gl.glGetUniformLocation(mProgramObject, qPrintable(binding.name));
    if (location < 0)
        return false;

    if (!gl.v4_2)
        return false;
    if (!binding.texture)
        return false;

#if GL_VERSION_4_2
    auto &texture = *binding.texture;
    const auto target = texture.target();
    const auto textureId = texture.getReadWriteTextureId();
    const auto format = (binding.format
            ? static_cast<GLenum>(binding.format)
            : static_cast<GLenum>(texture.format()));

    auto formatSupported = GLint();
    gl.v4_2->glGetInternalformativ(target, format, GL_SHADER_IMAGE_LOAD, 1,
        &formatSupported);
    if (formatSupported == GL_NONE) {
        *mCallMessages += MessageList::insert(binding.bindingItemId,
            MessageType::ImageFormatNotBindable);
    } else {
        gl.v4_2->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        gl.v4_2->glBindTexture(target, textureId);
        gl.v4_2->glUniform1i(location, unit);
        gl.v4_2->glBindImageTexture(static_cast<GLuint>(unit), textureId,
            binding.level, (binding.layer < 0), std::max(binding.layer, 0),
            binding.access, format);
    }
    uniformSet(binding.name);
#endif
    return true;
}

bool GLProgram::apply(const GLBufferBinding &binding,
    ScriptEngine &scriptEngine)
{
    if (!binding.buffer)
        return false;

    if (!mBufferBindingPoints.contains(binding.name))
        return false;

    const auto offset =
        scriptEngine.evaluateInt(binding.offset, mItemId, *mCallMessages);
    const auto rowCount =
        scriptEngine.evaluateInt(binding.rowCount, mItemId, *mCallMessages);

    const auto [target, index] = mBufferBindingPoints[binding.name];
    binding.buffer->bindIndexedRange(target, index, offset,
        rowCount * binding.stride, binding.readonly);
    bufferSet(binding.name);
    return true;
}

bool GLProgram::applyPrintfBindings()
{
    if (!mPrintf.isUsed())
        return false;

    mPrintf.clear();

    const auto name = mPrintf.bufferBindingName();
    if (!mBufferBindingPoints.contains(name))
        return false;

    auto &gl = GLContext::currentContext();
    const auto [target, index] = mBufferBindingPoints[name];
    gl.glBindBufferBase(target, index, mPrintf.bufferObject());
    bufferSet(name);
    return true;
}

bool GLProgram::apply(const GLSubroutineBinding &binding)
{
    auto bound = false;
    for (Shader::ShaderType stage : mSubroutineUniforms.keys())
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
    for (Shader::ShaderType stage : mSubroutineUniforms.keys()) {
        for (const auto &uniform : qAsConst(mSubroutineUniforms[stage])) {
            auto isIndex = false;
            auto index = uniform.boundSubroutine.toUInt(&isIndex);
            if (!isIndex)
                index = gl.v4_0->glGetSubroutineIndex(mProgramObject, stage,
                    qPrintable(uniform.boundSubroutine));
            subroutineIndices.push_back(index);
            if (index == GL_INVALID_INDEX && !uniform.boundSubroutine.isEmpty())
                *mCallMessages += MessageList::insert(uniform.bindingItemId,
                    MessageType::InvalidSubroutine, uniform.boundSubroutine);
        }
        gl.v4_0->glUniformSubroutinesuiv(stage,
            static_cast<GLsizei>(subroutineIndices.size()),
            subroutineIndices.data());
        subroutineIndices.clear();
    }
}

bool GLProgram::allBuffersBound() const
{
    for (const auto &[buffer, set] : mBuffersSet)
        if (!set)
            return false;
    return true;
}
