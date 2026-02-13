#pragma once

#if defined(_WIN32)

#  include "ShaderCompiler.h"

#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <wrl.h>
#  include <d3d12shader.h>

using Microsoft::WRL::ComPtr;

namespace ShaderCompiler {

    bool compileDXIL(const Session &session, const Input &input,
        MessagePtrSet &messages, ComPtr<ID3DBlob> &binary,
        ComPtr<ID3D12ShaderReflection> &d3dReflection);

}; // namespace ShaderCompiler

inline void AssertIfFailed(HRESULT hr)
{
    Q_ASSERT(SUCCEEDED(hr));
}

#endif // _WIN32
