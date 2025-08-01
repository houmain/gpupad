#include "Item.h"
#include "ItemEnums.h"
#include <QMetaEnum>

Item::Type getItemTypeByName(const QString &name, bool *ok)
{
    const auto metaType = QMetaEnum::fromType<ItemEnums::ItemType>();
    const auto index = metaType.keyToValue(qUtf8Printable(name));
    if (ok)
        *ok = (index >= 0);
    return (index < 0 ? Item::Type::Group : static_cast<Item::Type>(index));
}

int getFieldSize(const Field &field)
{
    switch (field.dataType) {
    case Field::DataType::Int8:   return 1;
    case Field::DataType::Int16:  return 2;
    case Field::DataType::Int32:  return 4;
    case Field::DataType::Int64:  return 8;
    case Field::DataType::Uint8:  return 1;
    case Field::DataType::Uint16: return 2;
    case Field::DataType::Uint32: return 4;
    case Field::DataType::Uint64: return 8;
    case Field::DataType::Float:  return 4;
    case Field::DataType::Double: return 8;
    }
    return 0;
}

int getFieldRowOffset(const Field &field)
{
    auto offset = 0;
    const auto &block = *static_cast<const Block *>(field.parent);
    for (const Item *item : block.items) {
        if (&field == item)
            break;
        const auto &f = *static_cast<const Field *>(item);
        offset += f.count * getFieldSize(f) + f.padding;
    }
    return offset;
}

int getBlockStride(const Block &block)
{
    auto stride = 0;
    for (const Item *item : block.items) {
        const auto &field = *static_cast<const Field *>(item);
        stride += field.count * getFieldSize(field) + field.padding;
    }
    return stride;
}

TextureKind getKind(const Texture &texture)
{
    auto kind = TextureKind{};

    switch (texture.target) {
    case QOpenGLTexture::Target1D:
    case QOpenGLTexture::Target1DArray: kind.dimensions = 1; break;
    case QOpenGLTexture::Target3D:      kind.dimensions = 3; break;
    default:                            kind.dimensions = 2;
    }

    switch (texture.target) {
    case QOpenGLTexture::Target1DArray:
    case QOpenGLTexture::Target2DArray:
    case QOpenGLTexture::TargetCubeMapArray:
    case QOpenGLTexture::Target2DMultisampleArray: kind.array = true; break;
    default:                                       break;
    }

    switch (texture.target) {
    case QOpenGLTexture::TargetCubeMap:
    case QOpenGLTexture::TargetCubeMapArray: kind.cubeMap = true; break;
    default:                                 break;
    }

    switch (texture.format) {
    case QOpenGLTexture::D16:
    case QOpenGLTexture::D24:
    case QOpenGLTexture::D32:
    case QOpenGLTexture::D32F:      kind.depth = true; break;
    case QOpenGLTexture::D24S8:
    case QOpenGLTexture::D32FS8X24: kind.depth = kind.stencil = true; break;
    case QOpenGLTexture::S8:        kind.stencil = true; break;
    default:                        kind.color = true;
    }
    return kind;
}

CallKind getKind(const Call &call)
{
    auto kind = CallKind{};

    switch (call.callType) {
    case Call::CallType::Draw:         kind.draw = true; break;
    case Call::CallType::DrawIndexed:  kind.draw = kind.indexed = true; break;
    case Call::CallType::DrawIndirect: kind.draw = kind.indirect = true; break;
    case Call::CallType::DrawIndexedIndirect:
        kind.draw = kind.indirect = kind.indexed = true;
        break;
    case Call::CallType::DrawMeshTasks: kind.draw = kind.mesh = true; break;
    case Call::CallType::DrawMeshTasksIndirect:
        kind.draw = kind.mesh = kind.indirect = true;
        break;
    case Call::CallType::Compute: kind.compute = true; break;
    case Call::CallType::ComputeIndirect:
        kind.compute = kind.indirect = true;
        break;
    case Call::CallType::TraceRays: kind.trace = true; break;
    default:                        break;
    }

    if (call.primitiveType == Call::PrimitiveType::Patches)
        kind.patches = true;

    return kind;
}

bool callTypeSupportsShaderType(Call::CallType callType,
    Shader::ShaderType shaderType)
{
    if (shaderType == Shader::ShaderType::Includable)
        return true;

    switch (callType) {
    case Call::CallType::Draw:
    case Call::CallType::DrawIndexed:
    case Call::CallType::DrawIndirect:
    case Call::CallType::DrawIndexedIndirect:
        switch (shaderType) {
        case Shader::ShaderType::Vertex:
        case Shader::ShaderType::Fragment:
        case Shader::ShaderType::Geometry:
        case Shader::ShaderType::TessControl:
        case Shader::ShaderType::TessEvaluation: return true;
        default:                                 return false;
        }
    case Call::CallType::DrawMeshTasks:
    case Call::CallType::DrawMeshTasksIndirect:
        switch (shaderType) {
        case Shader::ShaderType::Task:
        case Shader::ShaderType::Mesh:
        case Shader::ShaderType::Fragment: return true;
        default:                           return false;
        }
    case Call::CallType::Compute:
    case Call::CallType::ComputeIndirect:
        return (shaderType == Shader::ShaderType::Compute);

    case Call::CallType::TraceRays:
        switch (shaderType) {
        case Shader::ShaderType::RayGeneration:
        case Shader::ShaderType::RayIntersection:
        case Shader::ShaderType::RayAnyHit:
        case Shader::ShaderType::RayClosestHit:
        case Shader::ShaderType::RayMiss:
        case Shader::ShaderType::RayCallable:     return true;
        default:                                  return false;
        }
    default: return false;
    }
}

bool shouldExecute(Call::ExecuteOn executeOn, EvaluationType evaluationType)
{
    switch (executeOn) {
    case Call::ExecuteOn::ResetEvaluation:
        return (evaluationType == EvaluationType::Reset);

    case Call::ExecuteOn::ManualEvaluation:
        return (evaluationType == EvaluationType::Reset
            || evaluationType == EvaluationType::Manual);

    case Call::ExecuteOn::EveryEvaluation: break;
    }
    return true;
}
