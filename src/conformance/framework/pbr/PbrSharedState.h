// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once

#include <openxr/openxr.h>

#include <stdint.h>

namespace Pbr
{

    namespace ShaderSlots
    {
        /// shader resource index
        enum VSResourceViews
        {
            Transforms = 0,
        };

        /// Sampler/texture index
        enum PSMaterial
        {  // For both samplers and textures.
            BaseColor = 0,
            MetallicRoughness,
            Normal,
            Occlusion,
            Emissive,
            LastMaterialSlot = Emissive,
            NumMaterialSlots = LastMaterialSlot + 1
        };

        /// Sampler/texture index
        enum Pbr
        {  // For both samplers and textures.
            Brdf = NumMaterialSlots
        };

        /// Sampler/texture index
        enum EnvironmentMap
        {  // For both samplers and textures.
            SpecularTexture = Brdf + 1,
            DiffuseTexture = SpecularTexture + 1,
            EnvironmentMapSampler = Brdf + 1
        };

        /// constant buffer index
        enum ConstantBuffers
        {
            Scene,     // Used by VS and PS
            Model,     // VS only
            Material,  // PS only
        };

        enum NumSlots
        {
            NumVSResourceViews = 1,
            NumTextures = DiffuseTexture + 1,
            NumSRVs = NumVSResourceViews + NumTextures,
            NumSamplers = EnvironmentMapSampler + 1,
            NumConstantBuffers = Material + 1,
        };

        namespace GLSL
        {
            enum BindingOffsets
            {
                VSResourceViewsOffset = NumConstantBuffers,
                MaterialTexturesOffset = (int)VSResourceViewsOffset + (int)NumVSResourceViews,
                GlobalTexturesOffset = (int)MaterialTexturesOffset + (int)NumMaterialSlots,
            };
        }
    }  // namespace ShaderSlots

    enum class FillMode : uint32_t
    {
        Solid,
        Wireframe,
    };

    enum class BlendState : uint32_t
    {
        NotAlphaBlended,
        AlphaBlended,
    };

    enum class DoubleSided : uint32_t
    {
        DoubleSided,
        NotDoubleSided,
    };

    enum class FrontFaceWindingOrder : uint32_t
    {
        ClockWise,
        CounterClockWise,
    };

    enum class DepthDirection : uint32_t
    {
        Forward,
        Reversed,
    };

    /// API-independent state
    class SharedState
    {
    public:
        void SetFillMode(FillMode mode);
        FillMode GetFillMode() const;

        void SetFrontFaceWindingOrder(FrontFaceWindingOrder windingOrder);
        FrontFaceWindingOrder GetFrontFaceWindingOrder() const;

        void SetDepthDirection(DepthDirection depthDirection);
        DepthDirection GetDepthDirection() const;

    private:
        FillMode m_fill = FillMode::Solid;
        FrontFaceWindingOrder m_windingOrder = FrontFaceWindingOrder::ClockWise;
        DepthDirection m_depthDirection = DepthDirection::Forward;
    };

}  // namespace Pbr
