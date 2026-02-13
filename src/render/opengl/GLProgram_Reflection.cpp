
#include "GLProgram.h"

namespace {
    bool operator==(const Reflection::Builder::DescriptorBinding &a,
        const Reflection::Builder::DescriptorBinding &b)
    {
        return std::tie(a.descriptorType, a.binding)
            == std::tie(b.descriptorType, b.binding);
    }

    template <typename C, typename T>
    bool containsValue(const C &container, const T &value)
    {
        for (const auto &val : container)
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

    std::string removeArrayIndex(std::string name)
    {
        if (auto index = name.find('['); index != std::string::npos)
            return name.substr(0, index);
        return name;
    }

    bool isElementName(const std::string &string)
    {
        return string.find('[') != std::string::npos;
    }

    std::string removePrefix(const std::string &string,
        const std::string &prefix)
    {
        if (string.starts_with(prefix + '.'))
            return string.substr(prefix.size() + 1);
        return string;
    }

    std::optional<Field::DataType> getComponentDataType(GLenum type)
    {
        switch (type) {
        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:          return Field::DataType::Int32;
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:         return Field::DataType::Uint32;
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT3x2:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x2:
        case GL_FLOAT_MAT4x3:      return Field::DataType::Float;
        case GL_DOUBLE:
        case GL_DOUBLE_VEC2:
        case GL_DOUBLE_VEC3:
        case GL_DOUBLE_VEC4:
        case GL_DOUBLE_MAT2:
        case GL_DOUBLE_MAT3:
        case GL_DOUBLE_MAT4:
        case GL_DOUBLE_MAT2x3:
        case GL_DOUBLE_MAT2x4:
        case GL_DOUBLE_MAT3x2:
        case GL_DOUBLE_MAT3x4:
        case GL_DOUBLE_MAT4x2:
        case GL_DOUBLE_MAT4x3:     return Field::DataType::Double;
        default:                   return { };
        }
    }

    int getTypeRows(GLenum type)
    {
        switch (type) {
        case GL_INT_VEC2:
        case GL_UNSIGNED_INT_VEC2:
        case GL_BOOL_VEC2:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3x2:
        case GL_FLOAT_MAT4x2:
        case GL_DOUBLE_VEC2:
        case GL_DOUBLE_MAT2:
        case GL_DOUBLE_MAT3x2:
        case GL_DOUBLE_MAT4x2:     return 2;
        case GL_INT_VEC3:
        case GL_UNSIGNED_INT_VEC3:
        case GL_BOOL_VEC3:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4x3:
        case GL_FLOAT_MAT2x3:
        case GL_DOUBLE_VEC3:
        case GL_DOUBLE_MAT3:
        case GL_DOUBLE_MAT2x3:
        case GL_DOUBLE_MAT4x3:     return 3;
        case GL_INT_VEC4:
        case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL_VEC4:
        case GL_FLOAT_VEC4:
        case GL_FLOAT_MAT4:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT3x4:
        case GL_DOUBLE_VEC4:
        case GL_DOUBLE_MAT4:
        case GL_DOUBLE_MAT2x4:
        case GL_DOUBLE_MAT3x4:     return 4;
        default:                   return 1;
        }
    }

    int getTypeColumns(GLenum type)
    {
        switch (type) {
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT2x3:
        case GL_DOUBLE_MAT2x3:
        case GL_FLOAT_MAT2x4:
        case GL_DOUBLE_MAT2x4: return 2;
        case GL_FLOAT_MAT3x2:
        case GL_DOUBLE_MAT3x2:
        case GL_FLOAT_MAT3:
        case GL_DOUBLE_MAT3:
        case GL_FLOAT_MAT3x4:
        case GL_DOUBLE_MAT3x4: return 3;
        case GL_FLOAT_MAT4x2:
        case GL_DOUBLE_MAT4x2:
        case GL_FLOAT_MAT4x3:
        case GL_DOUBLE_MAT4x3:
        case GL_FLOAT_MAT4:
        case GL_DOUBLE_MAT4:   return 4;
        default:               return 1;
        }
    }

    int getSize(GLenum type)
    {
        const auto dataType = getComponentDataType(type);
        if (!dataType)
            return 0;
    
        return getTypeRows(type) * getTypeColumns(type) *
            getDataTypeSize(*dataType);
    }

    SpvReflectTypeFlags getTypeFlags(GLenum type, bool isArray)
    {
        auto typeFlags = SpvReflectTypeFlags{};
        switch (type) {
            // TODO:
        case GL_SAMPLER_2D:
            return SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER
                | SPV_REFLECT_TYPE_FLAG_FLOAT;
        default:
            switch (getComponentDataType(type).value_or(Field::DataType::Uint8)) {
            case Field::DataType::Int32:
            case Field::DataType::Uint32:
                typeFlags |= SPV_REFLECT_TYPE_FLAG_INT;
                break;
            case Field::DataType::Float:
            case Field::DataType::Double:
                typeFlags |= SPV_REFLECT_TYPE_FLAG_FLOAT;
                break;

            default: Q_ASSERT(!"unhandled type");
            }
        }

        const auto rows = getTypeRows(type);
        const auto columns = getTypeColumns(type);
        if (rows > 1 && columns > 1)
            typeFlags |= SPV_REFLECT_TYPE_FLAG_MATRIX;
        else if (rows > 1)
            typeFlags |= SPV_REFLECT_TYPE_FLAG_VECTOR;

        if (isArray)
            typeFlags |= SPV_REFLECT_TYPE_FLAG_ARRAY;

        return typeFlags;
    }

    SpvReflectNumericTraits getNumericTraits(GLenum type)
    {
        const auto dataType = getComponentDataType(type);
        if (!dataType)
            return {};

        const auto bytes = getDataTypeSize(*dataType);
        const auto rows = getTypeRows(type);
        const auto columns = getTypeColumns(type);

        auto numeric = SpvReflectNumericTraits{};
        numeric.scalar.width = bytes * 8;
        numeric.scalar.signedness = (type == Field::DataType::Uint32 ? 0 : 1u);
        if (columns > 1) {
            numeric.matrix.column_count = columns;
            numeric.matrix.row_count = rows;
        } else {
            numeric.vector.component_count = rows;
        }
        return numeric;
    }

    SpvReflectFormat getInputFormat(GLenum format)
    {
        switch (format) {
        case GL_UNSIGNED_INT:      return SPV_REFLECT_FORMAT_R32_UINT;
        case GL_INT:               return SPV_REFLECT_FORMAT_R32_SINT;
        case GL_FLOAT:             return SPV_REFLECT_FORMAT_R32_SFLOAT;
        case GL_UNSIGNED_INT_VEC2: return SPV_REFLECT_FORMAT_R32G32_UINT;
        case GL_INT_VEC2:          return SPV_REFLECT_FORMAT_R32G32_SINT;
        case GL_FLOAT_VEC2:        return SPV_REFLECT_FORMAT_R32G32_SFLOAT;
        case GL_UNSIGNED_INT_VEC3: return SPV_REFLECT_FORMAT_R32G32B32_UINT;
        case GL_INT_VEC3:          return SPV_REFLECT_FORMAT_R32G32B32_SINT;
        case GL_FLOAT_VEC3:        return SPV_REFLECT_FORMAT_R32G32B32_SFLOAT;
        case GL_UNSIGNED_INT_VEC4: return SPV_REFLECT_FORMAT_R32G32B32A32_UINT;
        case GL_INT_VEC4:          return SPV_REFLECT_FORMAT_R32G32B32A32_SINT;
        case GL_FLOAT_VEC4:        return SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT;
        default:                   Q_ASSERT(!"not handled format");
        }
        return SPV_REFLECT_FORMAT_UNDEFINED;
    }

    bool isOpaqueType(GLenum type)
    {
        return !getComponentDataType(type).has_value();
    }

    SpvReflectDescriptorType getDescriptorType(GLenum type)
    {
        switch (type) {
        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_CUBE_MAP_ARRAY:
            return SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        case GL_IMAGE_1D:
        case GL_IMAGE_2D:
        case GL_IMAGE_3D:
        case GL_IMAGE_2D_RECT:
        case GL_IMAGE_CUBE:
        case GL_IMAGE_BUFFER:
        case GL_IMAGE_1D_ARRAY:
        case GL_IMAGE_2D_ARRAY:
        case GL_IMAGE_CUBE_MAP_ARRAY:
        case GL_IMAGE_2D_MULTISAMPLE:
        case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
            return SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        default: Q_ASSERT(!"not handled data type");
        }
        return {};
    }

    SpvReflectImageTraits getImageTraits(GLenum type)
    {
        enum Flag : uint32_t {
            Sampled = 1 << 0,
            Arrayed = 2 << 1,
            MS = 1 << 2,
            Depth = 1 << 3,
        };
        const auto make = [](SpvDim dim, uint32_t flags) {
            return SpvReflectImageTraits{
                .dim = dim,
                .depth = (flags & Depth ? 1u : 0),
                .arrayed = (flags & Arrayed ? 1u : 0),
                .ms = (flags & MS ? 1u : 0),
                .sampled = (flags & Sampled ? 1u : 0),
                .image_format = SpvImageFormatUnknown,
            };
        };
        switch (type) {
        case GL_SAMPLER_1D:             return make(SpvDim1D, Sampled);
        case GL_SAMPLER_2D:             return make(SpvDim2D, Sampled);
        case GL_SAMPLER_3D:             return make(SpvDim3D, Sampled);
        case GL_SAMPLER_CUBE:           return make(SpvDimCube, Sampled);
        case GL_SAMPLER_2D_MULTISAMPLE: return make(SpvDim2D, Sampled | MS);
        case GL_SAMPLER_1D_ARRAY:       return make(SpvDim1D, Sampled | Arrayed);
        case GL_SAMPLER_2D_ARRAY:       return make(SpvDim2D, Sampled | Arrayed);
        case GL_SAMPLER_CUBE_MAP_ARRAY:
            return make(SpvDimCube, Sampled | Arrayed);
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
            return make(SpvDim2D, Sampled | MS | Arrayed);
        default: return {};
        }
    }

    Reflection::Builder::BlockVariable createBlockVariable(
        const std::string &name, GLenum type, int arraySize, int offset,
        int arrayStride, int matrixStride, bool isRowMajor,
        int topLevelArraySize, int topLevelArrayStride)
    {
        const auto isArray = isElementName(name);
        return Reflection::Builder::BlockVariable{ 
            .name = removeArrayIndex(name),
            .offset = static_cast<uint32_t>(offset),
            .size = static_cast<uint32_t>(getSize(type)),
            .typeFlags = getTypeFlags(type, isArray),
            .numeric = getNumericTraits(type),
            .image = getImageTraits(type),
            .array = {
                .dims_count = (isArray ? 1u : 0u),
                .dims = { static_cast<uint32_t>(isArray ? arraySize : 0) },
                .stride = static_cast<uint32_t>(isArray ? arrayStride : 0),
            },
        };
    }
} // namespace

void GLProgram::generateReflectionFromProgram(GLuint program)
{
    using BlockVariable = Reflection::Builder::BlockVariable;
    using InterfaceVariable = Reflection::Builder::InterfaceVariable;
    using DescriptorBinding = Reflection::Builder::DescriptorBinding;
    auto &gl = GLContext::currentContext();

    const auto getResourceValues = [&](GLuint programInterface, GLuint index,
                                       auto properties) {
        auto result = std::array<GLint, properties.size()>{};
        gl.glGetProgramResourceiv(program, programInterface, index,
            static_cast<GLsizei>(properties.size()), properties.data(),
            static_cast<GLsizei>(result.size()), nullptr, result.data());
        return result;
    };

    const auto getResourceValue = [&](GLuint programInterface, GLuint index,
                                      GLuint property) -> GLint {
        auto result = GLint{};
        gl.glGetProgramResourceiv(program, programInterface, index, 1,
            &property, 1, nullptr, &result);
        return result;
    };

    const auto getResourceName = [&](GLuint programInterface, GLuint index) {
        auto name = std::string();
        name.resize(
            getResourceValue(programInterface, index, GL_NAME_LENGTH) - 1);
        gl.glGetProgramResourceName(program, programInterface, index,
            static_cast<GLsizei>(name.size() + 1), nullptr, name.data());
        return name;
    };

    const auto forEachActiveResource = [&](GLuint programInterface,
                                           auto callback) {
        auto resourceCount = GLint{};
        gl.glGetProgramInterfaceiv(program, programInterface,
            GL_ACTIVE_RESOURCES, &resourceCount);
        for (auto i = 0u; i < static_cast<GLuint>(resourceCount); ++i)
            callback(programInterface, i, getResourceName(programInterface, i));
    };

    const auto forEachActiveVariable = [&](GLuint programInterface,
                                           GLuint index,
                                           GLuint variableProgramInterface,
                                           auto callback) {
        auto variableIndices = std::vector<GLint>(
            getResourceValue(programInterface, index, GL_NUM_ACTIVE_VARIABLES));
        const auto property = GLenum{ GL_ACTIVE_VARIABLES };
        gl.glGetProgramResourceiv(program, programInterface, index, 1,
            &property, static_cast<GLsizei>(variableIndices.size()), nullptr,
            variableIndices.data());
        for (auto index : variableIndices)
            callback(variableProgramInterface, index,
                getResourceName(variableProgramInterface, index));
    };

    auto reflection = std::make_unique<Reflection::Builder>();
    if (!mShaders.empty())
        reflection->shaderStage = getShaderStage(mShaders.front().type());

    forEachActiveResource(GL_UNIFORM,
        [&](GLuint programInterface, GLuint index, std::string name) {
            const auto [blockIndex, arraySize, type, location] =
                getResourceValues(programInterface, index,
                    std::to_array<GLuint>({
                        GL_BLOCK_INDEX,
                        GL_ARRAY_SIZE,
                        GL_TYPE,
                        GL_LOCATION,
                    }));

            // skip uniforms in uniform blocks
            if (blockIndex != -1)
                return;

            Q_ASSERT(location >= 0);
            Q_ASSERT(type);
            const auto arrayName = getArrayName(QString::fromStdString(name));
            const auto baseName = (arrayName.isEmpty()
                    ? QString::fromStdString(name)
                    : arrayName);

            if (isOpaqueType(type)) {
                reflection->descriptorsBindings.push_back({
                    .descriptorType = getDescriptorType(type),
                    .name = baseName.toStdString(),
                    .image = getImageTraits(type),
                    // TODO: array
                });
                mDescriptorUniformLocations[baseName] = location;
            } else {
                mUniforms.push_back({
                    .name = baseName,
                    .location = location,
                    .dataType = static_cast<GLenum>(type),
                    .arraySize = arraySize,
                });
                if (!arrayName.isEmpty())
                    for (auto i = 0; i < arraySize; ++i)
                        mUniforms.push_back({
                            .name = getElementName(arrayName, i),
                            .location = location + i,
                            .dataType = static_cast<GLenum>(type),
                            .arraySize = 1,
                        });
            }
        });

    forEachActiveResource(GL_UNIFORM_BLOCK,
        [&](GLuint programInterface, GLuint index, std::string bufferName) {
            const auto [bufferBinding, bufferDataSize] =
                getResourceValues(programInterface, index,
                    std::to_array<GLuint>({
                        GL_BUFFER_BINDING,
                        GL_BUFFER_DATA_SIZE,
                    }));

            auto members = std::vector<BlockVariable>();
            forEachActiveVariable(GL_UNIFORM_BLOCK, index, GL_UNIFORM,
                [&](GLuint programInterface, GLuint index, std::string name) {
                    const auto [type, arraySize, offset, arrayStride,
                        matrixStride,
                        isRowMajor] = getResourceValues(programInterface, index,
                        std::to_array<GLuint>({
                            GL_TYPE,
                            GL_ARRAY_SIZE,
                            GL_OFFSET,
                            GL_ARRAY_STRIDE,
                            GL_MATRIX_STRIDE,
                            GL_IS_ROW_MAJOR,
                        }));

                    members.push_back(createBlockVariable(
                        removePrefix(name, bufferName), type, arraySize, offset,
                        arrayStride, matrixStride, isRowMajor, 0, 0));
                });

            reflection->descriptorsBindings.push_back(DescriptorBinding{
              .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .typeName = std::move(bufferName),
              .block = {
                  .size = static_cast<uint32_t>(bufferDataSize),
                  .members = std::move(members),
              },
              .binding = static_cast<uint32_t>(bufferBinding),
            });
        });

    forEachActiveResource(GL_SHADER_STORAGE_BLOCK,
        [&](GLuint programInterface, GLuint index, std::string bufferName) {
            const auto [bufferBinding, bufferDataSize] =
                getResourceValues(programInterface, index,
                    std::to_array<GLuint>({
                        GL_BUFFER_BINDING,
                        GL_BUFFER_DATA_SIZE,
                    }));

            auto members = std::vector<BlockVariable>();
            forEachActiveVariable(programInterface, index, GL_BUFFER_VARIABLE,
                [&](GLuint programInterface, GLuint index, std::string name) {
                    const auto [type, arraySize, offset, arrayStride,
                        matrixStride, isRowMajor, topLevelArraySize,
                        topLevelArrayStride] =
                        getResourceValues(programInterface, index,
                            std::to_array<GLuint>({
                                GL_TYPE,
                                GL_ARRAY_SIZE,
                                GL_OFFSET,
                                GL_ARRAY_STRIDE,
                                GL_MATRIX_STRIDE,
                                GL_IS_ROW_MAJOR,
                                GL_TOP_LEVEL_ARRAY_SIZE,
                                GL_TOP_LEVEL_ARRAY_STRIDE,
                            }));

                    members.push_back(createBlockVariable(
                        removePrefix(name, bufferName), type, arraySize, offset,
                        arrayStride, matrixStride, isRowMajor,
                        topLevelArraySize, topLevelArrayStride));
                });

            reflection->descriptorsBindings.push_back(
                DescriptorBinding{ 
                    .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .typeName = std::move(bufferName),
                    .block = {
                        .size = static_cast<uint32_t>(bufferDataSize),
                        .members = std::move(members),
                    },
                    .binding = static_cast<uint32_t>(bufferBinding),
                });
        });

    auto inputSemantics = std::map<std::string, std::string>();
    if (mSession.shaderLanguage == Session::ShaderLanguage::HLSL)
        if (auto spirv = find(mStageSpirv, Shader::ShaderType::Vertex))
            for (auto reflection = Reflection(*spirv);
                auto input : reflection.inputVariables())
                if (input->name && input->semantic)
                    inputSemantics[input->name] = input->semantic;

    for (auto programInterface : { GL_PROGRAM_INPUT, GL_PROGRAM_OUTPUT })
        forEachActiveResource(programInterface,
            [&](GLuint programInterface, GLuint index, std::string name) {
                const auto [type, arraySize, location, isPerPatch,
                    locationComponent] = getResourceValues(programInterface,
                    index,
                    std::to_array<GLuint>({
                        GL_TYPE,
                        GL_ARRAY_SIZE,
                        GL_LOCATION,
                        GL_IS_PER_PATCH,
                        GL_LOCATION_COMPONENT,
                    }));

                if (location >= 0) {
                    auto &variables = (programInterface == GL_PROGRAM_INPUT
                            ? reflection->inputs
                            : reflection->outputs);
                    variables.push_back(InterfaceVariable{
                        .name = name,
                        .semantic = inputSemantics[name],
                        .format = getInputFormat(type),
                        .location = static_cast<GLuint>(location),
                    });
                }
            });

    mReflection = Reflection(std::move(reflection));
}

void GLProgram::enumerateSubroutines(GLuint program)
{
    auto &gl = GLContext::currentContext();
    auto buffer = std::vector<char>(256);
    auto nameLength = GLint{};
    auto uniforms = GLint{};

    auto stages = QSet<Shader::ShaderType>();
    for (const auto &shader : mShaders)
        stages += shader.type();

    auto subroutineIndices = std::vector<GLint>();
    for (const auto &stage : std::as_const(stages)) {
        gl.glGetProgramStageiv(program, stage, GL_ACTIVE_SUBROUTINE_UNIFORMS,
            &uniforms);
        for (auto i = 0u; i < static_cast<GLuint>(uniforms); ++i) {
            gl.glGetActiveSubroutineUniformName(program, stage, i,
                static_cast<GLsizei>(buffer.size()), &nameLength,
                buffer.data());
            auto name = QString(buffer.data());

            auto compatible = GLint{};
            gl.glGetActiveSubroutineUniformiv(program, stage, i,
                GL_NUM_COMPATIBLE_SUBROUTINES, &compatible);

            subroutineIndices.resize(static_cast<size_t>(compatible));
            gl.glGetActiveSubroutineUniformiv(program, stage, i,
                GL_COMPATIBLE_SUBROUTINES, subroutineIndices.data());

            auto subroutines = QStringList();
            for (auto index : subroutineIndices) {
                gl.glGetActiveSubroutineName(program, stage,
                    static_cast<GLuint>(index),
                    static_cast<GLsizei>(buffer.size()), &nameLength,
                    buffer.data());
                subroutines += QString(buffer.data());
            }
            mStageSubroutines[stage].push_back({
                .name = name,
                .subroutines = subroutines,
            });
        }
    }
}

int GLProgram::getDescriptorUniformLocation(
    const SpvReflectDescriptorBinding &desc) const
{
    const auto it = mDescriptorUniformLocations.find(desc.name);
    return (it == mDescriptorUniformLocations.end() ? -1 : it->second);
}

void GLProgram::automapDescriptorBindings()
{
    // generate one binding unit per unique uniform location
    auto uniqueLocations = QSet<int>();
    for (auto &[name, location] : mDescriptorUniformLocations)
        uniqueLocations.insert(location);

    for (auto &desc : std::span(mReflection->descriptor_bindings,
             mReflection->descriptor_binding_count))
        if (auto location = getDescriptorUniformLocation(desc); location >= 0)
            desc.binding = std::distance(uniqueLocations.begin(),
                uniqueLocations.find(location));
}
