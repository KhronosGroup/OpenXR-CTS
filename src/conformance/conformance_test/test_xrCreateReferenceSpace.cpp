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
#include "conformance_utils.h"
#include "two_call.h"
#include "matchers.h"

#include "utilities/xr_math_operators.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace Conformance
{
    using namespace openxr::math_operators;

    TEST_CASE("xrCreateReferenceSpace", "")
    {
        AutoBasicSession session(AutoBasicSession::createSession);

        // Get all supported reference space types and exercise them.
        auto refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session.GetSession());

        for (auto refSpaceType : refSpaceTypes) {
            INFO("Reference space type is " << refSpaceType);

            XrSpace referenceSpace{XR_NULL_HANDLE_CPP};
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            referenceSpaceCreateInfo.referenceSpaceType = refSpaceType;
            referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity;

            // Test a success case.
            CHECK(XR_SUCCESS == xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &referenceSpace));
            CHECK_FALSE(referenceSpace == XR_NULL_HANDLE_CPP);
            CHECK(XR_SUCCESS == xrDestroySpace(referenceSpace));

            // Runtimes should ignore unrecognized struct types.
            UnrecognizableInputStruct unknown;
            unknown.Insert(&referenceSpaceCreateInfo);
            XrResult result = xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &referenceSpace);
            CHECK(result == XR_SUCCESS);

            if (XR_SUCCEEDED(result)) {
                CHECK(xrDestroySpace(referenceSpace) == XR_SUCCESS);
            }

            {
                INFO("Exercise XR_ERROR_POSE_INVALID");
                {
                    CAPTURE(referenceSpaceCreateInfo.poseInReferenceSpace.orientation.w = 0);  // Make the quaternion invalid.
                    CHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &referenceSpace) == XR_ERROR_POSE_INVALID);
                }
                {
                    CAPTURE(referenceSpaceCreateInfo.poseInReferenceSpace.orientation.w = NAN);  // Make the quaternion invalid.
                    CHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &referenceSpace) == XR_ERROR_POSE_INVALID);
                }
                referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity;  // Restore it.
            }

            // Exercise other invalid handles.
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise NULL handle.
                result = xrCreateReferenceSpace(XR_NULL_HANDLE_CPP, &referenceSpaceCreateInfo, &referenceSpace);
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Exercise any invalid handle.
                result = xrCreateReferenceSpace(InvalidValues::InvalidHandleValue<XrSession>(), &referenceSpaceCreateInfo, &referenceSpace);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
            }
        }

        SECTION("Calling CreateReferenceSpace with nonexistent reference space type")
        {
            constexpr XrReferenceSpaceType InvalidReferenceSpaceTypes[] = {
                static_cast<XrReferenceSpaceType>(0),
                XR_REFERENCE_SPACE_TYPE_MAX_ENUM,
            };

            for (XrReferenceSpaceType xst : InvalidReferenceSpaceTypes) {
                INFO("XrReferenceSpaceType: " << xst);

                XrSpace referenceSpace{XR_NULL_HANDLE_CPP};
                XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                referenceSpaceCreateInfo.referenceSpaceType = xst;
                referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity;
                XrResult result = xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &referenceSpace);

                // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#_xrcreatereferencespace
                //
                // The runtime must return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED if the given reference space type is not supported by this session.

                // TYPE_MAX_ENUM is not a valid reference space.
                REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_REFERENCE_SPACE_UNSUPPORTED}));
                if (result == XR_ERROR_REFERENCE_SPACE_UNSUPPORTED) {
                    // It's better to return XR_ERROR_VALIDATION_FAILURE, though possibly not technically a requirement of the spec,
                    // depending on how you interpret the phrase "not supported by this session".
                    // Generally, in OpenXR, UNSUPPORTED means "this value is a legal or possibly legal value for this variable/type,
                    // but this runtime won't let you use it here and now"
                    WARN(
                        "Runtime responded to use of 0, XR_REFERENCE_SPACE_TYPE_MAX_ENUM (which is an invalid value) with XR_ERROR_REFERENCE_SPACE_UNSUPPORTED, which make it harder for apps to reason about the error.");
                }

                REQUIRE(referenceSpace == XR_NULL_HANDLE_CPP);
            }
        }

        SECTION("Enumerated reference spaces are creatable and vice versa")
        {
#define AS_LIST(name, val) name,
            constexpr XrReferenceSpaceType KnownReferenceSpaceTypes[] = {XR_LIST_ENUM_XrReferenceSpaceType(AS_LIST)};
#undef AS_LIST

            for (XrReferenceSpaceType xst : KnownReferenceSpaceTypes) {
                INFO("XrReferenceSpaceType: " << xst);

                // If the given core type wasn't enumerated by the runtime, make sure it isn't creatable.
                bool wasEnumerated = std::find(refSpaceTypes.begin(), refSpaceTypes.end(), xst) != refSpaceTypes.end();

                XrSpace referenceSpace{XR_NULL_HANDLE_CPP};
                XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                referenceSpaceCreateInfo.referenceSpaceType = xst;
                referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity;
                XrResult result = xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &referenceSpace);

                if (wasEnumerated) {
                    // We validate that enumerated spaces can be created in the first test in this file.
                    REQUIRE(result == XR_SUCCESS);
                    xrDestroySpace(referenceSpace);
                }
                else {
                    REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_REFERENCE_SPACE_UNSUPPORTED}));
                }
            }
        }
    }

}  // namespace Conformance
