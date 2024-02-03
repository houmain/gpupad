
#include "KDGpuEnums.h"

KDGpu::Format toKDGpu(QOpenGLTexture::TextureFormat format)
{
    using E = QOpenGLTexture::TextureFormat;
    using KD = KDGpu::Format;
    switch (format) {
        case E::R8_UNorm              : return KD::R8_UNORM;
        case E::RG8_UNorm             : return KD::R8G8_UNORM;
        case E::RGB8_UNorm            : return KD::R8G8B8_UNORM;
        case E::RGBA8_UNorm           : return KD::R8G8B8A8_UNORM;
        case E::R16_UNorm             : return KD::R16_UNORM;
        case E::RG16_UNorm            : return KD::R16G16_UNORM;
        case E::RGB16_UNorm           : return KD::R16G16B16_UNORM;
        case E::RGBA16_UNorm          : return KD::R16G16B16A16_UNORM;
        case E::R8_SNorm              : return KD::R8_SNORM;
        case E::RG8_SNorm             : return KD::R8G8_SNORM;
        case E::RGB8_SNorm            : return KD::R8G8B8_SNORM;
        case E::RGBA8_SNorm           : return KD::R8G8B8A8_SNORM;
        case E::R16_SNorm             : return KD::R16_SNORM;
        case E::RG16_SNorm            : return KD::R16G16_SNORM;
        case E::RGB16_SNorm           : return KD::R16G16B16_SNORM;
        case E::RGBA16_SNorm          : return KD::R16G16B16A16_SNORM;
        case E::R8U                   : return KD::R8_UINT;
        case E::RG8U                  : return KD::R8G8_UINT;
        case E::RGB8U                 : return KD::R8G8B8_UINT;
        case E::RGBA8U                : return KD::R8G8B8A8_UINT;
        case E::R16U                  : return KD::R16_UINT;
        case E::RG16U                 : return KD::R16G16_UINT;
        case E::RGB16U                : return KD::R16G16B16_UINT;
        case E::RGBA16U               : return KD::R16G16B16A16_UINT;
        case E::R32U                  : return KD::R32_UINT;
        case E::RG32U                 : return KD::R32G32_UINT;
        case E::RGB32U                : return KD::R32G32B32_UINT;
        case E::RGBA32U               : return KD::R32G32B32A32_UINT;
        case E::R8I                   : return KD::R8_SINT;
        case E::RG8I                  : return KD::R8G8_SINT;
        case E::RGB8I                 : return KD::R8G8B8_SINT;
        case E::RGBA8I                : return KD::R8G8B8A8_SINT;
        case E::R16I                  : return KD::R16_SINT;
        case E::RG16I                 : return KD::R16G16_SINT;
        case E::RGB16I                : return KD::R16G16B16_SINT;
        case E::RGBA16I               : return KD::R16G16B16A16_SINT;
        case E::R32I                  : return KD::R32_SINT;
        case E::RG32I                 : return KD::R32G32_SINT;
        case E::RGB32I                : return KD::R32G32B32_SINT;
        case E::RGBA32I               : return KD::R32G32B32A32_SINT;
        case E::R16F                  : return KD::R16_SFLOAT;
        case E::RG16F                 : return KD::R16G16_SFLOAT;
        case E::RGB16F                : return KD::R16G16B16_SFLOAT;
        case E::RGBA16F               : return KD::R16G16B16A16_SFLOAT;
        case E::R32F                  : return KD::R32_SFLOAT;
        case E::RG32F                 : return KD::R32G32_SFLOAT;
        case E::RGB32F                : return KD::R32G32B32_SFLOAT;
        case E::RGBA32F               : return KD::R32G32B32A32_SFLOAT;
        case E::RGB9E5                : return KD::E5B9G9R9_UFLOAT_PACK32;
        case E::RG11B10F              : return KD::B10G11R11_UFLOAT_PACK32;
        //case E::RG3B2                 : return KD::PACK8;
        case E::R5G6B5                : return KD::R5G6B5_UNORM_PACK16;
        case E::RGB5A1                : return KD::A1R5G5B5_UNORM_PACK16;
        case E::RGBA4                 : return KD::R4G4B4A4_UNORM_PACK16;
        case E::RGB10A2               : return KD::A2B10G10R10_UNORM_PACK32;
        case E::D16                   : return KD::D16_UNORM;
        case E::D24                   : return KD::X8_D24_UNORM_PACK32;
        case E::D24S8                 : return KD::D24_UNORM_S8_UINT;
        case E::D32                   : return KD::D32_SFLOAT;
        case E::D32F                  : return KD::D32_SFLOAT;
        case E::D32FS8X24             : return KD::D32_SFLOAT_S8_UINT;
        case E::S8                    : return KD::S8_UINT;
        case E::RGB_DXT1              : return KD::BC1_RGB_UNORM_BLOCK;
        case E::RGBA_DXT1             : return KD::BC1_RGBA_UNORM_BLOCK;
        case E::RGBA_DXT3             : return KD::BC2_UNORM_BLOCK;
        case E::RGBA_DXT5             : return KD::BC3_UNORM_BLOCK;
        case E::R_ATI1N_UNorm         : return KD::BC4_UNORM_BLOCK;
        case E::R_ATI1N_SNorm         : return KD::BC4_SNORM_BLOCK;
        case E::RG_ATI2N_UNorm        : return KD::BC5_UNORM_BLOCK;
        case E::RG_ATI2N_SNorm        : return KD::BC5_SNORM_BLOCK;
        case E::RGB_BP_UNSIGNED_FLOAT : return KD::BC6H_UFLOAT_BLOCK;
        case E::RGB_BP_SIGNED_FLOAT   : return KD::BC6H_SFLOAT_BLOCK;
        case E::RGB_BP_UNorm          : return KD::BC7_UNORM_BLOCK;
        case E::SRGB8                 : return KD::R8G8B8_SRGB;
        case E::SRGB8_Alpha8          : return KD::R8G8B8A8_SRGB;
        case E::SRGB_DXT1             : return KD::BC1_RGBA_SRGB_BLOCK;
        case E::SRGB_Alpha_DXT1       : return KD::BC1_RGB_SRGB_BLOCK;
        case E::SRGB_Alpha_DXT3       : return KD::BC2_SRGB_BLOCK;
        case E::SRGB_Alpha_DXT5       : return KD::BC3_SRGB_BLOCK;
        case E::SRGB_BP_UNorm         : return KD::BC7_SRGB_BLOCK;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::Format toKDGpu(Field::DataType dataType, int count)
{
    using KD = KDGpu::Format;

#define ADD(DATATYPE, BITS, TYPE) case DATATYPE: switch(count) { \
        case 1: return KD::R##BITS##_##TYPE; \
        case 2: return KD::R##BITS##G##BITS##_##TYPE; \
        case 3: return KD::R##BITS##G##BITS##B##BITS##_##TYPE; \
        case 4: return KD::R##BITS##G##BITS##B##BITS##A##BITS##_##TYPE; \
    }
    switch (dataType) {
        ADD(Field::DataType::Int8, 8, SINT);
        ADD(Field::DataType::Int16, 16, SINT);
        ADD(Field::DataType::Int32, 32, SINT);
        ADD(Field::DataType::Uint8, 8, UINT);
        ADD(Field::DataType::Uint16, 16, UINT);
        ADD(Field::DataType::Uint32, 32, UINT);
        ADD(Field::DataType::Float, 32, SFLOAT);
    }
#undef ADD
    Q_UNREACHABLE();
    return { };
}

KDGpu::PrimitiveTopology toKDGpu(Call::PrimitiveType primitiveType)
{
    using E = Call::PrimitiveType;
    using KD = KDGpu::PrimitiveTopology;
    switch (primitiveType) {
        case E::Points: return KD::PointList;
        case E::LineStrip: return KD::LineStrip;
        case E::Lines: return KD::LineList;
        case E::LineStripAdjacency: return KD::LineStripWithAdjacency;
        case E::LinesAdjacency: return KD::LineListWithAdjacency;
        case E::TriangleStrip: return KD::TriangleStrip;
        case E::TriangleFan: return KD::TriangleFan;
        case E::Triangles: return KD::TriangleList;
        case E::TriangleStripAdjacency: return KD::TriangleStripWithAdjacency;
        case E::TrianglesAdjacency: return KD::TriangleListWithAdjacency;
        case E::Patches: return KD::PatchList;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::CullModeFlags toKDGpu(Target::CullMode cullMode)
{
    using E = Target::CullMode;
    using KD = KDGpu::CullModeFlagBits;
    switch (cullMode) {
        case E::NoCulling: return KD::None;
        case E::Front: return KD::FrontBit;
        case E::Back: return KD::BackBit;
        case E::FrontAndBack: return KD::FrontAndBack;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::FrontFace toKDGpu(Target::FrontFace frontFace)
{
    // TODO: fix winding order issue
    // https://stackoverflow.com/questions/58753504/vulkan-front-face-winding-order
    switch (frontFace) {
        case Target::FrontFace::CW: return KDGpu::FrontFace::CounterClockwise;
        case Target::FrontFace::CCW: return KDGpu::FrontFace::Clockwise;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::PolygonMode toKDGpu(Target::PolygonMode polygonMode)
{
    switch (polygonMode) {
        case Target::PolygonMode::Fill: return KDGpu::PolygonMode::Fill;
        case Target::PolygonMode::Line: return KDGpu::PolygonMode::Line;
        case Target::PolygonMode::Point: return KDGpu::PolygonMode::Point;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::BlendOperation toKDGpu(Attachment::BlendEquation eq)
{
    using E = Attachment::BlendEquation;
    using KD = KDGpu::BlendOperation;
    switch (eq) {
        case E::Add: return KD::Add;
        case E::Subtract: return KD::Subtract;
        case E::ReverseSubtract: return KD::ReverseSubtract;
        case E::Min: return KD::Min;
        case E::Max: return KD::Max;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::BlendFactor toKDGpu(Attachment::BlendFactor factor)
{
    using E = Attachment::BlendFactor;
    using KD = KDGpu::BlendFactor;
    switch (factor) {
        case E::Zero: return KD::Zero;
        case E::One: return KD::One;
        case E::SrcColor: return KD::SrcColor;
        case E::OneMinusSrcColor: return KD::OneMinusSrcColor;
        case E::DstColor: return KD::DstColor;
        case E::OneMinusDstColor: return KD::OneMinusDstColor;
        case E::SrcAlpha: return KD::SrcAlpha;
        case E::OneMinusSrcAlpha: return KD::OneMinusSrcAlpha;
        case E::DstAlpha: return KD::DstAlpha;
        case E::OneMinusDstAlpha: return KD::OneMinusDstAlpha;
        case E::ConstantColor: return KD::ConstantColor;
        case E::OneMinusConstantColor: return KD::OneMinusConstantColor;
        case E::ConstantAlpha: return KD::ConstantAlpha;
        case E::OneMinusConstantAlpha: return KD::OneMinusConstantAlpha;
        case E::SrcAlphaSaturate: return KD::SrcAlphaSaturate;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::CompareOperation toKDGpu(Attachment::ComparisonFunc func)
{
    using E = Attachment::ComparisonFunc;
    using KD = KDGpu::CompareOperation;
    switch (func) {
        case E::Never: return KD::Never;
        case E::Less: return KD::Less;
        case E::Equal: return KD::Equal;
        case E::LessEqual: return KD::LessOrEqual;
        case E::Greater: return KD::Greater;
        case E::NotEqual: return KD::NotEqual;
        case E::GreaterEqual: return KD::GreaterOrEqual;
        case E::Always: return KD::Always;
    }
    Q_UNREACHABLE();
    return { };
}

KDGpu::StencilOperation toKDGpu(Attachment::StencilOperation op)
{
    using E = Attachment::StencilOperation;
    using KD = KDGpu::StencilOperation;
    switch (op) {
        case E::Keep: return KD::Keep;
        case E::Zero: return KD::Zero;
        case E::Replace: return KD::Replace;
        case E::Increment: return KD::IncrementAndClamp;
        case E::Decrement: return KD::DecrementAndClamp;
        case E::Invert: return KD::Invert;
        case E::IncrementWrap: return KD::IncrementAndWrap;
        case E::DecrementWrap: return KD::DecrementAndWrap;
    }
    Q_UNREACHABLE();
    return { };
}
