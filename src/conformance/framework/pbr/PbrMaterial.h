// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include "IGltfBuilder.h"
#include "PbrCommon.h"
#include "PbrSharedState.h"

#include <openxr/openxr.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tinygltf
{
    struct Image;
    struct Sampler;
}  // namespace tinygltf

namespace Pbr
{
    /// A Material contains the metallic roughness parameters and textures.
    /// Primitives specify which Material to use when being rendered.
    struct Material
    {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
        /// Coefficients used by the shader. Each texture is sampled and multiplied by these coefficients.
        struct ConstantBufferData
        {
            // packoffset(c0)
            RGBAColor BaseColorFactor{1, 1, 1, 1};

            // packoffset(c1.x and c1.y)
            float MetallicFactor{1};
            float RoughnessFactor{1};
            float _pad0[2];

            // packoffset(c2)
            RGBColor EmissiveFactor{1, 1, 1};
            // padding here must be explicit
            float _pad1;

            // packoffset(c3.x, c3.y and c3.z)
            float NormalScale{1};
            float OcclusionStrength{1};
            float AlphaCutoff{0};
            // needed to round out the size
            float _pad2;
        };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        static_assert((sizeof(ConstantBufferData) % 16) == 0, "Constant Buffer must be divisible by 16 bytes");
        static_assert(sizeof(ConstantBufferData) == 64, "Size must be the same as known");
        static_assert(offsetof(ConstantBufferData, BaseColorFactor) == 0, "Offsets must match shader");
        static_assert(offsetof(ConstantBufferData, MetallicFactor) == 16, "Offsets must match shader");
        static_assert(offsetof(ConstantBufferData, RoughnessFactor) == 20, "Offsets must match shader");
        static_assert(offsetof(ConstantBufferData, EmissiveFactor) == 32, "Offsets must match shader");
        static_assert(offsetof(ConstantBufferData, NormalScale) == 48, "Offsets must match shader");
        static_assert(offsetof(ConstantBufferData, OcclusionStrength) == 52, "Offsets must match shader");
        static_assert(offsetof(ConstantBufferData, AlphaCutoff) == 56, "Offsets must match shader");

        // Create a uninitialized material. Textures and shader coefficients must be set.
        Material();

        // Need at least one virtual function to trigger a vtable and things like dynamic_pointer_cast
        virtual ~Material() = default;

        void SetDoubleSided(DoubleSided doubleSided);
        void SetAlphaBlended(BlendState alphaBlended);

        DoubleSided GetDoubleSided() const;
        FillMode GetWireframe() const;
        BlendState GetAlphaBlended() const;

        ConstantBufferData& Parameters();
        const ConstantBufferData& Parameters() const;

        std::string Name;
        bool Hidden{false};
        // virtual void SetTexture(ShaderSlots::PSMaterial slot, ITexture& texture) = 0;

    protected:
        // copy settings but not state, used in sub-material's Clone
        void CopyFrom(Material const& from);

        mutable bool m_parametersChanged{true};
        std::aligned_storage_t<sizeof(ConstantBufferData), alignof(ConstantBufferData)> m_parametersStorage;

        ConstantBufferData m_parameters{};

        BlendState m_alphaBlended{BlendState::NotAlphaBlended};
        DoubleSided m_doubleSided{DoubleSided::NotDoubleSided};
    };
}  // namespace Pbr
