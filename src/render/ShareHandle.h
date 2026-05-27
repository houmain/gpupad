#pragma once

#include <cstddef>
#include <cstdint>

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
};

struct ShareHandle
{
    ShareHandleType type{};
    void *handle{};
    size_t allocationSize{};
    size_t allocationOffset{};
    bool dedicated{};

    explicit operator bool() const { return (handle != nullptr); }
    bool sameResource(const ShareHandle &other) const
    {
        return type == other.type && handle == other.handle
            && allocationSize == other.allocationSize
            && allocationOffset == other.allocationOffset
            && dedicated == other.dedicated;
    }
    bool operator==(const ShareHandle &other) const
    {
        return sameResource(other);
    }
};
