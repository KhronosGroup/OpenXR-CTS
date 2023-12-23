// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once
#include "GLCommon.h"
#include "GLResources.h"

#include "../PbrHandles.h"
#include "../PbrModel.h"

#include "common/xr_linear.h"

#include <stdint.h>
#include <vector>

namespace Pbr
{

    struct GLPrimitive;
    struct GLResources;

    class GLModel final : public Model
    {
    public:
        // Render the model.
        void Render(Pbr::GLResources const& pbrResources);

    private:
        // Updated the transforms used to render the model. This needs to be called any time a node transform is changed.
        void UpdateTransforms(Pbr::GLResources const& pbrResources);

        // Temporary buffer holds the world transforms, computed from the node's local transforms.
        mutable std::vector<XrMatrix4x4f> m_modelTransforms;
        mutable ScopedGLBuffer m_modelTransformsStructuredBuffer;

        mutable uint32_t TotalModifyCount{0};
    };
}  // namespace Pbr
