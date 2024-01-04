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
    template <typename ModelType, typename ResourcesType>
    class GltfModelBase
    {

    public:
        GltfModelBase(ResourcesType& pbrResources, std::shared_ptr<tinygltf::Model> gltf, std::shared_ptr<ModelType> pbrModel = nullptr,
                      Pbr::FillMode fillMode = Pbr::FillMode::Solid)
            : m_gltf(gltf)
            , m_pbrModel(pbrModel != nullptr ? std::move(pbrModel) : Gltf::FromGltfObject<ModelType>(pbrResources, *gltf))
            , m_fillMode(fillMode)
        {
        }

        void SetModel(std::shared_ptr<ModelType>&& model)
        {
            m_pbrModel = std::move(model);
        }
        const std::shared_ptr<ModelType>& GetModel() const noexcept
        {
            return m_pbrModel;
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
            for (uint32_t k = 0; k < GetModel()->GetPrimitiveCount(); k++) {
                auto& material = pbrResources.GetPrimitive(GetModel()->GetPrimitive(k)).GetMaterial();
                material->Parameters().BaseColorFactor = color;
            }
        }

    private:
        std::shared_ptr<tinygltf::Model> m_gltf;
        std::shared_ptr<ModelType> m_pbrModel;
        Pbr::FillMode m_fillMode;
    };
}  // namespace Conformance
