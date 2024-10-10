// Copyright (c) 2022-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: MIT
#pragma once

#include "cts_tinygltf.h"

#include "gltf/GltfHelper.h"
#include "pbr/GltfLoader.h"
#include "pbr/PbrSharedState.h"

#include <memory>

namespace Conformance
{

    /// Templated base class for API-specific model objects in the main CTS code.
    template <typename ModelInstanceType, typename ResourcesType>
    class RenderableGltfModelInstanceBase
    {

    public:
        explicit RenderableGltfModelInstanceBase(ModelInstanceType&& pbrModelInstance, Pbr::FillMode fillMode = Pbr::FillMode::Solid)
            : m_pbrModelInstance(std::move(pbrModelInstance)), m_fillMode(fillMode)
        {
        }

        ModelInstanceType& GetModelInstance() noexcept
        {
            return m_pbrModelInstance;
        }
        const ModelInstanceType& GetModelInstance() const noexcept
        {
            return m_pbrModelInstance;
        }

        void SetFillMode(const Pbr::FillMode& fillMode)
        {
            m_fillMode = fillMode;
        }

        Pbr::FillMode GetFillMode() const noexcept
        {
            return m_fillMode;
        }

        void SetBaseColorFactor(ResourcesType& pbrResources, Pbr::RGBAColor color)
        {
            for (uint32_t k = 0; k < m_pbrModelInstance.GetPrimitiveCount(); k++) {
                auto& material = pbrResources.GetPrimitive(m_pbrModelInstance.GetPrimitiveHandle(k)).GetMaterial();
                material->Parameters().BaseColorFactor = color;
            }
        }

    private:
        std::shared_ptr<const tinygltf::Model> m_gltf;
        ModelInstanceType m_pbrModelInstance;
        Pbr::FillMode m_fillMode;
    };
}  // namespace Conformance
