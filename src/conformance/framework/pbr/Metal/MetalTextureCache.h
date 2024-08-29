// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "utilities/metal_utils.h"

#include <openxr/openxr.h>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <map>
#include <memory>
#include <mutex>

namespace Pbr
{

    /// Cache of single-color textures.
    ///
    /// Device-dependent, drop when device is lost or destroyed.
    class MetalTextureCache
    {
    public:
        /// Default constructor makes an invalid cache.
        MetalTextureCache() = default;

        MetalTextureCache(MetalTextureCache&&) = default;
        MetalTextureCache& operator=(MetalTextureCache&&) = default;

        explicit MetalTextureCache(MTL::Device* device);

        bool IsValid() const noexcept
        {
            return (bool)m_device;
        }

        /// Find or create a single pixel texture of the given color
        NS::SharedPtr<MTL::Texture> CreateTypedSolidColorTexture(XrColor4f color);

    private:
        NS::SharedPtr<MTL::Device> m_device;
        /// in unique_ptr to make it moveable
        std::unique_ptr<std::mutex> m_cacheMutex;
        std::map<uint32_t, NS::SharedPtr<MTL::Texture>> m_solidColorTextureCache;
    };

}  // namespace Pbr
