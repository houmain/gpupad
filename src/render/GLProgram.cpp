#include <array>
#include "GLProgram.h"

void GLProgram::initialize(PrepareContext &context, const Program &program)
{
    mItemId = program.id;
    context.usedItems += mItemId;

    auto header = QString();
    for (const auto& item : program.items)
        if (auto shader = castItem<Shader>(item)) {
            context.usedItems += item->id;

            mShaders.emplace_back(context, header, *shader);

            // if it is a header, remove from list an prepend to next shader
            if (shader->type == Shader::Header) {
                header = mShaders.back().source();
                mShaders.pop_back();
            }
            else {
                header.clear();
            }
        }
}

void GLProgram::addTextureBinding(PrepareContext &context,
    const Texture &texture, QString name, int arrayIndex)
{
    mSamplerBindings.push_back({
        0,
        (name.isEmpty() ? texture.name : name),
        arrayIndex,
        addOnce(mTextures, texture, context),
        QOpenGLTexture::Nearest, QOpenGLTexture::Nearest,
        QOpenGLTexture::Repeat, QOpenGLTexture::Repeat,
        QOpenGLTexture::Repeat });
}

void GLProgram::addSamplerBinding(PrepareContext &context,
    const Sampler &sampler, QString name, int arrayIndex)
{
    if (auto texture = context.session.findItem<Texture>(sampler.textureId)) {
        mSamplerBindings.push_back({
            sampler.id,
            (name.isEmpty() ? sampler.name : name),
            arrayIndex,
            addOnce(mTextures, *texture, context),
            sampler.minFilter, sampler.magFilter,
            sampler.wrapModeX, sampler.wrapModeY,
            sampler.wrapModeZ });
    }
}

void GLProgram::addBufferBinding(PrepareContext &context,
    const Buffer &buffer,QString name, int arrayIndex)
{
    mBufferBindings.push_back({
        (name.isEmpty() ? buffer.name : name),
        arrayIndex,
        addOnce(mBuffers, buffer, context) });
}

void GLProgram::addBinding(PrepareContext &context, const Binding &binding)
{
    if (binding.type == Binding::Sampler) {
        context.usedItems += binding.id;
        for (auto i = 0; i < binding.valueCount(); ++i) {
            auto samplerId = binding.getField(i, 0).toInt();
            if (auto sampler = context.session.findItem<Sampler>(samplerId))
                addSamplerBinding(context, *sampler, binding.name, i);
        }
    }
    else if (binding.type == Binding::Texture) {
        context.usedItems += binding.id;
        for (auto i = 0; i < binding.valueCount(); ++i) {
            auto textureId = binding.getField(i, 0).toInt();
            if (auto texture = context.session.findItem<Texture>(textureId))
                addTextureBinding(context, *texture, binding.name, i);
        }
    }
    else if (binding.type == Binding::Image) {
        context.usedItems += binding.id;
        for (auto i = 0; i < binding.valueCount(); ++i) {
            auto textureId = binding.getField(i, 0).toInt();
            if (auto texture = context.session.findItem<Texture>(textureId)) {
                auto layer = binding.getField(i, 1).toInt();
                auto level = 0;
                auto layered = false;
                auto access = GLenum{ GL_READ_WRITE };
                mImageBindings.push_back({ binding.name, i,
                    addOnce(mTextures, *texture, context),
                    level, layered, layer, access });
            }
        }
    }
    else if (binding.type == Binding::Buffer) {
        context.usedItems += binding.id;
        for (auto i = 0; i < binding.valueCount(); ++i) {
            auto bufferId = binding.getField(i, 0).toInt();
            if (auto buffer = context.session.findItem<Buffer>(bufferId))
                addBufferBinding(context, *buffer, binding.name, i);
        }
    }
    else {
        mUniforms.push_back({ binding.id, binding.name, binding.type, binding.values });
    }
}

void GLProgram::addScript(PrepareContext &context, const Script &script)
{
    context.usedItems += script.id;
    context.messages.setContext(script.fileName);

    auto source = QString();
    if (!Singletons::fileCache().getSource(script.fileName, &source)) {
        context.messages.insert(MessageType::LoadingFileFailed, script.fileName);
    }
    else {
        mScriptEngine.evalScript(script.fileName, source);
    }
}

void GLProgram::cache(RenderContext &context, GLProgram &&update)
{
    mItemId = update.mItemId;

    mUniforms = std::move(update.mUniforms);
    mSamplerBindings = std::move(update.mSamplerBindings);
    mImageBindings = std::move(update.mImageBindings);
    mBufferBindings = std::move(update.mBufferBindings);

    if (mScriptEngine != update.mScriptEngine)
        mScriptEngine = std::move(update.mScriptEngine);

    if (updateList(mShaders, std::move(update.mShaders)))
        mProgramObject.reset();

    link(context);

    updateList(mTextures, std::move(update.mTextures));
    updateList(mBuffers, std::move(update.mBuffers));
}

bool GLProgram::link(RenderContext &context)
{
    if (mProgramObject)
        return true;

    auto freeProgram = [](GLuint program) {
        auto& gl = *QOpenGLContext::currentContext()->functions();
        gl.glDeleteProgram(program);
    };

    auto program = GLObject(context.glCreateProgram(), freeProgram);
    for (auto& shader : mShaders) {
        if (!shader.compile(context))
            return false;
        context.glAttachShader(program, shader.shaderObject());
    }

    context.glLinkProgram(program);

    auto status = GLint{ };
    context.glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        auto length = GLint{ };
        context.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        auto log = std::vector<char>(static_cast<size_t>(length));
        context.glGetProgramInfoLog(program, length, NULL, log.data());
        context.messages.setContext(mItemId);
        GLShader::parseLog(log.data(), context.messages);
        return false;
    }

    auto uniforms = GLint{ };
    context.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
    for (auto i = 0; i < uniforms; ++i) {
        auto name = std::array<char, 256>();
        auto size = GLint{ };
        auto type = GLenum{ };
        auto nameLength = GLint{ };
        context.glGetActiveUniform(program, static_cast<GLuint>(i), name.size(),
            &nameLength, &size, &type, name.data());
        mUniformDataTypes[QString(name.data())
            .remove(QRegExp("\\[0\\]"))] = type;
    }

    mProgramObject = std::move(program);
    return true;
}

int GLProgram::getUniformLocation(RenderContext &context, const QString &name) const
{
    return context.glGetUniformLocation(mProgramObject, qPrintable(name));
}

int GLProgram::getAttributeLocation(RenderContext &context, const QString &name) const
{
    return context.glGetAttribLocation(mProgramObject, qPrintable(name));
}

GLenum GLProgram::getUniformDataType(const QString &uniformName) const
{
    return mUniformDataTypes[uniformName];
}

bool GLProgram::bind(RenderContext &context)
{
    if (!mProgramObject)
        return false;

    context.glUseProgram(mProgramObject);

    auto unit = 0;
    for (const auto& binding : mSamplerBindings)
        applySamplerBinding(context, binding, unit++);
    for (const auto& binding : mImageBindings)
        applyImageBinding(context, binding, unit++);

    unit = 0;
    for (const auto& binding : mBufferBindings)
        applyBufferBinding(context, binding, unit++);

    for (const auto& uniform : mUniforms)
        applyUniform(context, uniform);

    return true;
}

void GLProgram::unbind(RenderContext &context)
{
    context.glUseProgram(GL_NONE);
}

QList<std::pair<QString, QImage>> GLProgram::getModifiedImages(
    RenderContext &context)
{
    auto result = QList<std::pair<QString, QImage>>();
    for (auto& binding : mImageBindings)
        result += mTextures[binding.textureIndex].getModifiedImages(context);
    return result;
}

QList<std::pair<QString, QByteArray>> GLProgram::getModifiedBuffers(
    RenderContext &context)
{
    auto result = QList<std::pair<QString, QByteArray>>();
    for (auto& binding : mBufferBindings)
        result += mBuffers[binding.bufferIndex].getModifiedData(context);
    return result;
}

void GLProgram::applySamplerBinding(RenderContext &context,
    const GLSamplerBinding &binding, int unit)
{
    auto location = getUniformLocation(context, binding.name);
    if (location < 0)
        return;

    auto& texture = mTextures[binding.textureIndex];
    const auto target = texture.target();
    context.glActiveTexture(GL_TEXTURE0 + unit);
    context.glBindTexture(target, texture.getReadOnlyTextureId(context));
    context.glUniform1i(location + binding.arrayIndex, unit);
    context.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, binding.minFilter);
    context.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, binding.magFilter);
    context.glTexParameteri(target, GL_TEXTURE_WRAP_S, binding.wrapModeX);
    context.glTexParameteri(target, GL_TEXTURE_WRAP_T, binding.wrapModeY);
    context.glTexParameteri(target, GL_TEXTURE_WRAP_R, binding.wrapModeZ);

    if (binding.itemId)
        context.usedItems += binding.itemId;
    context.usedItems += texture.itemId();
}

void GLProgram::applyImageBinding(RenderContext &context,
    const GLImageBinding &binding, int unit)
{
    if (!context.gl42)
        return;

    auto location = getUniformLocation(context, binding.name);
    if (location < 0)
        return;

    auto& texture = mTextures[binding.textureIndex];
    const auto target = texture.target();
    const auto textureId = texture.getReadWriteTextureId(context);
    context.gl42->glActiveTexture(GL_TEXTURE0 + unit);
    context.gl42->glBindTexture(target, textureId);
    context.gl42->glUniform1i(location + binding.arrayIndex, unit);
    context.gl42->glBindImageTexture(unit, textureId, binding.level,
        binding.layered, binding.layer, binding.access, texture.format());

    context.usedItems += texture.itemId();
}

void GLProgram::applyBufferBinding(RenderContext &context,
    const GLBufferBinding &binding, int unit)
{
    auto index = context.glGetUniformBlockIndex(
        mProgramObject, qPrintable(binding.name));
    if (index != GL_INVALID_INDEX) {
        auto& buffer = mBuffers[binding.bufferIndex];
        context.glUniformBlockBinding(mProgramObject, index, unit);
        context.glBindBufferBase(GL_UNIFORM_BUFFER, unit,
            buffer.getReadOnlyBufferId(context));

        context.usedItems += buffer.itemId();
        return;
    }

    if (context.gl43) {
        index = context.gl43->glGetProgramResourceIndex(mProgramObject,
            GL_SHADER_STORAGE_BLOCK, qPrintable(binding.name));
        if (index != GL_INVALID_INDEX) {
            auto& buffer = mBuffers[binding.bufferIndex];
            context.gl43->glShaderStorageBlockBinding(mProgramObject, index, unit);
            context.gl43->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, unit,
                buffer.getReadWriteBufferId(context));

            context.usedItems += buffer.itemId();
            return;
        }
    }
}

void GLProgram::applyUniform(RenderContext &context, const GLUniform &uniform)
{
    const auto transpose = false;
    const auto dataType = getUniformDataType(uniform.name);

    for (auto i = 0; i < uniform.values.size(); ++i) { 
        auto name = uniform.name;
        if (i > 0)
            name += QStringLiteral("[%1]").arg(i);
        const auto location = getUniformLocation(context, name);
        if (location < 0)
            return;

        context.usedItems += uniform.itemId;

        auto floats = std::array<GLfloat, 16>();
        auto ints = std::array<GLint, 16>();
        auto uints = std::array<GLuint, 16>();
        auto doubles = std::array<GLdouble, 16>();
        auto j = 0u;
        foreach (QString field, mScriptEngine.evalValue(
                uniform.values[i].toStringList(), context.messages)) {
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
            ADD(GL_FLOAT, floats, context.glUniform1fv);
            ADD(GL_FLOAT_VEC2, floats, context.glUniform2fv);
            ADD(GL_FLOAT_VEC3, floats, context.glUniform3fv);
            ADD(GL_FLOAT_VEC4, floats, context.glUniform4fv);
            ADD(GL_DOUBLE, doubles, context.gl40->glUniform1dv);
            ADD(GL_DOUBLE_VEC2, doubles, context.gl40->glUniform2dv);
            ADD(GL_DOUBLE_VEC3, doubles, context.gl40->glUniform3dv);
            ADD(GL_DOUBLE_VEC4, doubles, context.gl40->glUniform4dv);
            ADD(GL_INT, ints, context.glUniform1iv);
            ADD(GL_INT_VEC2, ints, context.glUniform2iv);
            ADD(GL_INT_VEC3, ints, context.glUniform3iv);
            ADD(GL_INT_VEC4, ints, context.glUniform4iv);
            ADD(GL_UNSIGNED_INT, uints, context.glUniform1uiv);
            ADD(GL_UNSIGNED_INT_VEC2, uints, context.glUniform2uiv);
            ADD(GL_UNSIGNED_INT_VEC3, uints, context.glUniform3uiv);
            ADD(GL_UNSIGNED_INT_VEC4, uints, context.glUniform4uiv);
            ADD(GL_BOOL, ints, context.glUniform1iv);
            ADD(GL_BOOL_VEC2, ints, context.glUniform2iv);
            ADD(GL_BOOL_VEC3, ints, context.glUniform3iv);
            ADD(GL_BOOL_VEC4, ints, context.glUniform4iv);
            ADD_MATRIX(GL_FLOAT_MAT2, floats, context.glUniformMatrix2fv);
            ADD_MATRIX(GL_FLOAT_MAT3, floats, context.glUniformMatrix3fv);
            ADD_MATRIX(GL_FLOAT_MAT4, floats, context.glUniformMatrix4fv);
            ADD_MATRIX(GL_FLOAT_MAT2x3, floats, context.glUniformMatrix2x3fv);
            ADD_MATRIX(GL_FLOAT_MAT2x4, floats, context.glUniformMatrix2x4fv);
            ADD_MATRIX(GL_FLOAT_MAT3x2, floats, context.glUniformMatrix3x2fv);
            ADD_MATRIX(GL_FLOAT_MAT3x4, floats, context.glUniformMatrix3x4fv);
            ADD_MATRIX(GL_FLOAT_MAT4x2, floats, context.glUniformMatrix4x2fv);
            ADD_MATRIX(GL_FLOAT_MAT4x3, floats, context.glUniformMatrix4x3fv);
            ADD_MATRIX(GL_DOUBLE_MAT2, doubles, context.gl40->glUniformMatrix2dv);
            ADD_MATRIX(GL_DOUBLE_MAT3, doubles, context.gl40->glUniformMatrix3dv);
            ADD_MATRIX(GL_DOUBLE_MAT4, doubles, context.gl40->glUniformMatrix4dv);
            ADD_MATRIX(GL_DOUBLE_MAT2x3, doubles, context.gl40->glUniformMatrix2x3dv);
            ADD_MATRIX(GL_DOUBLE_MAT2x4, doubles, context.gl40->glUniformMatrix2x4dv);
            ADD_MATRIX(GL_DOUBLE_MAT3x2, doubles, context.gl40->glUniformMatrix3x2dv);
            ADD_MATRIX(GL_DOUBLE_MAT3x4, doubles, context.gl40->glUniformMatrix3x4dv);
            ADD_MATRIX(GL_DOUBLE_MAT4x2, doubles, context.gl40->glUniformMatrix4x2dv);
            ADD_MATRIX(GL_DOUBLE_MAT4x3, doubles, context.gl40->glUniformMatrix4x3dv);
        }
    }
#undef ADD
#undef ADD_MATRIX
}
