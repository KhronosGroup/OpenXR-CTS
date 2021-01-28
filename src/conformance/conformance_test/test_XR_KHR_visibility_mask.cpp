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

#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include <array>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    TEST_CASE("XR_KHR_visibility_mask", "")
    {
        // successcodes="XR_SUCCESS,XR_SESSION_LOSS_PENDING"
        // errorcodes="XR_ERROR_HANDLE_INVALID,XR_ERROR_INSTANCE_LOST,XR_ERROR_RUNTIME_FAILURE,XR_ERROR_VALIDATION_FAILURE,
        //             XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED,XR_ERROR_SIZE_INSUFFICIENT,XR_ERROR_SESSION_LOST,XR_ERROR_FUNCTION_UNSUPPORTED"
        //
        // XrResult xrGetVisibilityMaskKHR(XrSession session, XrViewConfigurationType viewConfigurationType,
        //              uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask);

        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported("XR_KHR_visibility_mask")) {
            return;
        }

        if (!globalData.IsUsingGraphicsPlugin()) {
            return;
        }

        XrResult result;
        AutoBasicInstance instance({"XR_KHR_visibility_mask"});
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

        // Verify that we can acquire the function.
        auto xrGetVisibilityMaskKHR_ =
            GetInstanceExtensionFunction<PFN_xrGetVisibilityMaskKHR>(session.GetInstance(), "xrGetVisibilityMaskKHR");
        REQUIRE(xrGetVisibilityMaskKHR_ != nullptr);

        // We need to exercise whatever view configuration type is active (currently mono, stereo, quad),
        // and retrieve masks for 1, 2, or 4 views respectively, depending on the view configuration type.
        // We need to exercise each of the mask visibility types hidden, visible, line.
        // We need to exercise the two call idiom (call once to get required capacities).

        const XrViewConfigurationType viewConfigurationType = globalData.options.viewConfigurationValue;
        uint32_t viewCount = 0;

        switch (viewConfigurationType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            viewCount = 1;
            break;

        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
            viewCount = 2;
            break;

        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO:
            viewCount = 4;
            break;

        case XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM:
        default:  // Unexpected value.
            break;
        }

        REQUIRE_MSG(viewCount != 0, "Unexpected XrViewConfigurationType. Update to XR_KHR_visibility_mask test needed.");

        auto isCounterClockwise = [](XrVector2f& a, XrVector2f& b, XrVector2f& c) -> bool {
            return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y) >= 0;
        };

        for (uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
            std::array<XrVisibilityMaskTypeKHR, 3> maskTypeArray{XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR,
                                                                 XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR,
                                                                 XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR};

            for (size_t m = 0; m != maskTypeArray.size(); ++m) {
                const XrVisibilityMaskTypeKHR maskType = maskTypeArray[m];

                XrVisibilityMaskKHR visibilityMask{XR_TYPE_VISIBILITY_MASK_KHR};

                // First call once to get the required vertex and index counts.
                result = xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask);
                REQUIRE(ValidateResultAllowed("xrGetVisibilityMaskKHR", result));
                CHECK_RESULT_SUCCEEDED(result);

                // Runtime may return 0 vertices/indices if no view mask is available.
                if (visibilityMask.indexCountOutput > 0) {
                    std::vector<XrVector2f> vertexVector(visibilityMask.vertexCountOutput);
                    std::vector<uint32_t> indexVector(visibilityMask.indexCountOutput);
                    visibilityMask.vertexCapacityInput = (uint32_t)vertexVector.size();
                    visibilityMask.vertices = vertexVector.data();
                    visibilityMask.indexCapacityInput = (uint32_t)indexVector.size();
                    visibilityMask.indices = indexVector.data();

                    // Call to get the full data. Expect success.
                    result = xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask);
                    REQUIRE(ValidateResultAllowed("xrGetVisibilityMaskKHR", result));
                    CHECK_RESULT_SUCCEEDED(result);

                    // Do some output validation
                    switch (maskType) {
                    case XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR:
                    case XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR: {
                        CHECK((visibilityMask.indexCountOutput % 3) == 0);
                        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; ++i) {
                            CHECK(visibilityMask.indices[i] < visibilityMask.vertexCountOutput);  // Index should be valid.
                        }

                        REQUIRE((visibilityMask.indexCountOutput % 3) == 0);
                        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; i += 3) {
                            // a, b, c should format a count-clockwise triangle
                            XrVector2f& a = visibilityMask.vertices[visibilityMask.indices[i]];
                            XrVector2f& b = visibilityMask.vertices[visibilityMask.indices[i + 1]];
                            XrVector2f& c = visibilityMask.vertices[visibilityMask.indices[i + 2]];
                            CHECK(isCounterClockwise(a, b, c));
                        }

                        break;
                    }

                    case XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR: {
                        CHECK(visibilityMask.indexCountOutput == visibilityMask.vertexCountOutput);
                        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; ++i) {
                            CHECK(visibilityMask.indices[i] < visibilityMask.vertexCountOutput);  // Index should be valid.
                        }

                        for (uint32_t i = 0; i < visibilityMask.indexCountOutput; ++i) {
                            // The line is counter-clockwise (around the origin)
                            XrVector2f origin{0, 0};
                            XrVector2f& a = visibilityMask.vertices[visibilityMask.indices[i]];
                            // With the last point implicitly connecting to the first point.
                            XrVector2f& b = visibilityMask.vertices[visibilityMask.indices[(i + 1) % visibilityMask.indexCountOutput]];
                            CHECK(isCounterClockwise(origin, a, b));
                        }

                        break;
                    }

                    case XR_VISIBILITY_MASK_TYPE_MAX_ENUM_KHR:
                    default:  // Unexpected value.
                        break;
                    }

                    // Exercise XR_ERROR_SIZE_INSUFFICIENT for vertex capacity input.
                    visibilityMask.vertexCapacityInput = 1;
                    result = xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask);
                    REQUIRE(ValidateResultAllowed("xrGetVisibilityMaskKHR", result));
                    CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);

                    // Exercise XR_ERROR_SIZE_INSUFFICIENT for index capacity input.
                    visibilityMask.vertexCapacityInput = (uint32_t)vertexVector.size();  // Restore this.
                    visibilityMask.indexCapacityInput = 1;
                    result = xrGetVisibilityMaskKHR_(session, viewConfigurationType, viewIndex, maskType, &visibilityMask);
                    REQUIRE(ValidateResultAllowed("xrGetVisibilityMaskKHR", result));
                    CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);
                    visibilityMask.indexCapacityInput = (uint32_t)indexVector.size();  // Restore this.
                }

                OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
                {
                    // Exercise XR_ERROR_HANDLE_INVALID
                    result = xrGetVisibilityMaskKHR_(XR_NULL_HANDLE_CPP, viewConfigurationType, viewIndex, maskType, &visibilityMask);
                    REQUIRE(ValidateResultAllowed("xrGetVisibilityMaskKHR", result));
                    REQUIRE(result == XR_ERROR_HANDLE_INVALID);

                    result =
                        xrGetVisibilityMaskKHR_(globalData.invalidSession, viewConfigurationType, viewIndex, maskType, &visibilityMask);
                    REQUIRE(ValidateResultAllowed("xrGetVisibilityMaskKHR", result));
                    REQUIRE(result == XR_ERROR_HANDLE_INVALID);
                }
            }
        }
    }  // namespace Conformance
}  // namespace Conformance
