#pragma once

#include "session/Item.h"
#include <ktx.h>
#include <vulkan/vulkan.h>
#include <ktxvulkan.h>
#include <memory>

class TextureData
{
public:
    bool isSharedWith(const TextureData &other) const;
    bool create(Texture::Target target, Texture::Format format, int width,
        int height, int depth, int layers, int levels = 0);
    TextureData resize(int width, int height, int depth, int layers) const;
    TextureData convert(Texture::Format format) const;
    TextureData convert(Texture::Format format, int width, int height,
        int depth, int layers) const;
    bool load(const QString &fileName, bool flipVertically);
    bool loadQImage(QImage image, bool flipVertically);
    bool save(const QString &fileName, bool flipVertically) const;
    bool isNull() const;
    void clear();
    QImage toImage() const;
    bool isConvertibleToImage() const;

    bool isArray() const;
    bool isCubemap() const;
    bool isCompressed() const;
    int dimensions() const;
    Texture::Target getTarget(int samples = 0) const;
    Texture::Format format() const;
    uint32_t pixelFormat() const;
    uint32_t pixelType() const;
    int width() const { return getLevelWidth(0); }
    int height() const { return getLevelHeight(0); }
    int depth() const { return getLevelDepth(0); }
    int getLevelWidth(int level) const;
    int getLevelHeight(int level) const;
    int getLevelDepth(int level) const;
    int getLevelStride(int level) const;
    int levels() const;
    int layers() const;
    int faces() const;
    void setFlippedVertically(bool flipped) { mFlippedVertically = flipped; }
    bool flippedVertically() const { return mFlippedVertically; }
    uchar *getWriteonlyData();
    const uchar *getData() const;
    int getDataSize() const;
    uchar *getWriteonlyData(int level, int layer, int faceSlice);
    const uchar *getData(int level, int layer, int faceSlice) const;
    size_t getOffset(int level, int layer, int faceSlice) const;
    int getImageSize(int level) const;
    int getSlicesSize(int level) const;
    int getLevelSize(int level) const;
    bool uploadGL(GLuint *textureId) const;
    bool uploadVK(ktxVulkanDeviceInfo *vdi, ktxVulkanTexture *vkTexture,
        VkImageUsageFlags usageFlags, VkImageLayout initialLayout) const;

    friend bool operator==(const TextureData &a, const TextureData &b);
    friend bool operator!=(const TextureData &a, const TextureData &b);

private:
    bool loadKtx(const QString &fileName, bool flipVertically);
    bool loadOpenImageIO(const QString &fileName, bool flipVertically);
    bool loadQImage(const QString &fileName, bool flipVertically);
    bool loadPfm(const QString &fileName, bool flipVertically);
    bool saveKtx(const QString &fileName, bool flipVertically) const;
    bool saveQImage(const QString &fileName, bool flipVertically) const;
    bool saveOpenImageIO(const QString &fileName, bool flipVertically) const;
    bool savePfm(const QString &fileName, bool flipVertically) const;
    void flipVertically();

    std::shared_ptr<ktxTexture1> mKtxTexture;
    bool mFlippedVertically{};
};

enum class TextureSampleType {
    Normalized,
    Normalized_sRGB,
    Float,
    Int8,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Uint_10_10_10_2,
};

enum class TextureDataType {
    Packed,
    Compressed,
    Int8,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Float16,
    Float32,
};

enum class ShareHandleType {
    None = 0,
    OPENGL_TEXTURE_ID = 1,
    OPAQUE_FD = 0x9586,
    OPAQUE_WIN32 = 0x9587,
    OPAQUE_WIN32_KMT = 0x9588,
    D3D12_TILEPOOL = 0x9589,
    D3D12_RESOURCE = 0x958A,
    D3D11_IMAGE = 0x958B,
    D3D11_IMAGE_KMT = 0x958C,
    D3D_FENCE = 0x9594,
};

struct ShareHandle
{
    ShareHandleType type;
    void *handle;
    size_t allocationSize;
    size_t allocationOffset;
    bool dedicated;

    explicit operator bool() const { return (handle != nullptr); }
    auto operator<=>(const ShareHandle &) const = default;
};

bool isMultisampleTarget(Texture::Target target);
bool isCubemapTarget(Texture::Target target);
TextureSampleType getTextureSampleType(Texture::Format format);
TextureDataType getTextureDataType(Texture::Format format);
int getTextureDataSize(Texture::Format dataType);
int getTextureComponentCount(Texture::Format format);

Q_DECLARE_METATYPE(TextureData)
