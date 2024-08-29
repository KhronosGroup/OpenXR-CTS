// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#include "PbrMaterial.h"
#include <type_traits>

namespace Pbr
{
    static_assert(std::is_standard_layout<Material::ConstantBufferData>::value, "Type must be standard layout");
    static_assert(sizeof(RGBAColor) == 16, "RGBAColor must be 16 bytes");
    static_assert(offsetof(Material::ConstantBufferData, MetallicFactor) == 16, "Offsets must match struct in shader");
    static_assert(offsetof(Material::ConstantBufferData, RoughnessFactor) == 20, "Offsets must match struct in shader");
    static_assert(offsetof(Material::ConstantBufferData, EmissiveFactor) == 32, "Offsets must match struct in shader");
    static_assert(offsetof(Material::ConstantBufferData, NormalScale) == 48, "Offsets must match struct in shader");
    static_assert(offsetof(Material::ConstantBufferData, OcclusionStrength) == 52, "Offsets must match struct in shader");
    static_assert(offsetof(Material::ConstantBufferData, AlphaCutoff) == 56, "Offsets must match struct in shader");

    Material::Material()
    {
    }

    void Material::CopyFrom(Material const& from)
    {
        Name = from.Name;
        Hidden = from.Hidden;
        m_parameters = from.m_parameters;
        m_parametersChanged = true;
        // m_alphaBlended = from.m_alphaBlended;
        // m_doubleSided = from.m_doubleSided;
    }

    void Material::SetDoubleSided(DoubleSided doubleSided)
    {
        m_doubleSided = doubleSided;
    }

    void Material::SetAlphaBlended(BlendState alphaBlended)
    {
        m_alphaBlended = alphaBlended;
    }

    DoubleSided Material::GetDoubleSided() const
    {
        return m_doubleSided;
    }

    BlendState Material::GetAlphaBlended() const
    {
        return m_alphaBlended;
    }

    Material::ConstantBufferData& Material::Parameters()
    {
        m_parametersChanged = true;
        return m_parameters;
    }

    const Material::ConstantBufferData& Material::Parameters() const
    {
        return m_parameters;
    }
}  // namespace Pbr
