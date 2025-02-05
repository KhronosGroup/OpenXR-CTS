// Copyright (c) 2019-2025 The Khronos Group Inc.
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
#include "matchers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Conformance
{
    TEST_CASE("ViewConfigurations", "")
    {
        // XrResult xrEnumerateViewConfigurations(XrInstance instance, XrSystemId systemId, uint32_t viewConfigurationTypeCapacityInput,
        // uint32_t* viewConfigurationTypeCountOutput, XrViewConfigurationType* viewConfigurationTypes); XrResult
        // xrGetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType,
        // XrViewConfigurationProperties* configurationProperties); XrResult xrEnumerateViewConfigurationViews(XrInstance instance,
        // XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput,
        // XrViewConfigurationView* views);

        AutoBasicInstance instance(AutoBasicInstance::createSystemId);

        uint32_t countOutput = 0;
        std::vector<XrViewConfigurationType> vctArray;

        // xrEnumerateViewConfigurations
        {
            // Test the 0-sized input mode.
            REQUIRE(xrEnumerateViewConfigurations(instance, instance.systemId, 0, &countOutput, nullptr) == XR_SUCCESS);

            if (countOutput) {
                REQUIRE_NOTHROW(vctArray.resize(countOutput, XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM));
                countOutput = 0;

                if (countOutput >= 2)  // The -1 below needs the result to be >0 because 0 is a special case as exercised above.
                {
                    // Exercise XR_ERROR_SIZE_INSUFFICIENT.
                    REQUIRE(xrEnumerateViewConfigurations(instance, instance.systemId, countOutput - 1, &countOutput, vctArray.data()) ==
                            XR_ERROR_SIZE_INSUFFICIENT);
                    REQUIRE_MSG(vctArray[countOutput - 1] == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM,
                                "xrEnumerateViewConfigurations write past capacity");

                    std::fill(vctArray.begin(), vctArray.end(), XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM);
                    countOutput = 0;
                }

                REQUIRE(xrEnumerateViewConfigurations(instance, instance.systemId, countOutput, &countOutput, vctArray.data()) ==
                        XR_SUCCESS);
                REQUIRE(countOutput == vctArray.size());
            }
        }

        // Ensure unsupported view configuration types fail.
        {
#define AS_LIST(name, val) name,
            constexpr XrViewConfigurationType KnownViewTypes[] = {XR_LIST_ENUM_XrViewConfigurationType(AS_LIST)};
#undef AS_LIST

            XrSystemId systemId = instance.systemId;

            // Get the list of supported view configurations
            uint32_t viewCount = 0;
            REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurations(instance, systemId, 0, &viewCount, nullptr));
            std::vector<XrViewConfigurationType> runtimeViewTypes(viewCount);
            REQUIRE(XR_SUCCESS == xrEnumerateViewConfigurations(instance, systemId, viewCount, &viewCount, runtimeViewTypes.data()));

            AutoBasicSession session(AutoBasicSession::createSession, instance);
            FrameIterator frameIterator(&session);
            frameIterator.RunToSessionState(XR_SESSION_STATE_READY);

            for (XrViewConfigurationType viewType : KnownViewTypes) {
                CAPTURE(viewType);

                // Is this enum valid, check against enabled extensions.
                bool valid = IsViewConfigurationTypeEnumValid(viewType);

                if (!IsViewConfigurationTypeEnumValid(viewType)) {
                    INFO("Must not enumerate invalid view configuration type");
                    CHECK_THAT(runtimeViewTypes, !Catch::Matchers::VectorContains(viewType));
                }

                // Skip this view config if it is supported, since we cannot test correct handling of unsupported values with it.
                if (Catch::Matchers::VectorContains(viewType).match(runtimeViewTypes)) {
                    continue;
                }

                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = viewType;
                XrResult result = xrBeginSession(session, &beginInfo);
                REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED}));
                if (!valid && result == XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED) {
                    WARN(
                        "On receiving an 'invalid' enum value "
                        << viewType
                        << ", the runtime returned as XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED instead of XR_ERROR_VALIDATION_FAILURE, which may make it harder for apps to reason about the error.");
                }
                else if (valid && result == XR_ERROR_VALIDATION_FAILURE) {
                    WARN(
                        "On receiving a 'valid' but not supported enum value "
                        << viewType
                        << ", the runtime returned as XR_ERROR_VALIDATION_FAILURE instead of XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED, which may make it harder for apps to reason about the error.");
                }
            }
        }

        // xrGetViewConfigurationProperties
        {
            // XrResult xrGetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType
            // viewConfigurationType, XrViewConfigurationProperties* configurationProperties);

            if (vctArray.size()) {
                XrViewConfigurationProperties vcp{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};

                // Need to enumerate again because the array was reset above.
                /// @todo restructure to use sections and avoid this.
                REQUIRE(xrEnumerateViewConfigurations(instance, instance.systemId, (uint32_t)vctArray.size(), &countOutput,
                                                      vctArray.data()) == XR_SUCCESS);

                for (XrViewConfigurationType vct : vctArray) {
                    INFO("XrViewConfigurationType: " << vct);
                    REQUIRE(xrGetViewConfigurationProperties(instance, instance.systemId, vct, &vcp) == XR_SUCCESS);
                    REQUIRE(vcp.viewConfigurationType == vct);

                    // We have nothing to say here about vcp.fovMutable. However, we will later want
                    // to use that when submitting frames to mutate the fov.
                }

                SECTION("Unrecognized extension")
                {
                    // Runtimes should ignore unrecognized struct extensins.
                    InsertUnrecognizableExtension(&vcp);
                    REQUIRE(xrGetViewConfigurationProperties(instance, instance.systemId, vctArray[0], &vcp) == XR_SUCCESS);
                    REQUIRE(vcp.viewConfigurationType == vctArray[0]);
                }

                // Exercise XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED
                XrResult result = xrGetViewConfigurationProperties(instance, instance.systemId, XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM, &vcp);
                REQUIRE_THAT(result, In<XrResult>({XR_ERROR_VALIDATION_FAILURE, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED}));
                if (result == XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED) {
                    WARN(
                        "Runtime accepted an invalid enum value as unsupported, which makes it harder for apps to reason about the error.");
                }
            }
        }

        // xrEnumerateViewConfigurationViews
        {
            // XrResult xrEnumerateViewConfigurationViews(instance, instance.systemId, XrViewConfigurationType viewConfigurationType,
            //                 uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views);

            for (XrViewConfigurationType vct : vctArray) {
                std::vector<XrViewConfigurationView> vcvArray;

                REQUIRE(xrEnumerateViewConfigurationViews(instance, instance.systemId, vct, 0, &countOutput, nullptr) == XR_SUCCESS);
                CHECK_MSG(countOutput > 0, "Viewport configuration provides no views.");

                if (countOutput) {
                    const XrViewConfigurationView initView{
                        XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

                    REQUIRE_NOTHROW(vcvArray.resize(countOutput, initView));

                    if (countOutput >= 2)  // The -1 below needs the result to be >0 because 0 is a special case as exercised above.
                    {
                        SECTION("Exercise XR_ERROR_SIZE_INSUFFICIENT")
                        {
                            REQUIRE(xrEnumerateViewConfigurationViews(instance, instance.systemId, vct, countOutput - 1, &countOutput,
                                                                      vcvArray.data()) == XR_ERROR_SIZE_INSUFFICIENT);
                            REQUIRE_MSG(vcvArray[countOutput - 1].recommendedImageRectWidth == UINT32_MAX,
                                        "xrEnumerateViewConfigurationViews write past capacity");
                        }
                    }

                    SECTION("Normal call")
                    {
                        REQUIRE(xrEnumerateViewConfigurationViews(instance, instance.systemId, vct, countOutput, &countOutput,
                                                                  vcvArray.data()) == XR_SUCCESS);
                        REQUIRE(countOutput == vcvArray.size());

                        // At this point we have an array of XrViewConfigurationView.
                        for (XrViewConfigurationView view : vcvArray) {
                            // To do: validate these to the extent possible.
                            REQUIRE(view.type == XR_TYPE_VIEW_CONFIGURATION_VIEW);
                            REQUIRE(view.next == nullptr);
                            (void)view.recommendedImageRectWidth;
                            (void)view.maxImageRectWidth;
                            (void)view.recommendedImageRectHeight;
                            (void)view.maxImageRectHeight;
                            (void)view.recommendedSwapchainSampleCount;
                            (void)view.maxSwapchainSampleCount;
                        }
                    }

                    SECTION("Unrecognized extension")
                    {
                        // Runtimes should ignore unrecognized struct extensins.
                        InsertUnrecognizableExtensionArray(vcvArray.data(), vcvArray.size());
                        REQUIRE(xrEnumerateViewConfigurationViews(instance, instance.systemId, vct, countOutput, &countOutput,
                                                                  vcvArray.data()) == XR_SUCCESS);
                    }

                    OPTIONAL_INVALID_TYPE_VALIDATION_SECTION
                    {
                        XrViewConfigurationView invalidInitView{XR_TYPE_UNKNOWN};
                        invalidInitView.recommendedImageRectWidth = UINT32_MAX;
                        invalidInitView.maxImageRectWidth = UINT32_MAX;
                        invalidInitView.recommendedImageRectHeight = UINT32_MAX;
                        invalidInitView.maxImageRectHeight = UINT32_MAX;
                        invalidInitView.recommendedSwapchainSampleCount = UINT32_MAX;
                        invalidInitView.maxSwapchainSampleCount = UINT32_MAX;

                        std::vector<XrViewConfigurationView> invalidVcvArray(countOutput, invalidInitView);
                        REQUIRE(xrEnumerateViewConfigurationViews(instance, instance.systemId, vct, countOutput, &countOutput,
                                                                  invalidVcvArray.data()) == XR_ERROR_VALIDATION_FAILURE);
                    }
                }
            }
        }
    }

}  // namespace Conformance
