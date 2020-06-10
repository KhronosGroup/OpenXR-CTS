// Copyright (c) 2019-2020 The Khronos Group Inc.
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
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{

    TEST_CASE("xrCreateReferenceSpace", "")
    {
        AutoBasicSession session(AutoBasicSession::createSession);

        // Get all supported reference space types and exercise them.
        auto refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);

        for (auto refSpaceType : refSpaceTypes) {
            INFO("Reference space type is " << refSpaceType);
            XrSpace localSpace = XR_NULL_HANDLE_CPP;
            XrReferenceSpaceCreateInfo reference_space_create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr, refSpaceType,
                                                                   XrPosefCPP()};

            // Test a success case.
            CHECK(XR_SUCCESS == xrCreateReferenceSpace(session, &reference_space_create_info, &localSpace));
            CHECK_FALSE(localSpace == XR_NULL_HANDLE_CPP);
            CHECK(XR_SUCCESS == xrDestroySpace(localSpace));

            // Runtimes should ignore unrecognized struct extensions.
            InsertUnrecognizableExtension(&reference_space_create_info);
            XrResult result = xrCreateReferenceSpace(session, &reference_space_create_info, &localSpace);
            CHECK(result == XR_SUCCESS);
            if (XR_SUCCEEDED(result))
                CHECK(xrDestroySpace(localSpace) == XR_SUCCESS);

            // Exercise XR_ERROR_POSE_INVALID.
            reference_space_create_info.poseInReferenceSpace.orientation.w = 0;  // Make the quaternion invalid.
            CHECK(xrCreateReferenceSpace(session, &reference_space_create_info, &localSpace) == XR_ERROR_POSE_INVALID);
            reference_space_create_info.poseInReferenceSpace = XrPosefCPP();  // Restore it.

            // Exercise other invalid handles.
            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // Exercise NULL handle.
                result = xrCreateReferenceSpace(XR_NULL_HANDLE_CPP, &reference_space_create_info, &localSpace);
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Exercise any invalid handle.
                result = xrCreateReferenceSpace(GlobalData().invalidSession, &reference_space_create_info, &localSpace);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
            }
        }
        SECTION("Calling CreateReferenceSpace with nonexistent reference space type")
        {
            XrSpace localSpace = XR_NULL_HANDLE_CPP;
            XrReferenceSpaceCreateInfo reference_space_create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr,
                                                                   XR_REFERENCE_SPACE_TYPE_MAX_ENUM, XrPosefCPP()};
            CHECK(XR_ERROR_REFERENCE_SPACE_UNSUPPORTED == xrCreateReferenceSpace(session, &reference_space_create_info, &localSpace));
            REQUIRE(localSpace == XR_NULL_HANDLE_CPP);

            // To do: Auto-generate the list of known core space types.
            for (XrReferenceSpaceType xst : {XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_STAGE}) {
                // If the given core type wasn't enumerated by the runtime, make sure it isn't creatable.
                if (std::find(refSpaceTypes.begin(), refSpaceTypes.end(), xst) == refSpaceTypes.end()) {
                    reference_space_create_info.referenceSpaceType = xst;
                    CHECK(xrCreateReferenceSpace(session, &reference_space_create_info, &localSpace) ==
                          XR_ERROR_REFERENCE_SPACE_UNSUPPORTED);
                }
            }
        }
    }

}  // namespace Conformance
