
#include "D3DEnums.h"

DXGI_FORMAT toDXGIFormat(Texture::Format format)
{
    using TF = Texture::Format;
    switch (format) {
    case TF::R8_UNorm:              return DXGI_FORMAT_R8_UNORM;
    case TF::RG8_UNorm:             return DXGI_FORMAT_R8G8_UNORM;
    //case TF::RGB8_UNorm:
    case TF::RGBA8_UNorm:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TF::R16_UNorm:             return DXGI_FORMAT_R16_UNORM;
    case TF::RG16_UNorm:            return DXGI_FORMAT_R16G16_UNORM;
    //case TF::RGB16_UNorm
    case TF::RGBA16_UNorm:          return DXGI_FORMAT_R16G16B16A16_UNORM;
    case TF::R8_SNorm:              return DXGI_FORMAT_R8_SNORM;
    case TF::RG8_SNorm:             return DXGI_FORMAT_R8G8_SNORM;
    //case TF::RGB8_SNorm:
    case TF::RGBA8_SNorm:           return DXGI_FORMAT_R8G8B8A8_SNORM;
    case TF::R16_SNorm:             return DXGI_FORMAT_R16_SNORM;
    case TF::RG16_SNorm:            return DXGI_FORMAT_R16G16_SNORM;
    //case TF::RGB16_SNorm:
    case TF::RGBA16_SNorm:          return DXGI_FORMAT_R16_SNORM;
    case TF::R8U:                   return DXGI_FORMAT_R8_UINT;
    case TF::RG8U:                  return DXGI_FORMAT_R8G8_UINT;
    //case TF::RGB8U:
    case TF::RGBA8U:                return DXGI_FORMAT_R8G8B8A8_UINT;
    case TF::R16U:                  return DXGI_FORMAT_R16_UINT;
    case TF::RG16U:                 return DXGI_FORMAT_R16G16_UINT;
    //case TF::RGB16U:
    case TF::RGBA16U:               return DXGI_FORMAT_R16G16B16A16_UINT;
    case TF::R32U:                  return DXGI_FORMAT_R32_UINT;
    case TF::RG32U:                 return DXGI_FORMAT_R32G32_UINT;
    case TF::RGB32U:                return DXGI_FORMAT_R32G32B32_UINT;
    case TF::RGBA32U:               return DXGI_FORMAT_R32G32B32A32_UINT;
    case TF::R8I:                   return DXGI_FORMAT_R8_SINT;
    case TF::RG8I:                  return DXGI_FORMAT_R8G8_SINT;
    //case TF::RGB8I:
    case TF::RGBA8I:                return DXGI_FORMAT_R8G8B8A8_SINT;
    case TF::R16I:                  return DXGI_FORMAT_R16_SINT;
    case TF::RG16I:                 return DXGI_FORMAT_R16G16_SINT;
    //case TF::RGB16I:
    case TF::RGBA16I:               return DXGI_FORMAT_R16G16B16A16_SINT;
    case TF::R32I:                  return DXGI_FORMAT_R32_SINT;
    case TF::RG32I:                 return DXGI_FORMAT_R32G32_SINT;
    case TF::RGB32I:                return DXGI_FORMAT_R32G32B32_SINT;
    case TF::RGBA32I:               return DXGI_FORMAT_R32G32B32A32_SINT;
    case TF::R16F:                  return DXGI_FORMAT_R16_FLOAT;
    case TF::RG16F:                 return DXGI_FORMAT_R16G16_FLOAT;
    //case TF::RGB16F:
    case TF::RGBA16F:               return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TF::R32F:                  return DXGI_FORMAT_R32_FLOAT;
    case TF::RG32F:                 return DXGI_FORMAT_R32G32_FLOAT;
    case TF::RGB32F:                return DXGI_FORMAT_R32G32B32_FLOAT;
    case TF::RGBA32F:               return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TF::RGB9E5:                return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    case TF::RG11B10F:              return DXGI_FORMAT_R11G11B10_FLOAT;
    case TF::RG3B2:
    case TF::R5G6B5:                return DXGI_FORMAT_B5G6R5_UNORM;
    case TF::RGB5A1:                return DXGI_FORMAT_B5G5R5A1_UNORM;
    case TF::RGBA4:                 return DXGI_FORMAT_B4G4R4A4_UNORM;
    case TF::RGB10A2:               return DXGI_FORMAT_R10G10B10A2_UNORM;
    case TF::D16:                   return DXGI_FORMAT_D16_UNORM;
    //case TF::D24:
    case TF::D24S8:                 return DXGI_FORMAT_D24_UNORM_S8_UINT;
    //case TF::D32:
    case TF::D32F:                  return DXGI_FORMAT_D32_FLOAT;
    case TF::D32FS8X24:             return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    //case TF::S8:
    case TF::RGB_DXT1:
    case TF::RGBA_DXT1:             return DXGI_FORMAT_BC1_UNORM;
    case TF::RGBA_DXT3:             return DXGI_FORMAT_BC2_UNORM;
    case TF::RGBA_DXT5:             return DXGI_FORMAT_BC3_UNORM;
    case TF::R_ATI1N_UNorm:         return DXGI_FORMAT_BC4_UNORM;
    case TF::R_ATI1N_SNorm:         return DXGI_FORMAT_BC4_SNORM;
    case TF::RG_ATI2N_UNorm:        return DXGI_FORMAT_BC5_UNORM;
    case TF::RG_ATI2N_SNorm:        return DXGI_FORMAT_BC5_SNORM;
    case TF::RGB_BP_UNSIGNED_FLOAT: return DXGI_FORMAT_BC6H_UF16;
    case TF::RGB_BP_SIGNED_FLOAT:   return DXGI_FORMAT_BC6H_SF16;
    case TF::RGB_BP_UNorm:          return DXGI_FORMAT_BC7_UNORM;
    //case TF::SRGB8:
    case TF::SRGB8_Alpha8:          return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TF::SRGB_DXT1:             return DXGI_FORMAT_BC1_UNORM_SRGB;
    case TF::SRGB_Alpha_DXT1:       return DXGI_FORMAT_BC1_UNORM_SRGB;
    case TF::SRGB_Alpha_DXT3:       return DXGI_FORMAT_BC2_UNORM_SRGB;
    case TF::SRGB_Alpha_DXT5:       return DXGI_FORMAT_BC3_UNORM_SRGB;
    case TF::SRGB_BP_UNorm:         return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:                        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT toDXGITypelessFormat(Texture::Format format)
{
    using TF = Texture::Format;
    switch (format) {
    case TF::R8_UNorm:              return DXGI_FORMAT_R8_TYPELESS;
    case TF::RG8_UNorm:             return DXGI_FORMAT_R8G8_TYPELESS;
    //case TF::RGB8_UNorm:
    case TF::RGBA8_UNorm:           return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case TF::R16_UNorm:             return DXGI_FORMAT_R16_TYPELESS;
    case TF::RG16_UNorm:            return DXGI_FORMAT_R16G16_TYPELESS;
    //case TF::RGB16_UNorm
    case TF::RGBA16_UNorm:          return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case TF::R8_SNorm:              return DXGI_FORMAT_R8_TYPELESS;
    case TF::RG8_SNorm:             return DXGI_FORMAT_R8G8_TYPELESS;
    //case TF::RGB8_SNorm:
    case TF::RGBA8_SNorm:           return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case TF::R16_SNorm:             return DXGI_FORMAT_R16_TYPELESS;
    case TF::RG16_SNorm:            return DXGI_FORMAT_R16G16_TYPELESS;
    //case TF::RGB16_SNorm:
    case TF::RGBA16_SNorm:          return DXGI_FORMAT_R16_TYPELESS;
    case TF::R8U:                   return DXGI_FORMAT_R8_TYPELESS;
    case TF::RG8U:                  return DXGI_FORMAT_R8G8_TYPELESS;
    //case TF::RGB8U:
    case TF::RGBA8U:                return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case TF::R16U:                  return DXGI_FORMAT_R16_TYPELESS;
    case TF::RG16U:                 return DXGI_FORMAT_R16G16_TYPELESS;
    //case TF::RGB16U:
    case TF::RGBA16U:               return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case TF::R32U:                  return DXGI_FORMAT_R32_TYPELESS;
    case TF::RG32U:                 return DXGI_FORMAT_R32G32_TYPELESS;
    case TF::RGB32U:                return DXGI_FORMAT_R32G32B32_TYPELESS;
    case TF::RGBA32U:               return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case TF::R8I:                   return DXGI_FORMAT_R8_TYPELESS;
    case TF::RG8I:                  return DXGI_FORMAT_R8G8_TYPELESS;
    //case TF::RGB8I:
    case TF::RGBA8I:                return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case TF::R16I:                  return DXGI_FORMAT_R16_TYPELESS;
    case TF::RG16I:                 return DXGI_FORMAT_R16G16_TYPELESS;
    //case TF::RGB16I:
    case TF::RGBA16I:               return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case TF::R32I:                  return DXGI_FORMAT_R32_TYPELESS;
    case TF::RG32I:                 return DXGI_FORMAT_R32G32_TYPELESS;
    case TF::RGB32I:                return DXGI_FORMAT_R32G32B32_TYPELESS;
    case TF::RGBA32I:               return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case TF::R16F:                  return DXGI_FORMAT_R16_TYPELESS;
    case TF::RG16F:                 return DXGI_FORMAT_R16G16_TYPELESS;
    //case TF::RGB16F:
    case TF::RGBA16F:               return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case TF::R32F:                  return DXGI_FORMAT_R32_TYPELESS;
    case TF::RG32F:                 return DXGI_FORMAT_R32G32_TYPELESS;
    case TF::RGB32F:                return DXGI_FORMAT_R32G32B32_TYPELESS;
    case TF::RGBA32F:               return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    //case TF::RGB9E5:
    //case TF::RG11B10F:
    //case TF::RG3B2:
    //case TF::R5G6B5:
    //case TF::RGB5A1:
    //case TF::RGBA4:
    case TF::RGB10A2:               return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    //case TF::D16:
    //case TF::D24:
    //case TF::D24S8:
    //case TF::D32:
    //case TF::D32F:
    //case TF::D32FS8X24:
    //case TF::S8:
    case TF::RGB_DXT1:
    case TF::RGBA_DXT1:             return DXGI_FORMAT_BC1_TYPELESS;
    case TF::RGBA_DXT3:             return DXGI_FORMAT_BC2_TYPELESS;
    case TF::RGBA_DXT5:             return DXGI_FORMAT_BC3_TYPELESS;
    case TF::R_ATI1N_UNorm:         return DXGI_FORMAT_BC4_TYPELESS;
    case TF::R_ATI1N_SNorm:         return DXGI_FORMAT_BC4_TYPELESS;
    case TF::RG_ATI2N_UNorm:        return DXGI_FORMAT_BC5_TYPELESS;
    case TF::RG_ATI2N_SNorm:        return DXGI_FORMAT_BC5_TYPELESS;
    case TF::RGB_BP_UNSIGNED_FLOAT:
    case TF::RGB_BP_SIGNED_FLOAT:   return DXGI_FORMAT_BC6H_TYPELESS;
    case TF::RGB_BP_UNorm:          return DXGI_FORMAT_BC7_TYPELESS;
    case TF::SRGB8:
    case TF::SRGB8_Alpha8:          return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case TF::SRGB_DXT1:             return DXGI_FORMAT_BC1_TYPELESS;
    case TF::SRGB_Alpha_DXT1:       return DXGI_FORMAT_BC1_TYPELESS;
    case TF::SRGB_Alpha_DXT3:       return DXGI_FORMAT_BC2_TYPELESS;
    case TF::SRGB_Alpha_DXT5:       return DXGI_FORMAT_BC3_TYPELESS;
    case TF::SRGB_BP_UNorm:         return DXGI_FORMAT_BC7_TYPELESS;
    default:                        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT toDXGIFormat(Field::DataType type, int count)
{
    using DT = Field::DataType;
    switch (type) {
    case DT::Int8:
        switch (count) {
        case 1: return DXGI_FORMAT_R8_SINT;
        case 2: return DXGI_FORMAT_R8G8_SINT;
        //case 3: return DXGI_FORMAT_R8G8B8_SINT;
        case 4: return DXGI_FORMAT_R8G8B8A8_SINT;
        }
        break;
    case DT::Int16:
        switch (count) {
        case 1: return DXGI_FORMAT_R16_SINT;
        case 2: return DXGI_FORMAT_R16G16_SINT;
        //case 3: return DXGI_FORMAT_R16G16B16_SINT;
        case 4: return DXGI_FORMAT_R16G16B16A16_SINT;
        }
        break;
    case DT::Int32:
        switch (count) {
        case 1: return DXGI_FORMAT_R32_SINT;
        case 2: return DXGI_FORMAT_R32G32_SINT;
        case 3: return DXGI_FORMAT_R32G32B32_SINT;
        case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
        }
        break;
    case DT::Uint8:
        switch (count) {
        case 1: return DXGI_FORMAT_R8_UINT;
        case 2: return DXGI_FORMAT_R8G8_UINT;
        //case 3: return DXGI_FORMAT_R8G8B8_UINT;
        case 4: return DXGI_FORMAT_R8G8B8A8_UINT;
        }
        break;
    case DT::Uint16:
        switch (count) {
        case 1: return DXGI_FORMAT_R16_UINT;
        case 2: return DXGI_FORMAT_R16G16_UINT;
        //case 3: return DXGI_FORMAT_R16G16B16_UINT;
        case 4: return DXGI_FORMAT_R16G16B16A16_UINT;
        }
        break;
    case DT::Uint32:
        switch (count) {
        case 1: return DXGI_FORMAT_R32_UINT;
        case 2: return DXGI_FORMAT_R32G32_UINT;
        case 3: return DXGI_FORMAT_R32G32B32_UINT;
        case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
        }
        break;
    case DT::Float:
        switch (count) {
        case 1: return DXGI_FORMAT_R32_FLOAT;
        case 2: return DXGI_FORMAT_R32G32_FLOAT;
        case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
        break;
    }
    return DXGI_FORMAT_UNKNOWN;
}

DXGI_SAMPLE_DESC toDXGISampleDesc(int samples)
{
    return {
        static_cast<UINT>(samples),
        (samples > 1 ? DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN : 0),
    };
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE toD3DTopologyType(Call::PrimitiveType type)
{
    using PT = Call::PrimitiveType;
    switch (type) {
    case PT::Points:             return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case PT::LineStrip:
    //case PT::LineLoop:
    case PT::Lines:
    case PT::LineStripAdjacency:
    case PT::LinesAdjacency:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    default:                     break;
    case PT::Patches:            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}

D3D12_PRIMITIVE_TOPOLOGY toD3DPrimitiveTopology(Call::PrimitiveType type,
    int patchVertices)
{
    using PT = Call::PrimitiveType;
    switch (type) {
    case PT::Points:             return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case PT::LineStrip:          return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case PT::Lines:              return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case PT::LineStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
    case PT::LinesAdjacency:     return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
    case PT::TriangleStrip:      return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PT::TriangleFan:        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN;
    case PT::Triangles:          return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PT::TriangleStripAdjacency:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
    case PT::TrianglesAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    case PT::Patches:
        return static_cast<D3D_PRIMITIVE_TOPOLOGY>(
            D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + patchVertices
            - 1);
    }
    Q_ASSERT(!"not handled");
    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

D3D12_CULL_MODE toD3D(Target::CullMode cullMode)
{
    switch (cullMode) {
    case Target::CullMode::NoCulling:    return D3D12_CULL_MODE_NONE;
    case Target::CullMode::Back:         return D3D12_CULL_MODE_BACK;
    case Target::CullMode::Front:        return D3D12_CULL_MODE_FRONT;
    case Target::CullMode::FrontAndBack: break;
    }
    Q_ASSERT(!"not handled");
    return D3D12_CULL_MODE_NONE;
}

BOOL isFrontCouterClockwise(Target::FrontFace frontFace)
{
    return (frontFace == Target::FrontFace::CCW);
}

D3D12_FILL_MODE toD3D(Target::PolygonMode polygonMode)
{
    switch (polygonMode) {
    case Target::PolygonMode::Fill:  return D3D12_FILL_MODE_SOLID;
    case Target::PolygonMode::Line:  return D3D12_FILL_MODE_WIREFRAME;
    case Target::PolygonMode::Point: break;
    }
    Q_ASSERT(!"not handled");
    return D3D12_FILL_MODE_SOLID;
}

D3D12_COMPARISON_FUNC toD3D(Attachment::ComparisonFunc func)
{
    using CF = Attachment::ComparisonFunc;
    switch (func) {
    case CF::NoComparisonFunc: return D3D12_COMPARISON_FUNC_NONE;
    case CF::Never:            return D3D12_COMPARISON_FUNC_NEVER;
    case CF::Less:             return D3D12_COMPARISON_FUNC_LESS;
    case CF::Equal:            return D3D12_COMPARISON_FUNC_EQUAL;
    case CF::LessEqual:        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CF::Greater:          return D3D12_COMPARISON_FUNC_GREATER;
    case CF::NotEqual:         return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CF::GreaterEqual:     return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CF::Always:           return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    Q_ASSERT(!"not handled");
    return D3D12_COMPARISON_FUNC_ALWAYS;
}

D3D12_BLEND_OP toD3D(Attachment::BlendEquation eq)
{
    using BE = Attachment::BlendEquation;
    switch (eq) {
    case BE::Add:             return D3D12_BLEND_OP_ADD;
    case BE::Min:             return D3D12_BLEND_OP_MIN;
    case BE::Max:             return D3D12_BLEND_OP_MAX;
    case BE::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
    case BE::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    }
    Q_ASSERT(!"not handled");
    return D3D12_BLEND_OP_ADD;
}

D3D12_BLEND toD3D(Attachment::BlendFactor factor)
{
    using BF = Attachment::BlendFactor;
    switch (factor) {
    case BF::Zero:                  return D3D12_BLEND_ZERO;
    case BF::One:                   return D3D12_BLEND_ONE;
    case BF::SrcColor:              return D3D12_BLEND_SRC_COLOR;
    case BF::OneMinusSrcColor:      return D3D12_BLEND_INV_SRC_COLOR;
    case BF::SrcAlpha:              return D3D12_BLEND_SRC_ALPHA;
    case BF::OneMinusSrcAlpha:      return D3D12_BLEND_INV_SRC_ALPHA;
    case BF::DstAlpha:              return D3D12_BLEND_DEST_ALPHA;
    case BF::OneMinusDstAlpha:      return D3D12_BLEND_INV_DEST_ALPHA;
    case BF::DstColor:              return D3D12_BLEND_DEST_COLOR;
    case BF::OneMinusDstColor:      return D3D12_BLEND_INV_DEST_COLOR;
    case BF::SrcAlphaSaturate:      return D3D12_BLEND_SRC_ALPHA_SAT;
    case BF::ConstantColor:         return D3D12_BLEND_BLEND_FACTOR;
    case BF::OneMinusConstantColor: return D3D12_BLEND_INV_BLEND_FACTOR;
    case BF::ConstantAlpha:         return D3D12_BLEND_ALPHA_FACTOR;
    case BF::OneMinusConstantAlpha: return D3D12_BLEND_INV_ALPHA_FACTOR;
    }
    Q_ASSERT(!"not handled");
    return D3D12_BLEND_ONE;
}

D3D12_TEXTURE_ADDRESS_MODE toD3D(Binding::WrapMode wrapMode)
{
    using WM = Binding::WrapMode;
    switch (wrapMode) {
    case WM::Repeat:         return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case WM::MirroredRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case WM::ClampToEdge:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case WM::ClampToBorder:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        //case WM::MirrorOnce:     return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    }
    Q_ASSERT(!"not handled");
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}