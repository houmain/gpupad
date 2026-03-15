
#include "GLProgram.h"

namespace {
    using BlockVariable = Reflection::Builder::BlockVariable;
    using InterfaceVariable = Reflection::Builder::InterfaceVariable;
    using DescriptorBinding = Reflection::Builder::DescriptorBinding;

    std::string getArrayName(const std::string &name)
    {
        if (name.ends_with("[0]"))
            return name.substr(0, name.size() - 3);
        return {};
    }

    std::string getElementName(const std::string &arrayName, int index)
    {
        return arrayName + "[" + std::to_string(index) + "]";
    }

    std::string removeInstanceName(std::string bufferName)
    {
        if (auto index = bufferName.find('.'); index != std::string::npos) {
            if (auto bracket = bufferName.find('[');
                bracket != std::string::npos) {
                auto copy = bufferName;
                copy.erase(index, bracket - index);
                return copy;
            }
            return bufferName.substr(0, index);
        }
        return bufferName;
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
        case GL_INT_VEC4:                    return Field::DataType::Int32;
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_ATOMIC_COUNTER:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:                   return Field::DataType::Uint32;
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
        case GL_FLOAT_MAT4x3:                return Field::DataType::Float;
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
        case GL_DOUBLE_MAT4x3:               return Field::DataType::Double;
        default:                             return {};
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

        return getTypeRows(type) * getTypeColumns(type)
            * getDataTypeSize(*dataType);
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
            switch (
                getComponentDataType(type).value_or(Field::DataType::Uint8)) {
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
        case GL_SAMPLER_2D_RECT:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_BUFFER:
        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_CUBE_MAP_ARRAY:
        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D_SHADOW:
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
        case GL_INT_IMAGE_1D:
        case GL_INT_IMAGE_2D:
        case GL_INT_IMAGE_3D:
        case GL_INT_IMAGE_2D_RECT:
        case GL_INT_IMAGE_CUBE:
        case GL_INT_IMAGE_BUFFER:
        case GL_INT_IMAGE_1D_ARRAY:
        case GL_INT_IMAGE_2D_ARRAY:
        case GL_INT_IMAGE_CUBE_MAP_ARRAY:
        case GL_INT_IMAGE_2D_MULTISAMPLE:
        case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_1D:
        case GL_UNSIGNED_INT_IMAGE_2D:
        case GL_UNSIGNED_INT_IMAGE_3D:
        case GL_UNSIGNED_INT_IMAGE_2D_RECT:
        case GL_UNSIGNED_INT_IMAGE_CUBE:
        case GL_UNSIGNED_INT_IMAGE_BUFFER:
        case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
            return SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        default: Q_ASSERT(!"not handled data type");
        }
        return {};
    }

    SpvReflectImageTraits getImageTraits(GLenum type)
    {
        enum Flag : uint32_t {
            None = 0,
            Sampled = 1 << 0,
            Arrayed = 2 << 1,
            MS = 1 << 2,
            Depth = 1 << 3,
        };
        const auto make = [](SpvDim dim, uint32_t flags = Flag::None) {
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
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_1D:             return make(SpvDim1D, Sampled);
          case GL_SAMPLER_2D_SHADOW:
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
        case GL_INT_IMAGE_1D:
        case GL_UNSIGNED_INT_IMAGE_1D:
        case GL_IMAGE_1D:                                return make(SpvDim1D);
        case GL_INT_IMAGE_2D:
        case GL_UNSIGNED_INT_IMAGE_2D:
        case GL_IMAGE_2D:                                return make(SpvDim2D);
        case GL_INT_IMAGE_3D:
        case GL_UNSIGNED_INT_IMAGE_3D:
        case GL_IMAGE_3D:                                return make(SpvDim3D);
        case GL_INT_IMAGE_2D_RECT:
        case GL_UNSIGNED_INT_IMAGE_2D_RECT:
        case GL_IMAGE_2D_RECT:                           return make(SpvDim2D);
        case GL_INT_IMAGE_CUBE:
        case GL_UNSIGNED_INT_IMAGE_CUBE:
        case GL_IMAGE_CUBE:                              return make(SpvDimCube);
        case GL_INT_IMAGE_BUFFER:
        case GL_UNSIGNED_INT_IMAGE_BUFFER:
        case GL_IMAGE_BUFFER:                            return make(SpvDimBuffer);
        case GL_INT_IMAGE_1D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
        case GL_IMAGE_1D_ARRAY:                          return make(SpvDim1D, Arrayed);
        case GL_INT_IMAGE_2D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
        case GL_IMAGE_2D_ARRAY:                          return make(SpvDim2D, Arrayed);
        case GL_INT_IMAGE_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
        case GL_IMAGE_CUBE_MAP_ARRAY:                    return make(SpvDimCube, Arrayed);
        case GL_INT_IMAGE_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
        case GL_IMAGE_2D_MULTISAMPLE:                    return make(SpvDim2D, MS);
        case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_IMAGE_2D_MULTISAMPLE_ARRAY:              return make(SpvDim2D, Arrayed | MS);
        default:                                         Q_ASSERT(!"unhandled type"); return {};
        }
    }

    SpvReflectDecorationFlags getDecorationFlags(bool isRowMajor)
    {
        auto flags = SpvReflectDecorationFlags{};
        if (isRowMajor)
            flags |= SPV_REFLECT_DECORATION_ROW_MAJOR;
        return flags;
    }

    auto splitArrayNameDims(const std::string &name)
        -> std::pair<std::string, SpvReflectArrayTraits>
    {
        const auto dot = name.rfind('.');
        auto baseName = std::string();
        auto array = SpvReflectArrayTraits{};
        auto begin = name.find('[', (dot != std::string::npos ? dot : 0));
        for (; begin != std::string::npos; begin = name.find('[', begin))
            if (const auto end = name.find(']', begin);
                end != std::string::npos) {
                const auto value = std::atoi(name.c_str() + begin + 1);
                array.dims[array.dims_count] = value + 1;
                if (array.dims_count == 0)
                    baseName = name.substr(0, begin);
                ++array.dims_count;
                begin = end + 1;
            }
        return { !baseName.empty() ? std::move(baseName) : name, array };
    }

    std::tuple<std::string, SpvReflectArrayTraits, std::string>
    splitNestedTypeName(const std::string &name)
    {
        const auto dot = name.rfind('.');
        if (dot == std::string::npos)
            return {};
        auto [typeName, array] = splitArrayNameDims(name.substr(0, dot));
        return { std::move(typeName), array, name.substr(dot + 1) };
    }

    BlockVariable createBlockVariable(const std::string &name, GLenum type,
        int arraySize, int offset, int arrayStride, int matrixStride,
        bool isRowMajor)
    {
        auto size = getSize(type);
        auto [baseName, array] = splitArrayNameDims(name);

        Q_ASSERT(arrayStride == 0 || array.dims_count > 0);
        if (arrayStride > 0 && array.dims_count > 0) {
            array.dims[array.dims_count - 1] = arraySize;
            array.stride = arrayStride;
            size = arraySize * arrayStride;
        }
        auto variable = BlockVariable{
            .name = baseName,
            .offset = static_cast<uint32_t>(offset),
            .size = static_cast<uint32_t>(size),
            .typeFlags = getTypeFlags(type, array.dims_count > 0),
            .decorationFlags = getDecorationFlags(isRowMajor),
            .array = array,
        };
        if (isOpaqueType(type)) {
            variable.image = getImageTraits(type);
        } else {
            variable.numeric = getNumericTraits(type);
        }
        return variable;
    }

    template <typename T, typename Tie, typename Merge>
    bool mergeAdjacentElements(std::vector<T> &elements, const Tie &tie,
        const Merge &merge)
    {
        auto merged = false;
        for (auto begin = elements.begin(); begin != elements.end(); ++begin) {
            const auto &first = tie(*begin);
            auto end = std::next(begin);
            for (; end != elements.end(); ++end)
                if (first != tie(*end))
                    break;
            if (std::distance(begin, end) > 1) {
                *begin = merge(std::span<T>(begin, end));
                elements.erase(std::next(begin), end);
                merged = true;
            }
        }
        return merged;
    }

    bool isArrayElementZero(const SpvReflectArrayTraits &array)
    {
        return *std::max_element(array.dims, array.dims + array.dims_count)
            <= 1;
    }

    void createBlockMemberNestedTypes(std::vector<BlockVariable> &members)
    {
        std::sort(members.begin(), members.end(),
            [](const auto &a, const auto &b) { return a.offset < b.offset; });
        // keep merging one level after the other
        while (mergeAdjacentElements(
            members,
            [](const auto &a) {
                // return something unique when not within a nested type
                if (a.name.find('.') == std::string::npos)
                    return std::to_string(reinterpret_cast<uintptr_t>(&a));
                return std::get<0>(splitNestedTypeName(a.name));
            },
            [](std::span<BlockVariable> members) {
                auto type = BlockVariable{
                    .typeFlags = SPV_REFLECT_TYPE_FLAG_STRUCT,
                };
                // first member of first element defines offset
                type.offset = members.front().offset;
                for (auto &member : members) {
                    auto [typeName, array, memberName] =
                        splitNestedTypeName(member.name);
                    type.name = std::move(typeName);
                    member.name = std::move(memberName);
                    member.offset -= type.offset;
                    if (isArrayElementZero(array)) {
                        // only add members of first element
                        type.array = array;
                        type.members.push_back(std::move(member));
                    } else if (!type.array.stride) {
                        // first member of second element defines stride
                        type.array = array;
                        type.array.stride = member.offset;
                    }
                }
                if (type.array.dims_count > 0) {
                    type.typeFlags |= SPV_REFLECT_TYPE_FLAG_ARRAY;
                    auto arrayElements = 1;
                    for (auto i = 0u; i < type.array.dims_count; ++i)
                        arrayElements *= type.array.dims[i];
                    type.size = type.array.stride * arrayElements;
                } else {
                    type.size = type.members.back().offset
                        + type.members.back().size;
                }
                return type;
            })) { }
    }

    void mergeBlockMemberArrays(std::vector<BlockVariable> &members)
    {
        std::sort(members.begin(), members.end(),
            [](const auto &a, const auto &b) { return a.offset < b.offset; });
        mergeAdjacentElements(
            members, [](const auto &a) { return std::tie(a.name); },
            [](std::span<BlockVariable> members) {
                auto merged = members.front();
                merged.array = members.back().array;
                merged.size = members.back().offset + members.back().size;
                return merged;
            });
    }

    void mergeDescriptorBindingArrays(std::vector<DescriptorBinding> &bindings)
    {
        mergeAdjacentElements(
            bindings,
            [](const auto &a) {
                return std::tie(a.descriptorType, a.name, a.typeName);
            },
            [](std::span<DescriptorBinding> members) {
                auto merged = members.front();
                merged.array = members.back().array;
                return merged;
            });
    }
} // namespace

void GLProgram::generateReflectionFromProgram(GLuint program,
    bool generateGlobalUniformBlockBinding)
{
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
        if (programInterface != GL_ATOMIC_COUNTER_BUFFER
            && programInterface != GL_TRANSFORM_FEEDBACK_BUFFER) {
            name.resize(
                getResourceValue(programInterface, index, GL_NAME_LENGTH) - 1);
            gl.glGetProgramResourceName(program, programInterface, index,
                static_cast<GLsizei>(name.size() + 1), nullptr, name.data());
        }
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

    auto nextTextureUnit = GLuint{};
    auto globalUniformBlockMembers = std::vector<BlockVariable>();
    forEachActiveResource(GL_UNIFORM,
        [&](GLuint programInterface, GLuint index, std::string name) {
            const auto [blockIndex, arraySize, type, location,
                atomicCounterBufferIndex,
                offset] = getResourceValues(programInterface, index,
                std::to_array<GLuint>({
                    GL_BLOCK_INDEX,
                    GL_ARRAY_SIZE,
                    GL_TYPE,
                    GL_LOCATION,
                    GL_ATOMIC_COUNTER_BUFFER_INDEX,
                    GL_OFFSET,
                }));

            Q_ASSERT(type);
            const auto arrayName = getArrayName(name);
            const auto baseName = (arrayName.empty() ? name : arrayName);

            if (atomicCounterBufferIndex >= 0) {
                reflection->descriptorsBindings.push_back(DescriptorBinding{
                    .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .typeName = baseName,
                    .block = {
                        .members = {
                            createBlockVariable(name, type, arraySize, offset, 0, 0, false),
                        },
                    },
                });
                mDescriptorBindingPoints[QString::fromStdString(baseName)] = {
                    GL_ATOMIC_COUNTER_BUFFER,
                    static_cast<uint32_t>(atomicCounterBufferIndex),
                };
            } else if (isOpaqueType(type)) {
                Q_ASSERT(location >= 0);
                const auto bindingPoint = nextTextureUnit;
                nextTextureUnit += arraySize;

                reflection->descriptorsBindings.push_back({
                    .descriptorType = getDescriptorType(type),
                    .name = baseName,
                    .image = getImageTraits(type),
                    .array = { 
                        .dims_count = (arraySize > 0 ? 1u : 0u),
                        .dims = { static_cast<uint32_t>(arraySize) },
                    },
                    .binding = bindingPoint,
                });
                mDescriptorBindingPoints[baseName.c_str()] = {
                    GL_TEXTURE,
                    static_cast<GLuint>(location),
                };
            } else if (blockIndex == -1) {
                Q_ASSERT(location >= 0);
                mUniforms.push_back({
                    .name = baseName.c_str(),
                    .location = location,
                    .dataType = static_cast<GLenum>(type),
                    .arraySize = arraySize,
                });
                if (!arrayName.empty())
                    for (auto i = 0; i < arraySize; ++i)
                        mUniforms.push_back({
                            .name = getElementName(arrayName, i).c_str(),
                            .location = location + i,
                            .dataType = static_cast<GLenum>(type),
                            .arraySize = 1,
                        });

                if (generateGlobalUniformBlockBinding)
                    globalUniformBlockMembers.push_back(
                        createBlockVariable(baseName, type, 0, 0, 0, 0, false));
            }
        });

    if (!globalUniformBlockMembers.empty())
        reflection->descriptorsBindings.push_back(DescriptorBinding{
            .descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .typeName = globalUniformBlockName,
            .block = {
                .members = std::move(globalUniformBlockMembers),
            },
        });

    auto nextUniformBlockBindingPoint = GLuint{};
    auto nextShaderStorageBindingPoint = GLuint{};
    for (auto programInterface : { GL_UNIFORM_BLOCK, GL_SHADER_STORAGE_BLOCK })
        forEachActiveResource(programInterface,
            [&](GLuint programInterface, GLuint index,
                const std::string &bufferName) {
                const auto [bufferBinding, bufferDataSize] =
                    getResourceValues(programInterface, index,
                        std::to_array<GLuint>({
                            GL_BUFFER_BINDING,
                            GL_BUFFER_DATA_SIZE,
                        }));

                auto [baseName, array] = splitArrayNameDims(bufferName);

                auto descriptorType = SpvReflectDescriptorType{};
                auto memberProgramInterface = GLenum{};
                if (programInterface == GL_UNIFORM_BLOCK) {
                    descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    memberProgramInterface = GL_UNIFORM;
                } else {
                    descriptorType = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    memberProgramInterface = GL_BUFFER_VARIABLE;
                }

                auto members = std::vector<BlockVariable>();
                forEachActiveVariable(programInterface, index,
                    memberProgramInterface,
                    [&](GLuint programInterface, GLuint index,
                        std::string name) {
                        const auto [type, arraySize, offset, arrayStride,
                            matrixStride, isRowMajor] =
                            getResourceValues(programInterface, index,
                                std::to_array<GLuint>({
                                    GL_TYPE,
                                    GL_ARRAY_SIZE,
                                    GL_OFFSET,
                                    GL_ARRAY_STRIDE,
                                    GL_MATRIX_STRIDE,
                                    GL_IS_ROW_MAJOR,
                                }));

                        members.push_back(createBlockVariable(
                            removePrefix(name, baseName), type, arraySize,
                            offset, arrayStride, matrixStride, isRowMajor));
                    });

                // glslang generates block_name.instance_name[N] instead of block_name[N]
                baseName = removeInstanceName(std::move(baseName));

                auto bindingPoint = 0u;
                if (programInterface == GL_UNIFORM_BLOCK) {
                    bindingPoint = nextUniformBlockBindingPoint++;
                    gl.glUniformBlockBinding(mProgramObject, index,
                        bindingPoint);
                    mDescriptorBindingPoints[QString::fromStdString(
                        baseName)] = { GL_UNIFORM_BUFFER, bindingPoint };
                } else {
                    bindingPoint = nextShaderStorageBindingPoint++;
                    gl.glShaderStorageBlockBinding(mProgramObject, index,
                        bindingPoint);
                    mDescriptorBindingPoints[QString::fromStdString(
                        baseName)] = { GL_SHADER_STORAGE_BUFFER, bindingPoint };
                    // TODO: GL_TOP_LEVEL_ARRAY_SIZE, GL_TOP_LEVEL_ARRAY_STRIDE
                }

                reflection->descriptorsBindings.push_back(DescriptorBinding{
                  .descriptorType = descriptorType,
                  .typeName = std::move(baseName),
                  .array = array,
                  .block = {
                      .size = static_cast<uint32_t>(bufferDataSize),
                      .members = std::move(members),
                  },
                  .binding = bindingPoint,
              });
            });

    for (auto &descriptorBinding : reflection->descriptorsBindings) {
        mergeBlockMemberArrays(descriptorBinding.block.members);
        createBlockMemberNestedTypes(descriptorBinding.block.members);
    }
    mergeDescriptorBindingArrays(reflection->descriptorsBindings);

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

                auto &variables = (programInterface == GL_PROGRAM_INPUT
                        ? reflection->inputs
                        : reflection->outputs);
                variables.push_back(InterfaceVariable{
                    .name = name,
                    .semantic = inputSemantics[name],
                    .builtIn = (name.starts_with("gl_") ? 0 : -1),
                    .format = getInputFormat(type),
                    .location = static_cast<GLuint>(location),
                });
            });

    // WORKAROUND: programs with mesh shaders should not enumerate any inputs
    if (std::count_if(mShaders.begin(), mShaders.end(),
            [](const auto &s) { return s.type() == Shader::ShaderType::Mesh; }))
        reflection->inputs.clear();

    mReflection = Reflection(std::move(reflection));
    Q_ASSERT(glGetError() == GL_NO_ERROR);
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

auto GLProgram::getDescriptorBindingPoint(
    const SpvReflectDescriptorBinding &desc, int arrayIndex) const
    -> BindingPoint
{
    Q_ASSERT(desc.name && desc.type_description
        && desc.type_description->type_name);
    const auto it = mDescriptorBindingPoints.find(
        *desc.name ? desc.name : desc.type_description->type_name);
    if (it != mDescriptorBindingPoints.end()) {
        auto [target, bindingPoint] = it->second;
        return { target, bindingPoint + arrayIndex };
    }
    Q_ASSERT(!"binding without assigned binding point");
    return { GL_NONE, 0 };
}
