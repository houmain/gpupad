#pragma once

#include "session/Item.h"
#include <ktx.h>
#include <atomic>
#include <cstdint>
#include <memory>

#if defined(OPENGL_ENABLED)
#  include <qopengl.h>
#endif

#if defined(VULKAN_ENABLED)
#  include <vulkan/vulkan.h>
#  include <ktxvulkan.h>
#endif

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

#if defined(OPENGL_ENABLED)
    bool uploadGL(GLuint *textureId) const;
#endif

#if defined(VULKAN_ENABLED)
    bool uploadVK(ktxVulkanDeviceInfo *vdi, ktxVulkanTexture *vkTexture,
        VkImageUsageFlags usageFlags, VkImageLayout finalLayout) const;
#endif

    friend bool operator==(const TextureData &a, const TextureData &b);
    friend bool operator!=(const TextureData &a, const TextureData &b);

private:
    bool loadKtx(const QString &fileName, bool flipVertically);
    bool loadDDS(const QString &fileName, bool flipVertically);
    bool loadOpenImageIO(const QString &fileName, bool flipVertically);
    bool loadQImage(const QString &fileName, bool flipVertically);
    bool loadPfm(const QString &fileName, bool flipVertically);
    bool saveKtx(const QString &fileName, bool flipVertically) const;
    bool saveDDS(const QString &fileName, bool flipVertically) const;
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

bool isMultisampleTarget(Texture::Target target);
bool isCubemapTarget(Texture::Target target);
TextureSampleType getTextureSampleType(Texture::Format format);
TextureDataType getTextureDataType(Texture::Format format);
int getTextureDataSize(Texture::Format dataType);
int getTextureComponentCount(Texture::Format format);

Q_DECLARE_METATYPE(TextureData)
