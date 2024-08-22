// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "utilities/metal_utils.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <memory>

namespace Pbr
{

    struct MetalPrimitive;
    struct MetalResources;

    class MetalModelInstance final : public ModelInstance
    {
    public:
        MetalModelInstance(Pbr::MetalResources& pbrResources, std::shared_ptr<const Model> model);

        /// Render the model.
        void Render(Pbr::MetalResources const& pbrResources, MTL::RenderCommandEncoder* renderCommandEncoder,
                    MTL::PixelFormat colorRenderTargetFormat, MTL::PixelFormat depthRenderTargetFormat);

    private:
        /// Updated the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::MetalResources const& pbrResources, MTL::RenderCommandEncoder* renderCommandEncoder);

        /// Temporary buffer holds the world transforms, computed from the node's local transforms.
        mutable std::vector<simd::float4x4> m_modelTransforms;
        mutable NS::SharedPtr<MTL::Buffer> m_modelTransformsStructuredBuffer;
    };
}  // namespace Pbr
