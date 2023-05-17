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

#include "conformance_utils.h"
#include "conformance_framework.h"
#include "two_call.h"
#include "matchers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    using Catch::Matchers::VectorContains;

    TEST_CASE("xrEnumerateReferenceSpaces")
    {
        GlobalData& globalData = GetGlobalData();
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession);

        SECTION("Normal reference space enumeration")
        {
            // Get all supported reference space types
            std::vector<XrReferenceSpaceType> refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);

            // at least VIEW and LOCAL need to be provided
            REQUIRE_FALSE(refSpaceTypes.size() < 2);

            THEN("Each reference space type should be recognized")
            {
                // Currently there are three types recognized. No others may be returned by a version-matching runtime.
                if (globalData.runtimeMatchesAPIVersion) {
                    for (XrReferenceSpaceType refSpaceType : refSpaceTypes) {
                        if (refSpaceType < 1000000000) {  // If it's a core type
                            CHECK_THAT(refSpaceType, In<XrReferenceSpaceType>({XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL,
                                                                               XR_REFERENCE_SPACE_TYPE_STAGE}));
                        }
                    }
                }
            }
            THEN("Local and view spaces are required to be provided")
            {
                CHECK_THAT(refSpaceTypes, VectorContains(XR_REFERENCE_SPACE_TYPE_LOCAL));
                CHECK_THAT(refSpaceTypes, VectorContains(XR_REFERENCE_SPACE_TYPE_VIEW));
            }
            // Verify that no space type is enumerated more than once.
            CHECK_THAT(refSpaceTypes, VectorHasOnlyUniqueElements<XrReferenceSpaceType>());
        }

        SECTION("wrong input to xrEnumerateReferenceSpaces")
        {
            XrResult result;
            uint32_t capacity;
            std::vector<XrReferenceSpaceType> refSpaceTypes(1, XR_REFERENCE_SPACE_TYPE_MAX_ENUM);

            // we know (and have tested before) that at least two spaces are supported, VIEW and LOCAL, so 1 is definitely too small
            result = xrEnumerateReferenceSpaces(session, 1, &capacity, refSpaceTypes.data());
            CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                // NULL handle
                result = xrEnumerateReferenceSpaces(XR_NULL_HANDLE_CPP, 0, &capacity, nullptr);
                CHECK(result == XR_ERROR_HANDLE_INVALID);

                // Other invalid handle.
                result = xrEnumerateReferenceSpaces(GlobalData().invalidSession, 0, &capacity, nullptr);
                CHECK(result == XR_ERROR_HANDLE_INVALID);
            }
        }
    }

}  // namespace Conformance
