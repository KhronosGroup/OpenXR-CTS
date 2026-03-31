// Copyright (c) 2019-2026 The Khronos Group Inc.
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

#include "conformance_framework.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include "common/xr_dependencies.h"

namespace Conformance
{
    TEST_CASE("XR_META_tile_properties_hint", "[XR_META_tile_properties_hint]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_META_TILE_PROPERTIES_HINT_EXTENSION_NAME)) {
            SKIP(XR_META_TILE_PROPERTIES_HINT_EXTENSION_NAME " is not supported");
        }

        AutoBasicInstance instance({XR_META_TILE_PROPERTIES_HINT_EXTENSION_NAME}, AutoBasicInstance::createSystemId);
        AutoBasicSession session(AutoBasicSession::createSession, instance);

        auto xrSetTilePropertiesHintMETA =
            GetInstanceExtensionFunction<PFN_xrSetTilePropertiesHintMETA>(instance, "xrSetTilePropertiesHintMETA");

        XrTilePropertiesMETA xrProperties{XR_TYPE_TILE_PROPERTIES_HINT_META};
        xrProperties.tileDimensions.width = 288;
        xrProperties.tileDimensions.height = 160;
        xrProperties.tileDimensions.depth = 1;
        xrProperties.apronDimensions.width = 0;
        xrProperties.apronDimensions.height = 0;
        xrProperties.origin.x = 0;
        xrProperties.origin.y = 0;

        SECTION("Test xrSetTilePropertiesHintMETA succeeds")
        {
            XrTilePropertiesHintMETA hint{XR_TYPE_TILE_PROPERTIES_HINT_META};
            hint.propertiesCount = 1;
            hint.properties = &xrProperties;
            REQUIRE(XR_SUCCEEDED(xrSetTilePropertiesHintMETA(session, &hint)));
        }
    }

}  // namespace Conformance
