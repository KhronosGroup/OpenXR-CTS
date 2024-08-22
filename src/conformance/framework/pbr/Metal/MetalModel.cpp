// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#if defined(XR_USE_GRAPHICS_API_METAL)

#include "MetalModel.h"

#include "MetalPrimitive.h"
#include "MetalResources.h"
#include "MetalMaterial.h"

#include "utilities/metal_utils.h"
#include "utilities/throw_helpers.h"

#include <numeric>

namespace Pbr
{

    MetalModelInstance::MetalModelInstance(Pbr::MetalResources& pbrResources, std::shared_ptr<const Model> model)
        : ModelInstance(std::move(model))
    {
        const auto& nodes = GetModel().GetNodes();

        XrMatrix4x4f identityMatrix;
        XrMatrix4x4f_CreateIdentity(&identityMatrix);  // or better yet poison it
        m_modelTransforms.resize(nodes.size(), (simd::float4x4&)identityMatrix);

        /// Create/recreate the structured buffer and SRV which holds the node transforms.
        uint32_t elemSize = sizeof(decltype(m_modelTransforms)::value_type);
        uint32_t count = (uint32_t)(m_modelTransforms.size());
        uint32_t size = (uint32_t)(count * elemSize);

        m_modelTransformsStructuredBuffer = NS::TransferPtr(pbrResources.GetDevice()->newBuffer(size, MTL::ResourceStorageModeManaged));
    }

    void MetalModelInstance::Render(Pbr::MetalResources const& pbrResources, MTL::RenderCommandEncoder* renderCommandEncoder,
                                    MTL::PixelFormat colorRenderTargetFormat, MTL::PixelFormat depthRenderTargetFormat)
    {
        renderCommandEncoder->pushDebugGroup(MTLSTR("MetalModel::Render"));

        UpdateTransforms(pbrResources, renderCommandEncoder);

        const uint8_t TransformsIndex = ShaderSlots::ConstantBuffers::Material + 1;
        assert(TransformsIndex == 3);

        renderCommandEncoder->setVertexBuffer(m_modelTransformsStructuredBuffer.get(), 0, TransformsIndex);

        for (PrimitiveHandle primitiveHandle : GetModel().GetPrimitiveHandles()) {
            const Pbr::MetalPrimitive& primitive = pbrResources.GetPrimitive(primitiveHandle);
            if (primitive.GetMaterial()->Hidden)
                continue;

            if (!IsAnyNodeVisible(primitive.GetNodes()))
                continue;

            primitive.Render(pbrResources, renderCommandEncoder, colorRenderTargetFormat, depthRenderTargetFormat);
        }

        renderCommandEncoder->popDebugGroup();
    }

    void MetalModelInstance::UpdateTransforms(Pbr::MetalResources const& /*pbrResources*/,
                                              MTL::RenderCommandEncoder* /*renderCommandEncoder*/)
    {
        /// If none of the node transforms have changed, no need to recompute/update the model transform structured buffer.
        if (ResolvedTransformsNeedUpdate()) {
            ResolveTransformsAndVisibilities(false);

            // Update node transform structured buffer.
            memcpy(m_modelTransformsStructuredBuffer->contents(), GetResolvedTransforms().data(),
                   m_modelTransformsStructuredBuffer->length());
            m_modelTransformsStructuredBuffer->didModifyRange(NS::Range(0, m_modelTransformsStructuredBuffer->length()));

            MarkResolvedTransformsUpdated();
        }
    }
}  // namespace Pbr

#endif  // defined(XR_USE_GRAPHICS_API_METAL)
