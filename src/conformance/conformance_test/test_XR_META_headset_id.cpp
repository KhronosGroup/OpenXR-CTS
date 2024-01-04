// Copyright (c) 2019-2024, The Khronos Group Inc.
// Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
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
#include "conformance_utils.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include "utilities/throw_helpers.h"
#include "utilities/system_properties_helper.h"

#include <openxr/openxr.h>

#include <cstring>
#include <string>

namespace Conformance
{
    static const auto QueryHeadsetId = MakeSystemPropertiesChecker(
        XrSystemHeadsetIdPropertiesMETA{XR_TYPE_SYSTEM_HEADSET_ID_PROPERTIES_META}, &XrSystemHeadsetIdPropertiesMETA::id);

    TEST_CASE("XR_META_headset_id", "[XR_META_headset_id]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsInstanceExtensionSupported(XR_META_HEADSET_ID_EXTENSION_NAME)) {
            SKIP(XR_META_HEADSET_ID_EXTENSION_NAME " not supported");
        }

        SECTION("Extension not enabled")
        {
            // validate that the extension has not been force enabled...
            if (!globalData.IsInstanceExtensionEnabled(XR_META_HEADSET_ID_EXTENSION_NAME)) {
                AutoBasicInstance instance(AutoBasicInstance::createSystemId);
                XrSystemId systemId = instance.systemId;

                XrUuidEXT headsetId = QueryHeadsetId(instance, systemId);

                // Validate headsetid has NOT been filled in
                XrUuidEXT empty{};
                REQUIRE(memcmp(&empty, &headsetId, sizeof(XrUuidEXT)) == 0);
            }
            else {
                WARN(XR_META_HEADSET_ID_EXTENSION_NAME " force-enabled, cannot test extension-disabled behavior.");
            }
        }

        SECTION("xrGetSystemProperties", "")
        {
            AutoBasicInstance instance({XR_META_HEADSET_ID_EXTENSION_NAME}, AutoBasicInstance::createSystemId);
            XrSystemId systemId = instance.systemId;

            SECTION("Valid UUID returned")
            {
                XrUuidEXT headsetId = QueryHeadsetId(instance, systemId);

                // Validate headsetid has been filled in
                XrUuidEXT empty{};
                REQUIRE(memcmp(&empty, &headsetId, sizeof(XrUuidEXT)) != 0);
            }

            SECTION("Consistent UUID returned")
            {
                XrUuidEXT headsetId1 = QueryHeadsetId(instance, systemId);
                XrUuidEXT headsetId2 = QueryHeadsetId(instance, systemId);

                // Validate headsetid is consistent
                REQUIRE(memcmp(&headsetId1, &headsetId2, sizeof(XrUuidEXT)) == 0);
            }
        }
    }

}  // namespace Conformance
