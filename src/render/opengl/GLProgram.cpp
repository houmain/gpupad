#include "GLProgram.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "GLStream.h"
#include "scripting/ScriptEngine.h"
#include <QRegularExpression>
#include <array>

namespace {
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

    QString getArrayName(const QString &name)
    {
        if (name.endsWith("[0]"))
            return name.left(name.size() - 3);
        return {};
    }

    QString getElementName(const QString &arrayName, int index)
    {
        return QString("%1[%2]").arg(arrayName).arg(index);
    }

    QString removeInstanceName(const QString &bufferName)
    {
        if (auto index = bufferName.indexOf('.'); index >= 0) {
            if (auto bracket = bufferName.indexOf('['); bracket >= 0) {
                auto copy = bufferName;
                copy.remove(index, bracket - index);
                return copy;
            }
            return bufferName.left(index);
        }
        return bufferName;
    }

    QString removeArrayIndex(const QString &bufferName)
    {
        if (auto index = bufferName.indexOf('['); index >= 0)
            return bufferName.left(index);
        return bufferName;
    }

    QString removePrefix(const QString &string, const QString &prefix)
    {
        if (string.startsWith(prefix + '.'))
            return string.mid(prefix.size() + 1);
        return string;
    }

    GLenum getBindingGLType(const SpvReflectDescriptorBinding &binding)
    {
#define ADD_EACH_DIMENSION(TYPE)                 \
    switch (image.dim) {                         \
    case SpvDim1D:   return GL_##TYPE##_1D;      \
    case SpvDim2D:   return GL_##TYPE##_2D;      \
    case SpvDim3D:   return GL_##TYPE##_3D;      \
    case SpvDimCube: return GL_##TYPE##_CUBE;    \
    case SpvDimRect: return GL_##TYPE##_2D_RECT; \
    default:         break;                      \
    }

        const auto flags = binding.type_description->type_flags;
        const auto image = binding.type_description->traits.image;
        const auto numeric = binding.type_description->traits.numeric;
        const auto is_integral = (flags & SPV_REFLECT_TYPE_FLAG_INT);
        const auto is_signed = (is_integral && numeric.scalar.signedness != 0);
        if (flags
            & (SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER
                | SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE)) {
            if (is_integral && is_signed) {
                ADD_EACH_DIMENSION(INT_SAMPLER)
            } else if (is_integral && !is_signed) {
                ADD_EACH_DIMENSION(UNSIGNED_INT_SAMPLER)
            } else {
                ADD_EACH_DIMENSION(SAMPLER)
            }
        } else if (flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE) {
            if (is_integral && is_signed) {
                ADD_EACH_DIMENSION(INT_IMAGE)
            } else if (is_integral && !is_signed) {
                ADD_EACH_DIMENSION(UNSIGNED_INT_IMAGE)
            } else {
                ADD_EACH_DIMENSION(IMAGE)
            }
        }
#undef ADD_EACH_DIMENSION
        assert(!"not handled binding type");
        return GL_NONE;
    }

    int getDataTypeSize(GLenum dataType)
    {
        switch (dataType) {
        case GL_FLOAT:             return 1 * sizeof(GLfloat);
        case GL_FLOAT_VEC2:        return 2 * sizeof(GLfloat);
        case GL_FLOAT_VEC3:        return 3 * sizeof(GLfloat);
        case GL_FLOAT_VEC4:        return 4 * sizeof(GLfloat);
        case GL_DOUBLE:            return 1 * sizeof(GLdouble);
        case GL_DOUBLE_VEC2:       return 2 * sizeof(GLdouble);
        case GL_DOUBLE_VEC3:       return 3 * sizeof(GLdouble);
        case GL_DOUBLE_VEC4:       return 4 * sizeof(GLdouble);
        case GL_INT:               return 1 * sizeof(GLint);
        case GL_INT_VEC2:          return 2 * sizeof(GLint);
        case GL_INT_VEC3:          return 3 * sizeof(GLint);
        case GL_INT_VEC4:          return 4 * sizeof(GLint);
        case GL_UNSIGNED_INT:      return 1 * sizeof(GLuint);
        case GL_UNSIGNED_INT_VEC2: return 2 * sizeof(GLuint);
        case GL_UNSIGNED_INT_VEC3: return 3 * sizeof(GLuint);
        case GL_UNSIGNED_INT_VEC4: return 4 * sizeof(GLuint);
        case GL_BOOL:              return 1 * sizeof(GLboolean);
        case GL_BOOL_VEC2:         return 2 * sizeof(GLboolean);
        case GL_BOOL_VEC3:         return 3 * sizeof(GLboolean);
        case GL_BOOL_VEC4:         return 4 * sizeof(GLboolean);
        case GL_FLOAT_MAT2:        return 2 * 2 * sizeof(GLfloat);
        case GL_FLOAT_MAT3:        return 3 * 3 * sizeof(GLfloat);
        case GL_FLOAT_MAT4:        return 4 * 4 * sizeof(GLfloat);
        case GL_FLOAT_MAT2x3:      return 2 * 3 * sizeof(GLfloat);
        case GL_FLOAT_MAT2x4:      return 2 * 4 * sizeof(GLfloat);
        case GL_FLOAT_MAT3x2:      return 3 * 2 * sizeof(GLfloat);
        case GL_FLOAT_MAT3x4:      return 3 * 4 * sizeof(GLfloat);
        case GL_FLOAT_MAT4x2:      return 4 * 2 * sizeof(GLfloat);
        case GL_FLOAT_MAT4x3:      return 4 * 3 * sizeof(GLfloat);
        case GL_DOUBLE_MAT2:       return 2 * 2 * sizeof(GLdouble);
        case GL_DOUBLE_MAT3:       return 3 * 3 * sizeof(GLdouble);
        case GL_DOUBLE_MAT4:       return 4 * 4 * sizeof(GLdouble);
        case GL_DOUBLE_MAT2x3:     return 2 * 3 * sizeof(GLdouble);
        case GL_DOUBLE_MAT2x4:     return 2 * 4 * sizeof(GLdouble);
        case GL_DOUBLE_MAT3x2:     return 3 * 2 * sizeof(GLdouble);
        case GL_DOUBLE_MAT3x4:     return 3 * 4 * sizeof(GLdouble);
        case GL_DOUBLE_MAT4x2:     return 4 * 2 * sizeof(GLdouble);
        case GL_DOUBLE_MAT4x3:     return 4 * 3 * sizeof(GLdouble);
        }
        return 0;
    }

    bool isOpaqueType(GLenum dataType)
    {
        return (getDataTypeSize(dataType) == 0);
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

    if (!getInterfaceFromSpirv()) {
        fillInterface(mInterface, mProgramObject);
        Q_ASSERT(glGetError() == GL_NO_ERROR);
    }

    automapUniformBindings(mInterface);

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
    if (mSession.shaderCompiler.isEmpty()) {
        for (auto &shader : mShaders)
            if (!shader.compile(mPrintf))
                return false;
    } else {
        auto inputs = std::vector<Spirv::Input>();
        for (auto &shader : mShaders)
            inputs.push_back(shader.getSpirvCompilerInput(mPrintf));

        auto stages = Spirv::compile(mSession, inputs, mItemId, mLinkMessages);
        for (auto &shader : mShaders) {
            const auto &spirv = stages[shader.type()];
            if (!shader.specialize(spirv))
                return false;

            if (getInterfaceFromSpirv())
                fillInterface(mInterface, spirv.getInterface());
        }
    }
    return true;
}

bool GLProgram::linkProgram()
{
    if (mShaders.empty()) {
        mLinkMessages +=
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
        GLShader::parseLog(log.data(), mLinkMessages, mItemId, {});
    }
    if (status != GL_TRUE)
        return false;

    mProgramObject = std::move(program);
    return true;
}

bool GLProgram::getInterfaceFromSpirv() const
{
    // glslang cannot automap bindings or locations when targeting OpenGL
    // keep enumerating interface using OpenGL for now
    return false;
    //return !mSession.shaderCompiler.isEmpty();
}

void GLProgram::fillInterface(Interface &interface, GLuint program)
{
    auto &gl = GLContext::currentContext();
    auto buffer = std::array<char, 256>();
    auto size = GLint{};
    auto dataType = GLenum{};
    auto nameLength = GLint{};

    auto uniforms = GLint{};
    auto nextBindingUnit = 0;
    gl.glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
    for (auto i = 0u; i < static_cast<GLuint>(uniforms); ++i) {
        gl.glGetActiveUniform(program, i, static_cast<GLsizei>(buffer.size()),
            &nameLength, &size, &dataType, buffer.data());

        const auto name = QString(buffer.data());
        const auto location =
            gl.glGetUniformLocation(program, qPrintable(name));

        if (location >= 0) {
            Q_ASSERT(dataType);
            const auto arrayName = getArrayName(name);

            // allow to set whole array only when non-opaque
            if (arrayName.isEmpty() || !isOpaqueType(dataType))
                interface.uniforms[arrayName.isEmpty() ? name : arrayName] = {
                    .binding = -1,
                    .location = location,
                    .dataType = dataType,
                    .size = size,
                };

            if (!arrayName.isEmpty())
                for (auto j = 0; j < size; ++j) {
                    const auto elementName = getElementName(arrayName, j);
                    const auto location = gl.glGetUniformLocation(program,
                        qPrintable(elementName));
                    Q_ASSERT(location >= 0);
                    interface.uniforms[elementName] = {
                        .binding = -1,
                        .location = location,
                        .dataType = dataType,
                        .size = 1,
                    };
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
        const auto bufferName = QString(buffer.data());
        const auto uniformBlockIndex =
            gl.glGetUniformBlockIndex(program, buffer.data());

        // skip when an invalid index is returned (with glslang on AMD/Linux)
        if (static_cast<int32_t>(uniformBlockIndex) < 0)
            continue;

        const auto uniformBlockBinding = i;
        gl.glUniformBlockBinding(program, uniformBlockIndex,
            uniformBlockBinding);
        gl.glGetActiveUniformBlockiv(program, i,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniforms);
        auto uniformIndices = std::vector<GLint>(static_cast<size_t>(uniforms));
        gl.glGetActiveUniformBlockiv(program, i,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, uniformIndices.data());

        // glslang generates block_name.instance_name[N] instead of block_name[N]
        const auto blockName = removeInstanceName(bufferName);
        const auto memberPrefix = removeArrayIndex(bufferName);
        auto members = std::map<QString, Interface::BufferMember>();
        auto minimumSize = 0;
        for (GLuint index : uniformIndices) {
            gl.glGetActiveUniform(program, index,
                static_cast<GLsizei>(buffer.size()), &nameLength, &size,
                &dataType, buffer.data());

            // qualify member with buffer name (glslang does already, driver not always)
            auto memberName = QString(buffer.data());
            memberName = removePrefix(memberName, memberPrefix);
            memberName = blockName + '.' + memberName;
            memberName = removeGlobalUniformBlockName(memberName);

            auto offset = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index, GL_UNIFORM_OFFSET,
                &offset);
            auto arrayStride = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index,
                GL_UNIFORM_ARRAY_STRIDE, &arrayStride);
            if (!arrayStride)
                arrayStride = getDataTypeSize(dataType);
            auto matrixStride = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index,
                GL_UNIFORM_MATRIX_STRIDE, &matrixStride);
            auto isRowMajor = GLint{};
            gl.glGetActiveUniformsiv(program, 1, &index,
                GL_UNIFORM_IS_ROW_MAJOR, &isRowMajor);

            const auto arrayName = getArrayName(memberName);
            members[arrayName.isEmpty() ? memberName : arrayName] = {
                .dataType = dataType,
                .size = size,
                .offset = offset,
                .arrayStride = arrayStride,
                .matrixStride = matrixStride,
                .isRowMajor = (isRowMajor != 0),
            };

            if (!arrayName.isEmpty())
                for (auto j = 0; j < size; ++j) {
                    const auto elementName = getElementName(arrayName, j);
                    members[elementName] = {
                        .dataType = dataType,
                        .size = 1,
                        .offset = offset + arrayStride * j,
                        .arrayStride = arrayStride,
                        .matrixStride = matrixStride,
                        .isRowMajor = (isRowMajor != 0),
                    };
                }
            minimumSize = std::max(minimumSize, offset + size * arrayStride);
        }
        Q_ASSERT(minimumSize);
        if (!minimumSize)
            continue;

        interface.bufferBindingPoints[blockName] = {
            .target = GL_UNIFORM_BUFFER,
            .index = uniformBlockBinding,
            .members = std::move(members),
            .minimumSize = minimumSize,
            .readonly = true,
        };
    }

    auto attributes = GLint{};
    gl.glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributes);
    for (auto i = 0u; i < static_cast<GLuint>(attributes); ++i) {
        gl.glGetActiveAttrib(program, i, static_cast<GLsizei>(buffer.size()),
            &nameLength, &size, &dataType, buffer.data());
        const auto name = QString(buffer.data());
        const auto location = gl.glGetAttribLocation(program, qPrintable(name));
        if (location >= 0)
            interface.attributeLocations[name] = static_cast<GLuint>(location);
    }

    if (auto gl40 = gl.v4_0) {
        auto stages = QSet<Shader::ShaderType>();
        for (const auto &shader : mShaders)
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
            // glslang generates block_name.instance_name instead of block_name
            const auto bufferName = QString(buffer.data());
            const auto blockName = removeInstanceName(bufferName);
            const auto storageBlockIndex = gl.v4_3->glGetProgramResourceIndex(
                program, GL_SHADER_STORAGE_BLOCK, qPrintable(bufferName));
            const auto storageBlockBinding = i;
            gl43->glShaderStorageBlockBinding(program, storageBlockIndex,
                storageBlockBinding);
            interface.bufferBindingPoints[blockName] = {
                .target = GL_SHADER_STORAGE_BUFFER,
                .index = storageBlockBinding,
            };
        }
    }
#endif
}

void GLProgram::fillInterface(Interface &interface,
    const Spirv::Interface &spirvInterface)
{
    if (spirvInterface->shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        for (auto i = 0u; i < spirvInterface->input_variable_count; ++i) {
            const auto &input = *spirvInterface->input_variables[i];
            if (!isBuiltIn(input))
                interface.attributeLocations[input.name] = input.location;
        }

    for (auto i = 0u; i < spirvInterface->descriptor_binding_count; ++i) {
        const auto &desc = spirvInterface->descriptor_bindings[i];
        if (!desc.accessed)
            continue;

        switch (desc.descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:          {
            interface.uniforms[desc.name] = {
                .binding = static_cast<int>(desc.binding),
                .location = -1,
                .dataType = getBindingGLType(desc),
                .size = static_cast<int>(getBindingArraySize(desc.array)),
            };
            break;
        }

        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            auto target = GLenum{ GL_UNIFORM_BUFFER };
            auto readonly = true;

            if (desc.descriptor_type
                == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                target = GL_SHADER_STORAGE_BUFFER;
                readonly = false;
            }

            auto members = std::map<QString, Interface::BufferMember>();
            auto minimumSize = 0;
            std::function<void(const SpvReflectBlockVariable &)> addMember;
            addMember = [&](const SpvReflectBlockVariable &member) {
                if (member.member_count) {
                    for (auto i = 0u; i < member.member_count; ++i)
                        addMember(member.members[i]);
                    return;
                }
                const auto offset = static_cast<int>(member.absolute_offset);
                const auto dataType = getBufferMemberGLType(member);
                auto arrayStride = getBufferMemberArrayStride(member);
                if (!arrayStride)
                    arrayStride = getDataTypeSize(dataType);
                const auto size = getBufferMemberArraySize(member);

                members[member.name] = {
                    .dataType = dataType,
                    .size = size,
                    .offset = offset,
                    .arrayStride = arrayStride,
                };
                minimumSize =
                    std::max(minimumSize, offset + size * arrayStride);
            };
            addMember(desc.block);

            Q_ASSERT(minimumSize);
            if (!minimumSize)
                continue;

            interface.bufferBindingPoints[desc.type_description->type_name] = {
                .target = target,
                .index = desc.binding,
                .members = std::move(members),
                .minimumSize = minimumSize,
                .readonly = readonly,
            };
            break;
        }

        default: Q_ASSERT(!"not implemented");
        }
    }
}

void GLProgram::automapUniformBindings(Interface &interface)
{
    // generate one binding unit per unique uniform location
    auto uniqueLocations = QSet<int>();
    for (auto &[name, uniform] : mInterface.uniforms)
        if (uniform.location >= 0 && isOpaqueType(uniform.dataType))
            uniqueLocations.insert(uniform.location);

    for (auto &[name, uniform] : mInterface.uniforms)
        if (uniform.location >= 0 && isOpaqueType(uniform.dataType))
            uniform.binding = std::distance(uniqueLocations.begin(),
                uniqueLocations.find(uniform.location));
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

GLBuffer &GLProgram::getDynamicUniformBuffer(const QString &name, int size)
{
    auto it = mDynamicUniformBuffers.find(name);
    if (it == mDynamicUniformBuffers.end())
        it = mDynamicUniformBuffers.emplace(name, size).first;
    return it->second;
}
