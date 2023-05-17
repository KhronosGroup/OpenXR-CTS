// Copyright (c) 2019-2023, The Khronos Group Inc.
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

#include "mesh_projection_layer.h"
#include "composition_utils.h"
#include "graphics_plugin.h"

#include <vector>

namespace
{
    using namespace Conformance;

    class MeshViewRenderer : public BaseProjectionLayerHelper::ViewRenderer
    {
    public:
        MeshViewRenderer(std::vector<MeshHandle>& meshes, span<const XrColor4f> bgColors) : m_meshes(meshes), m_bgColors(bgColors)
        {
        }

        ~MeshViewRenderer() override = default;
        void RenderView(const BaseProjectionLayerHelper& /* projectionLayerHelper */, uint32_t viewIndex,
                        const XrViewState& /* viewState */, const XrView& view, XrCompositionLayerProjectionView& projectionView,
                        const XrSwapchainImageBaseHeader* swapchainImage) override
        {
            // Clear to customized background color
            GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, 0, m_bgColors[viewIndex]);

            // Draw the mesh
            auto meshHandles = {MeshDrawable(m_meshes[viewIndex], view.pose, {1.0, 1.0, 1.0})};
            GetGlobalData().graphicsPlugin->RenderView(projectionView, swapchainImage, RenderParams{}.Draw(meshHandles));
        }

    private:
        span<const MeshHandle> m_meshes;
        span<const XrColor4f> m_bgColors;
    };
}  // namespace

namespace Conformance
{
    MeshProjectionLayerHelper::MeshProjectionLayerHelper(CompositionHelper& compositionHelper)
        : m_baseHelper(compositionHelper, XR_REFERENCE_SPACE_TYPE_LOCAL)
    {
        m_bgColors.resize(GetViewCount(), DarkSlateGrey);
    }

    void MeshProjectionLayerHelper::SetMeshes(std::vector<MeshHandle>&& meshes)
    {
        if (meshes.size() != GetViewCount()) {
            throw std::logic_error("Mismatch between mesh count and view count");
        }
        m_meshes = std::move(meshes);
    }

    void MeshProjectionLayerHelper::SetBgColors(std::vector<XrColor4f>&& bgColors)
    {
        if (bgColors.size() != GetViewCount()) {
            throw std::logic_error("Mismatch between bgColors count and view count");
        }
        m_bgColors = std::move(bgColors);
    }

    XrCompositionLayerBaseHeader* MeshProjectionLayerHelper::TryGetUpdatedProjectionLayer(const XrFrameState& frameState)
    {
        if (HasMeshes()) {
            MeshViewRenderer renderer{m_meshes, m_bgColors};
            return m_baseHelper.TryGetUpdatedProjectionLayer(frameState, renderer);
        }
        // no meshes to render
        return nullptr;
    }
}  // namespace Conformance
