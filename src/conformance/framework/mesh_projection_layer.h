// Copyright (c) 2019-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <array>
#include <thread>
#include <numeric>
#include "utils.h"
#include "report.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "composition_utils.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>
#include <xr_linear.h>

namespace Conformance
{

    /// Helper class to provide simple view-locked projection layer of a mesh. Each view of the projection is a separate swapchain.
    class MeshProjectionLayerHelper
    {
    public:
        MeshProjectionLayerHelper(CompositionHelper& compositionHelper);

        /// Set a mesh per view
        void SetMeshes(std::vector<MeshHandle>&& meshes);

        /// Set background colors per view
        void SetBgColors(std::vector<XrColor4f>&& bgColors);

        XrCompositionLayerBaseHeader* TryGetUpdatedProjectionLayer(const XrFrameState& frameState);

        uint32_t GetViewCount() const
        {
            return m_baseHelper.GetViewCount();
        }
        XrSpace GetLocalSpace() const
        {
            return m_baseHelper.GetLocalSpace();
        }

        bool HasMeshes() const
        {
            return m_meshes.size() == GetViewCount();
        }

    private:
        BaseProjectionLayerHelper m_baseHelper;
        std::vector<MeshHandle> m_meshes;
        std::vector<XrColor4f> m_bgColors;
    };
}  // namespace Conformance
