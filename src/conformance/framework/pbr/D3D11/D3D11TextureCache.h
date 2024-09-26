// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#ifdef _WIN32
#include <d3d11.h>
#include <d3d11_2.h>
#include <openxr/openxr.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <map>
#include <memory>
#include <mutex>

namespace Pbr
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
    struct D3D11Resources;

    /// Cache of single-color textures.
    ///
    /// Device-dependent, drop when device is lost or destroyed.
    class D3D11TextureCache
    {
    public:
        D3D11TextureCache();

        D3D11TextureCache(D3D11TextureCache&&) = default;
        D3D11TextureCache& operator=(D3D11TextureCache&&) = default;

        explicit D3D11TextureCache(ID3D11Device* device);

        /// Find or create a single pixel texture of the given color
        ComPtr<ID3D11ShaderResourceView> CreateTypedSolidColorTexture(const Pbr::D3D11Resources& pbrResources, XrColor4f color, bool sRGB);

    private:
        ComPtr<ID3D11Device> m_device;
        // in unique_ptr to make it moveable
        std::unique_ptr<std::mutex> m_cacheMutex;
        std::map<uint32_t, ComPtr<ID3D11ShaderResourceView>> m_solidColorTextureCache;
    };

}  // namespace Pbr
#endif
