// Copyright (c) 2019-2025 The Khronos Group Inc.
// Copyright (c) 2019 Collabora, Ltd.
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

#include <openxr/openxr.h>

#include <memory>
#include <vector>

#include "graphics_plugin.h"
#include "utilities/types_and_constants.h"

namespace Conformance
{

    struct SpatialTestRunner
    {
        virtual ~SpatialTestRunner() = default;
        void RunTest(const char* testName, const char* instructions,
                     const XrPosef& instructionsPanelPose = {{0, 0, 0, 1}, {-0.2f, 0, -1.0f}});
        virtual std::vector<const char*> getRequiredExtensions()
        {
            return {};
        }
        virtual void onCreateSpatialContext(std::vector<const XrSpatialCapabilityConfigurationBaseHeaderEXT*>& capabilityConfigs) = 0;
        virtual void postCreateSpatialContextCompletion(const XrFrameState& /*unused*/)
        {
        }
        virtual void postCreateDiscoverySnapshotCompletion(XrSpatialSnapshotEXT snapshot);
        virtual void querySnapshot(XrSpatialSnapshotEXT snapshot);
        virtual std::vector<XrSpatialComponentTypeEXT> getQueryComponents() = 0;
        virtual std::vector<XrBaseOutStructure*> getComponentDataListStructPtrs(uint32_t /*unused*/)
        {
            return {};
        }
        virtual void onUpdate(const XrFrameState& /*unused*/)
        {
        }
        virtual void render()
        {
        }
        void renderBounded2D(const XrSpatialBounded2DDataEXT& bounded2D, XrColor4f color = {0, 0, 0, 0});
        void renderGnomon(const XrPosef& pose);

        enum class DiscoveryState
        {
            WaitingForCtxCreation,
            Idle,
            DiscoveryRecommended,
            WaitingForDiscoveryResult,
            StopDiscovery,
        };

        DiscoveryState state;
        XrInstance instance = XR_NULL_HANDLE;
        XrSpace localSpace = XR_NULL_HANDLE;
        XrSpatialContextEXT spatialContext = XR_NULL_HANDLE;

        std::vector<XrSpatialEntityIdEXT> entityIds;
        std::vector<XrSpatialEntityTrackingStateEXT> entityStates;

        std::vector<Cube> renderedCubes;
        std::vector<MeshDrawable> renderedMeshes;

    private:
        XrFutureEXT discoveryFuture = XR_NULL_FUTURE_EXT;
        XrSpatialSnapshotEXT discoverySnapshot = XR_NULL_HANDLE;
        MeshHandle gnomonMesh;
    };

}  // namespace Conformance
