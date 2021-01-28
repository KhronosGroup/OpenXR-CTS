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

#include "swapchain_parameters.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "two_call.h"
#include "bitmask_generator.h"
#include <catch2/catch.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    static inline void checkCreateSwapchain(AutoBasicSession& session, int64_t format, const XrViewConfigurationView& vcv)
    {
        auto width = vcv.recommendedImageRectWidth;
        auto height = vcv.recommendedImageRectHeight;

        GlobalData& globalData = GetGlobalData();
        auto graphicsPlugin = globalData.GetGraphicsPlugin();
        auto formatName = graphicsPlugin->GetImageFormatName(format) + " " + std::to_string(width) + "x" + std::to_string(height);
        CAPTURE(formatName);

        SwapchainCreateTestParameters tp{};
        CHECK(graphicsPlugin->GetSwapchainCreateTestParameters(session.instance, session, session.systemId, format, &tp));

        XrSwapchainCreateInfo createInfo{
            XR_TYPE_SWAPCHAIN_CREATE_INFO,
            nullptr,
            XrSwapchainCreateFlags(0),
            tp.usageFlagsVector[0],
            format,
            uint32_t(1),  // sampleCount
            width,
            height,
            uint32_t(1),  // face count
            uint32_t(1),  // array size
            uint32_t(1),  // mip count
        };

        XrSwapchain swapchain = XR_NULL_HANDLE_CPP;
        XrResult result = xrCreateSwapchain(session, &createInfo, &swapchain);
        if (XR_SUCCEEDED(result)) {
            CHECK(swapchain != XR_NULL_HANDLE_CPP);
            CHECK_RESULT_SUCCEEDED(xrDestroySwapchain(swapchain));
        }
        else {
            CHECK_RESULT_SUCCEEDED(result);
        }
    }

    TEST_CASE("xrCreateSwapchain")
    {
        AutoBasicSession session(AutoBasicSession::createSession);

        if (!GetGlobalData().IsUsingGraphicsPlugin()) {
            auto formats = REQUIRE_TWO_CALL(int64_t, {}, xrEnumerateSwapchainFormats, session);
            SECTION("Headless shouldn't provide any swapchain formats")
            {
                REQUIRE(formats.empty());
            }
            return;
        }

        auto formats = REQUIRE_TWO_CALL(int64_t, {}, xrEnumerateSwapchainFormats, session);
        SECTION("A non-headless session should provide at least one swapchain format")
        {
            REQUIRE(formats.size() > 0);
        }
        auto viewConfigTypes =
            REQUIRE_TWO_CALL(XrViewConfigurationType, {}, xrEnumerateViewConfigurations, session.GetInstance(), session.GetSystemId());

        for (auto viewConfig : viewConfigTypes) {
            DYNAMIC_SECTION("Using enumerated view config " << viewConfig)
            {
                // const XrViewConfigurationView empty = {XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr, }
                auto viewConfigViews =
                    REQUIRE_TWO_CALL(XrViewConfigurationView, {XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr, 0, 0, 0, 0, 0, 0},
                                     xrEnumerateViewConfigurationViews, session.GetInstance(), session.GetSystemId(), viewConfig);

                for (auto format : formats) {
                    // runtimes might support formats which are unknown to the conformance tests
                    // in which case no test is performed due to the lack of matching parameters to test
                    if (GetGlobalData().graphicsPlugin->IsImageFormatKnown(format)) {
                        DYNAMIC_SECTION("using enumerated swapchain format " << format)
                        {
                            const XrViewConfigurationView& view = viewConfigViews.front();
                            checkCreateSwapchain(session, format, view);

                            // Do this to allow the graphics to potentially purge the memory associated with the swap chain.
                            // Normally apps don't need to do this, but we are creating and destroying many swapchains
                            // in succession, which is an unusual thing.
                            GetGlobalData().graphicsPlugin->Flush();
                        }
                    }
                }
            }
        }
    }
}  // namespace Conformance
