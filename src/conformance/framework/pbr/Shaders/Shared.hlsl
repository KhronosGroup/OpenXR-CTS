////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT

cbuffer SceneBuffer : register(b0)
{
    float4x4 ViewProjection     : packoffset(c0);
    float3 EyePosition          : packoffset(c4);
    float3 LightDirection       : packoffset(c5);
    float3 LightColor           : packoffset(c6);
    uint NumSpecularMipLevels   : packoffset(c7);
};
