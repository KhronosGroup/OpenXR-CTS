// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "D3D12Resources.h"

#include "utilities/d3d12_utils.h"

#include <d3d12.h>
#include <openxr/openxr.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <map>
#include <memory>
#include <mutex>

namespace Pbr
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    /// Cache of single-color textures.
    ///
    /// Device-dependent, drop when device is lost or destroyed.
    class D3D12TextureCache
    {
    public:
        D3D12TextureCache();

        D3D12TextureCache(D3D12TextureCache&&) = default;
        D3D12TextureCache& operator=(D3D12TextureCache&&) = default;

        /// Find or create a single pixel texture of the given color
        Conformance::D3D12ResourceWithSRVDesc CreateTypedSolidColorTexture(Pbr::D3D12Resources& pbrResources,
                                                                           ID3D12GraphicsCommandList* copyCommandList,
                                                                           StagingResources stagingResources, XrColor4f color, bool sRGB);

    private:
        // in unique_ptr to make it moveable
        std::unique_ptr<std::mutex> m_cacheMutex;
        std::map<uint32_t, Conformance::D3D12ResourceWithSRVDesc> m_solidColorTextureCache;
    };

}  // namespace Pbr
