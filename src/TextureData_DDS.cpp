#include "TextureData.h"

#if defined(_WIN32) && !defined(NOMINMAX)
#  define NOMINMAX
#endif
#include <DirectXTex.h>
#include <algorithm>
#include <cstring>
#include <limits>

using TF = Texture::Format;
using TT = Texture::Target;

namespace {
    DXGI_FORMAT toDXGIFormat(Texture::Format format)
    {
        switch (format) {
        case TF::R8_UNorm:              return DXGI_FORMAT_R8_UNORM;
        case TF::RG8_UNorm:             return DXGI_FORMAT_R8G8_UNORM;
        case TF::RGBA8_UNorm:           return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TF::R16_UNorm:             return DXGI_FORMAT_R16_UNORM;
        case TF::RG16_UNorm:            return DXGI_FORMAT_R16G16_UNORM;
        case TF::RGBA16_UNorm:          return DXGI_FORMAT_R16G16B16A16_UNORM;
        case TF::R8_SNorm:              return DXGI_FORMAT_R8_SNORM;
        case TF::RG8_SNorm:             return DXGI_FORMAT_R8G8_SNORM;
        case TF::RGBA8_SNorm:           return DXGI_FORMAT_R8G8B8A8_SNORM;
        case TF::R16_SNorm:             return DXGI_FORMAT_R16_SNORM;
        case TF::RG16_SNorm:            return DXGI_FORMAT_R16G16_SNORM;
        case TF::RGBA16_SNorm:          return DXGI_FORMAT_R16G16B16A16_SNORM;
        case TF::R8U:                   return DXGI_FORMAT_R8_UINT;
        case TF::RG8U:                  return DXGI_FORMAT_R8G8_UINT;
        case TF::RGBA8U:                return DXGI_FORMAT_R8G8B8A8_UINT;
        case TF::R16U:                  return DXGI_FORMAT_R16_UINT;
        case TF::RG16U:                 return DXGI_FORMAT_R16G16_UINT;
        case TF::RGBA16U:               return DXGI_FORMAT_R16G16B16A16_UINT;
        case TF::R32U:                  return DXGI_FORMAT_R32_UINT;
        case TF::RG32U:                 return DXGI_FORMAT_R32G32_UINT;
        case TF::RGB32U:                return DXGI_FORMAT_R32G32B32_UINT;
        case TF::RGBA32U:               return DXGI_FORMAT_R32G32B32A32_UINT;
        case TF::R8I:                   return DXGI_FORMAT_R8_SINT;
        case TF::RG8I:                  return DXGI_FORMAT_R8G8_SINT;
        case TF::RGBA8I:                return DXGI_FORMAT_R8G8B8A8_SINT;
        case TF::R16I:                  return DXGI_FORMAT_R16_SINT;
        case TF::RG16I:                 return DXGI_FORMAT_R16G16_SINT;
        case TF::RGBA16I:               return DXGI_FORMAT_R16G16B16A16_SINT;
        case TF::R32I:                  return DXGI_FORMAT_R32_SINT;
        case TF::RG32I:                 return DXGI_FORMAT_R32G32_SINT;
        case TF::RGB32I:                return DXGI_FORMAT_R32G32B32_SINT;
        case TF::RGBA32I:               return DXGI_FORMAT_R32G32B32A32_SINT;
        case TF::R16F:                  return DXGI_FORMAT_R16_FLOAT;
        case TF::RG16F:                 return DXGI_FORMAT_R16G16_FLOAT;
        case TF::RGBA16F:               return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TF::R32F:                  return DXGI_FORMAT_R32_FLOAT;
        case TF::RG32F:                 return DXGI_FORMAT_R32G32_FLOAT;
        case TF::RGB32F:                return DXGI_FORMAT_R32G32B32_FLOAT;
        case TF::RGBA32F:               return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TF::RGB9E5:                return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case TF::RG11B10F:              return DXGI_FORMAT_R11G11B10_FLOAT;
        case TF::R5G6B5:                return DXGI_FORMAT_B5G6R5_UNORM;
        case TF::RGB5A1:                return DXGI_FORMAT_B5G5R5A1_UNORM;
        case TF::RGBA4:                 return DXGI_FORMAT_B4G4R4A4_UNORM;
        case TF::RGB10A2:               return DXGI_FORMAT_R10G10B10A2_UNORM;
        case TF::D16:                   return DXGI_FORMAT_D16_UNORM;
        case TF::D24S8:                 return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TF::D32F:                  return DXGI_FORMAT_D32_FLOAT;
        case TF::D32FS8X24:             return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
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
        case TF::SRGB8_Alpha8:          return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TF::SRGB_DXT1:
        case TF::SRGB_Alpha_DXT1:       return DXGI_FORMAT_BC1_UNORM_SRGB;
        case TF::SRGB_Alpha_DXT3:       return DXGI_FORMAT_BC2_UNORM_SRGB;
        case TF::SRGB_Alpha_DXT5:       return DXGI_FORMAT_BC3_UNORM_SRGB;
        case TF::SRGB_BP_UNorm:         return DXGI_FORMAT_BC7_UNORM_SRGB;
        default:                        return DXGI_FORMAT_UNKNOWN;
        }
    }

    Texture::Format fromDXGIFormat(const DirectX::TexMetadata &metadata)
    {
        switch (metadata.format) {
        case DXGI_FORMAT_R8_UNORM:             return TF::R8_UNorm;
        case DXGI_FORMAT_R8G8_UNORM:           return TF::RG8_UNorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:       return TF::RGBA8_UNorm;
        case DXGI_FORMAT_R16_UNORM:            return TF::R16_UNorm;
        case DXGI_FORMAT_R16G16_UNORM:         return TF::RG16_UNorm;
        case DXGI_FORMAT_R16G16B16A16_UNORM:   return TF::RGBA16_UNorm;
        case DXGI_FORMAT_R8_SNORM:             return TF::R8_SNorm;
        case DXGI_FORMAT_R8G8_SNORM:           return TF::RG8_SNorm;
        case DXGI_FORMAT_R8G8B8A8_SNORM:       return TF::RGBA8_SNorm;
        case DXGI_FORMAT_R16_SNORM:            return TF::R16_SNorm;
        case DXGI_FORMAT_R16G16_SNORM:         return TF::RG16_SNorm;
        case DXGI_FORMAT_R16G16B16A16_SNORM:   return TF::RGBA16_SNorm;
        case DXGI_FORMAT_R8_UINT:              return TF::R8U;
        case DXGI_FORMAT_R8G8_UINT:            return TF::RG8U;
        case DXGI_FORMAT_R8G8B8A8_UINT:        return TF::RGBA8U;
        case DXGI_FORMAT_R16_UINT:             return TF::R16U;
        case DXGI_FORMAT_R16G16_UINT:          return TF::RG16U;
        case DXGI_FORMAT_R16G16B16A16_UINT:    return TF::RGBA16U;
        case DXGI_FORMAT_R32_UINT:             return TF::R32U;
        case DXGI_FORMAT_R32G32_UINT:          return TF::RG32U;
        case DXGI_FORMAT_R32G32B32_UINT:       return TF::RGB32U;
        case DXGI_FORMAT_R32G32B32A32_UINT:    return TF::RGBA32U;
        case DXGI_FORMAT_R8_SINT:              return TF::R8I;
        case DXGI_FORMAT_R8G8_SINT:            return TF::RG8I;
        case DXGI_FORMAT_R8G8B8A8_SINT:        return TF::RGBA8I;
        case DXGI_FORMAT_R16_SINT:             return TF::R16I;
        case DXGI_FORMAT_R16G16_SINT:          return TF::RG16I;
        case DXGI_FORMAT_R16G16B16A16_SINT:    return TF::RGBA16I;
        case DXGI_FORMAT_R32_SINT:             return TF::R32I;
        case DXGI_FORMAT_R32G32_SINT:          return TF::RG32I;
        case DXGI_FORMAT_R32G32B32_SINT:       return TF::RGB32I;
        case DXGI_FORMAT_R32G32B32A32_SINT:    return TF::RGBA32I;
        case DXGI_FORMAT_R16_FLOAT:            return TF::R16F;
        case DXGI_FORMAT_R16G16_FLOAT:         return TF::RG16F;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:   return TF::RGBA16F;
        case DXGI_FORMAT_R32_FLOAT:            return TF::R32F;
        case DXGI_FORMAT_R32G32_FLOAT:         return TF::RG32F;
        case DXGI_FORMAT_R32G32B32_FLOAT:      return TF::RGB32F;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:   return TF::RGBA32F;
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:   return TF::RGB9E5;
        case DXGI_FORMAT_R11G11B10_FLOAT:      return TF::RG11B10F;
        case DXGI_FORMAT_B5G6R5_UNORM:         return TF::R5G6B5;
        case DXGI_FORMAT_B5G5R5A1_UNORM:       return TF::RGB5A1;
        case DXGI_FORMAT_B4G4R4A4_UNORM:       return TF::RGBA4;
        case DXGI_FORMAT_R10G10B10A2_UNORM:    return TF::RGB10A2;
        case DXGI_FORMAT_D16_UNORM:            return TF::D16;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:    return TF::D24S8;
        case DXGI_FORMAT_D32_FLOAT:            return TF::D32F;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return TF::D32FS8X24;
        case DXGI_FORMAT_BC1_UNORM:
            return (metadata.GetAlphaMode() == DirectX::TEX_ALPHA_MODE_OPAQUE
                    ? TF::RGB_DXT1
                    : TF::RGBA_DXT1);
        case DXGI_FORMAT_BC2_UNORM:           return TF::RGBA_DXT3;
        case DXGI_FORMAT_BC3_UNORM:           return TF::RGBA_DXT5;
        case DXGI_FORMAT_BC4_UNORM:           return TF::R_ATI1N_UNorm;
        case DXGI_FORMAT_BC4_SNORM:           return TF::R_ATI1N_SNorm;
        case DXGI_FORMAT_BC5_UNORM:           return TF::RG_ATI2N_UNorm;
        case DXGI_FORMAT_BC5_SNORM:           return TF::RG_ATI2N_SNorm;
        case DXGI_FORMAT_BC6H_UF16:           return TF::RGB_BP_UNSIGNED_FLOAT;
        case DXGI_FORMAT_BC6H_SF16:           return TF::RGB_BP_SIGNED_FLOAT;
        case DXGI_FORMAT_BC7_UNORM:           return TF::RGB_BP_UNorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return TF::SRGB8_Alpha8;
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return (metadata.GetAlphaMode() == DirectX::TEX_ALPHA_MODE_OPAQUE
                    ? TF::SRGB_DXT1
                    : TF::SRGB_Alpha_DXT1);
        case DXGI_FORMAT_BC2_UNORM_SRGB: return TF::SRGB_Alpha_DXT3;
        case DXGI_FORMAT_BC3_UNORM_SRGB: return TF::SRGB_Alpha_DXT5;
        case DXGI_FORMAT_BC7_UNORM_SRGB: return TF::SRGB_BP_UNorm;
        default:                         return TF::NoFormat;
        }
    }

    Texture::Target getTextureTarget(const DirectX::TexMetadata &metadata)
    {
        switch (metadata.dimension) {
        case DirectX::TEX_DIMENSION_TEXTURE1D:
            return (metadata.arraySize > 1 ? TT::Target1DArray : TT::Target1D);
        case DirectX::TEX_DIMENSION_TEXTURE2D:
            if (metadata.IsCubemap())
                return (metadata.arraySize > 6 ? TT::TargetCubeMapArray
                                               : TT::TargetCubeMap);
            return (metadata.arraySize > 1 ? TT::Target2DArray : TT::Target2D);
        case DirectX::TEX_DIMENSION_TEXTURE3D: return TT::Target3D;
        default:                               return {};
        }
    }

    bool dimensionsFit(const DirectX::TexMetadata &metadata)
    {
        const auto maxDimension =
            static_cast<size_t>(std::numeric_limits<int>::max());
        return metadata.width > 0 && metadata.width <= maxDimension
            && metadata.height > 0 && metadata.height <= maxDimension
            && metadata.depth > 0 && metadata.depth <= maxDimension
            && metadata.arraySize > 0 && metadata.arraySize <= maxDimension
            && metadata.mipLevels > 0 && metadata.mipLevels <= maxDimension;
    }

    size_t getRowCount(const DirectX::Image &image)
    {
        if (!image.rowPitch || image.slicePitch % image.rowPitch != 0)
            return 0;
        return image.slicePitch / image.rowPitch;
    }

    bool copyImage(uchar *destination, size_t destinationSize,
        const DirectX::Image &source, bool flipVertically)
    {
        const auto rows = getRowCount(source);
        if (!destination || !source.pixels || !rows
            || destinationSize % rows != 0)
            return false;

        const auto destinationPitch = destinationSize / rows;
        if (destinationPitch < source.rowPitch)
            return false;

        for (auto row = size_t{}; row < rows; ++row) {
            const auto sourceRow = (flipVertically ? rows - row - 1 : row);
            auto *dest = destination + row * destinationPitch;
            std::memcpy(dest, source.pixels + sourceRow * source.rowPitch,
                source.rowPitch);
            std::memset(dest + source.rowPitch, 0,
                destinationPitch - source.rowPitch);
        }
        return true;
    }

    bool copyToDirectX(const uchar *source, size_t sourceSize,
        const DirectX::Image &destination, bool flipVertically)
    {
        const auto rows = getRowCount(destination);
        if (!source || !destination.pixels || !rows || sourceSize % rows != 0)
            return false;

        const auto sourcePitch = sourceSize / rows;
        if (sourcePitch < destination.rowPitch)
            return false;

        for (auto row = size_t{}; row < rows; ++row) {
            const auto sourceRow = (flipVertically ? rows - row - 1 : row);
            std::memcpy(destination.pixels + row * destination.rowPitch,
                source + sourceRow * sourcePitch, destination.rowPitch);
        }
        return true;
    }

    template <typename F>
    bool forEachImage(const DirectX::TexMetadata &metadata,
        const DirectX::ScratchImage &images, int layers, int faces, F &&func)
    {
        for (auto level = 0; level < static_cast<int>(metadata.mipLevels);
            ++level) {
            if (metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D) {
                const auto slices =
                    std::max(metadata.depth >> level, size_t{ 1 });
                for (auto slice = size_t{}; slice < slices; ++slice) {
                    const auto *image = images.GetImage(level, 0, slice);
                    if (!image
                        || !func(level, 0, static_cast<int>(slice), *image))
                        return false;
                }
                continue;
            }

            for (auto layer = 0; layer < layers; ++layer)
                for (auto face = 0; face < faces; ++face) {
                    const auto item = static_cast<size_t>(layer * faces + face);
                    const auto *image = images.GetImage(level, item, 0);
                    if (!image || !func(level, layer, face, *image))
                        return false;
                }
        }
        return true;
    }
} // namespace

bool TextureData::loadDDS(const QString &fileName, bool flipVertically)
{
    if (!fileName.endsWith(".dds", Qt::CaseInsensitive))
        return false;

    auto metadata = DirectX::TexMetadata{};
    auto images = DirectX::ScratchImage{};
    const auto path = fileName.toStdWString();
    if (FAILED(DirectX::LoadFromDDSFile(path.c_str(),
            DirectX::DDS_FLAGS_FORCE_RGB, &metadata, images)))
        return false;

    const auto format = fromDXGIFormat(metadata);
    const auto target = getTextureTarget(metadata);
    if (format == TF::NoFormat || target == static_cast<TT>(0)
        || !dimensionsFit(metadata))
        return false;

    if (metadata.IsCubemap() && metadata.arraySize % 6 != 0)
        return false;

    const auto layers = static_cast<int>(metadata.IsCubemap()
            ? metadata.arraySize / 6
            : metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D
            ? 1
            : metadata.arraySize);
    const auto faces = (metadata.IsCubemap() ? 6 : 1);
    if (!create(target, format, static_cast<int>(metadata.width),
            static_cast<int>(metadata.height), static_cast<int>(metadata.depth),
            layers, static_cast<int>(metadata.mipLevels)))
        return false;

    const auto copied = forEachImage(metadata, images, layers, faces,
        [&](int level, int layer, int faceSlice, const DirectX::Image &image) {
            return copyImage(getWriteonlyData(level, layer, faceSlice),
                static_cast<size_t>(getImageSize(level)), image,
                flipVertically);
        });
    if (!copied) {
        *this = {};
        return false;
    }

    setFlippedVertically(flipVertically);
    return true;
}

bool TextureData::saveDDS(const QString &fileName, bool flipVertically) const
{
    if (!fileName.endsWith(".dds", Qt::CaseInsensitive) || isNull())
        return false;

    auto metadata = DirectX::TexMetadata{};
    metadata.width = static_cast<size_t>(width());
    metadata.height = static_cast<size_t>(height());
    metadata.depth = static_cast<size_t>(depth());
    metadata.mipLevels = static_cast<size_t>(levels());
    metadata.format = toDXGIFormat(format());
    if (metadata.format == DXGI_FORMAT_UNKNOWN)
        return false;

    auto flags = DirectX::DDS_FLAGS_NONE;
    if (format() == TF::RGB_DXT1 || format() == TF::SRGB_DXT1) {
        metadata.SetAlphaMode(DirectX::TEX_ALPHA_MODE_OPAQUE);
        flags = DirectX::DDS_FLAGS_FORCE_DX10_EXT_MISC2;
    }

    const auto target = getTarget();
    switch (target) {
    case TT::Target1D:
    case TT::Target1DArray:
        metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE1D;
        break;
    case TT::Target2D:
    case TT::Target2DArray:
    case TT::TargetRectangle:
        metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
        break;
    case TT::TargetCubeMap:
    case TT::TargetCubeMapArray:
        metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
        metadata.miscFlags = DirectX::TEX_MISC_TEXTURECUBE;
        break;
    case TT::Target3D:
        metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE3D;
        break;
    default: return false;
    }

    metadata.arraySize = (isCubemap() ? static_cast<size_t>(layers()) * 6
                                      : static_cast<size_t>(layers()));

    auto images = DirectX::ScratchImage{};
    if (FAILED(images.Initialize(metadata)))
        return false;

    const auto copied = forEachImage(metadata, images, layers(), faces(),
        [&](int level, int layer, int faceSlice, const DirectX::Image &image) {
            return copyToDirectX(getData(level, layer, faceSlice),
                static_cast<size_t>(getImageSize(level)), image,
                flipVertically);
        });
    if (!copied)
        return false;

    const auto path = fileName.toStdWString();
    return SUCCEEDED(DirectX::SaveToDDSFile(images.GetImages(),
        images.GetImageCount(), metadata, flags, path.c_str()));
}
