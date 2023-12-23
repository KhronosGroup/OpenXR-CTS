// Copyright 2023, The Khronos Group, Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "GLCommon.h"

#include "common/gfxwrapper_opengl.h"

#include <openxr/openxr.h>

#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>

namespace Pbr
{

    /// Cache of single-color textures.
    ///
    /// Device-dependent, drop when device is lost or destroyed.
    class GLTextureCache
    {
    public:
        /// Default constructor makes an invalid cache.
        GLTextureCache() = default;

        GLTextureCache(GLTextureCache&&) = default;
        GLTextureCache& operator=(GLTextureCache&&) = default;

        void Init();

        bool IsValid() const noexcept
        {
            return m_cacheMutex != nullptr;
        }

        /// Find or create a single pixel texture of the given color
        std::shared_ptr<ScopedGLTexture> CreateTypedSolidColorTexture(XrColor4f color);

    private:
        // in unique_ptr to make it moveable
        std::unique_ptr<std::mutex> m_cacheMutex;
        std::map<uint32_t, std::shared_ptr<ScopedGLTexture>> m_solidColorTextureCache;
    };

}  // namespace Pbr
