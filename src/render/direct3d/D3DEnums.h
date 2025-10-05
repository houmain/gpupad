#pragma once

#if defined(_WIN32)

#include "D3DContext.h"
#include "session/Item.h"
#include <optional>

DXGI_FORMAT toDXGIFormat(QOpenGLTexture::TextureFormat format);
DXGI_FORMAT toDXGITypelessFormat(Texture::Format format);
DXGI_FORMAT toDXGIFormat(Field::DataType type, int count);
DXGI_SAMPLE_DESC toDXGISampleDesc(int samples);
D3D12_PRIMITIVE_TOPOLOGY_TYPE toD3DTopologyType(Call::PrimitiveType type);
D3D12_PRIMITIVE_TOPOLOGY toD3DPrimitiveTopology(Call::PrimitiveType type,
    int patchVertices);
D3D12_CULL_MODE toD3D(Target::CullMode cullMode);
BOOL isFrontCouterClockwise(Target::FrontFace frontFace);
D3D12_FILL_MODE toD3D(Target::PolygonMode polygonMode);
D3D12_COMPARISON_FUNC toD3D(Attachment::ComparisonFunc func);
D3D12_COMPARISON_FUNC toD3D(Binding::ComparisonFunc compareFunc);
D3D12_BLEND_OP toD3D(Attachment::BlendEquation eq);
D3D12_BLEND toD3D(Attachment::BlendFactor factor);
D3D12_TEXTURE_ADDRESS_MODE toD3D(Binding::WrapMode wrapMode);

#endif // _WIN32
