// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0

#pragma once
#include "GLCommon.h"
#include "GLResources.h"

#include "../GlslBuffers.h"
#include "../PbrHandles.h"
#include "../PbrModel.h"

namespace Pbr
{

    struct GLPrimitive;
    struct GLResources;

    class GLModelInstance final : public ModelInstance
    {
    public:
        GLModelInstance(Pbr::GLResources& pbrResources, std::shared_ptr<const Model> model);

        /// Render the model.
        void Render(Pbr::GLResources const& pbrResources, XrMatrix4x4f modelToWorld);

    private:
        /// Update the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::GLResources const& pbrResources);

        Glsl::ModelConstantBuffer m_modelBuffer;
        ScopedGLBuffer m_modelConstantBuffer;

        ScopedGLBuffer m_modelTransformsStructuredBuffer;
    };
}  // namespace Pbr
