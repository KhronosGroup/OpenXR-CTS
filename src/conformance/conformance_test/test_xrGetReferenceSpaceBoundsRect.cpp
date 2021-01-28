// Copyright (c) 2019-2021, The Khronos Group Inc.
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

#include "conformance_utils.h"
#include "conformance_framework.h"
#include "two_call.h"
#include "matchers.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    TEST_CASE("xrGetReferenceSpaceBoundsRect", "")
    {
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession);  // Create instance and session.

        XrResult result;

        // Get all supported reference space types and exercise them.
        auto spaceTypeVector = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);

        XrExtent2Df extent{-1.f, -1.f};
        // Note: View has to be supported and we already checked this in the xrEnumerateReferenceSpaces tests.

        // Max is not a valid reference space.
        result = xrGetReferenceSpaceBoundsRect(session, XR_REFERENCE_SPACE_TYPE_MAX_ENUM, &extent);
        REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_REFERENCE_SPACE_UNSUPPORTED}));
        if (result == XR_ERROR_REFERENCE_SPACE_UNSUPPORTED) {
            WARN("Runtime accepted an invalid max enum value as unsupported, which make it harder for apps to reason about the error.");
        }

        // Exercise other invalid handles.
        OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
        {
            // Exercise a NULL handle:
            result = xrGetReferenceSpaceBoundsRect(XR_NULL_HANDLE_CPP, XR_REFERENCE_SPACE_TYPE_VIEW, &extent);
            CHECK(result == XR_ERROR_HANDLE_INVALID);

            // Exercise other invalid handles.
            result = xrGetReferenceSpaceBoundsRect(GlobalData().invalidSession, XR_REFERENCE_SPACE_TYPE_VIEW, &extent);
            CHECK(result == XR_ERROR_HANDLE_INVALID);
        }

        // get bounds for all supported space types:
        for (XrReferenceSpaceType rst : spaceTypeVector) {
            XrExtent2Df bounds{-1.f, -1.f};
            result = xrGetReferenceSpaceBoundsRect(session, rst, &bounds);
            REQUIRE_THAT(result, In<XrResult>({XR_SUCCESS, XR_SPACE_BOUNDS_UNAVAILABLE}));
            CAPTURE(bounds.width);
            CAPTURE(bounds.height);
            CHECK(!std::isnan(bounds.width));
            CHECK(!std::isnan(bounds.height));

            if (result == XR_SUCCESS) {
                CHECK(bounds.width > 0);
                CHECK(bounds.height > 0);
            }
            else if (result == XR_SPACE_BOUNDS_UNAVAILABLE) {
                CHECK(bounds.width == 0);
                CHECK(bounds.height == 0);
            }
        }
    }
}  // namespace Conformance
