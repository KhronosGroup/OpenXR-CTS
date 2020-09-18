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

#include "utils.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "matchers.h"
#include "swapchain_parameters.h"
#include "report.h"
#include <openxr/openxr.h>
#include <catch2/catch.hpp>
#include <vector>
#include <set>

XRC_DISABLE_MSVC_WARNING(4505)  // unreferenced local function has been removed

namespace Conformance
{
    static bool TestSwapchainHandle(int64_t imageFormat, const SwapchainCreateTestParameters* tp, const XrSwapchainCreateInfo* createInfo,
                                    XrSwapchain swapchain)
    {
        const GlobalData& globalData = GetGlobalData();
        uint32_t imageCount = 0;  // Not known until we first call xrEnumerateSwapchainImages.
        {
            INFO("ValidateSwapchainImages internally exercises xrEnumerateSwapchainImages.");
            REQUIRE(globalData.graphicsPlugin->ValidateSwapchainImages(imageFormat, tp, swapchain, &imageCount));
            REQUIRE(imageCount > 0);
        }

        // xrEnumerateSwapchainImages
        {
            // Currently this is handled by GraphicsPlugin::ValidateSwapchainImages above, but we could
            // provide some generic handling here by getting the size of the platform-specific
            // XrSwapchainImage_KHR struct and treating it as a black box.
        }

        // xrAcquireSwapchainImage, xrWaitSwapchainImage, xrReleaseSwapchainImage
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // "xrAcquireSwapchainImage: The runtime must allow the application to acquire more than one image
            //  from a single swapchain at a time, for example if the application implements a multiple frame
            //  deep rendering pipeline."
            //
            // "xrAcquireSwapchainImage: Acquires the image corresponding to the index position in the array returned
            //  by xrEnumerateSwapchainImages. The runtime must return XR_ERROR_CALL_ORDER_INVALID if index has already
            //  been acquired and not yet released with xrReleaseSwapchainImage. If the swapchain was created with
            //  the XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT set in XrSwapchainCreateInfo::createFlags, this function must
            //  not have been previously called for this swapchain."
            //
            // "xrAcquireSwapchainImage: acquireInfo exists for extensibility purposes, it is NULL or a pointer
            //  to a valid XrSwapchainImageAcquireInfo."
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // "xrWaitSwapchainImage will implicitly wait on the oldest acquired swapchain image which has not yet been
            //  successfully waited on. Once a swapchain image has been successfully waited on, it must be released
            //  before waiting on the next acquired swapchain image."
            //
            // "xrWaitSwapchainImage: If the timeout expires without the image becoming available for writing,
            //  XR_TIMEOUT_EXPIRED is returned. If xrWaitSwapchainImage returns XR_TIMEOUT_EXPIRED, the next call
            //  to xrWaitSwapchainImage will wait on the same image index again until the function succeeds with XR_SUCCESS."
            //
            // "xrWaitSwapchainImage: The runtime must return XR_ERROR_CALL_ORDER_INVALID if no image has been
            //  acquired by calling xrAcquireSwapchainImage."
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            // "xrReleaseSwapchainImage: The swapchain image must have been successfully waited on before it is released."
            //
            // "xrReleaseSwapchainImage: The runtime must return XR_ERROR_CALL_ORDER_INVALID if no image has been waited
            //  on by calling xrWaitSwapchainImage."
            //
            // "xrReleaseSwapchainImage: If releaseInfo is not NULL, releaseInfo must be a pointer to a
            //  valid XrSwapchainImageReleaseInfo structure"
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////

            XrSwapchainImageAcquireInfo imageAcquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            std::vector<uint32_t> indexVector(imageCount, UINT32_MAX);
            uint32_t index;

            {
                // Verify that a wait on a non-acquired swapchain image results in XR_ERROR_CALL_ORDER_INVALID.
                XrSwapchainImageWaitInfo imageWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                imageWaitInfo.timeout = 0;
                CHECK(xrWaitSwapchainImage(swapchain, &imageWaitInfo) == XR_ERROR_CALL_ORDER_INVALID);
            }

            {
                // Verify that a release on a non-acquired swapchain image results in XR_ERROR_CALL_ORDER_INVALID.
                XrSwapchainImageReleaseInfo imageReleaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK(xrReleaseSwapchainImage(swapchain, &imageReleaseInfo) == XR_ERROR_CALL_ORDER_INVALID);
            }

            for (uint32_t i = 0; i < imageCount; ++i) {
                // For odd values of i we exercise that runtimes must accept NULL image acquire info.
                const XrSwapchainImageAcquireInfo* imageAcquireInfoToUse = ((i % 2) ? &imageAcquireInfo : nullptr);
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrAcquireSwapchainImage(swapchain, imageAcquireInfoToUse, &indexVector[i]));

                REQUIRE(globalData.graphicsPlugin->ValidateSwapchainImageState(swapchain, indexVector[i], imageFormat));

                // Verify that a release on a non-waited swapchain image results in XR_ERROR_CALL_ORDER_INVALID.
                XrSwapchainImageReleaseInfo imageReleaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK(xrReleaseSwapchainImage(swapchain, &imageReleaseInfo) == XR_ERROR_CALL_ORDER_INVALID);
            }

            {
                // At this point, all images should be acquired, but we've wait/released none of them.
                // Another acquire should result in XR_ERROR_CALL_ORDER_INVALID.
                CHECK(xrAcquireSwapchainImage(swapchain, &imageAcquireInfo, &index) == XR_ERROR_CALL_ORDER_INVALID);
            }

            // Wait/release all the images.
            for (uint32_t i = 0; i < imageCount; ++i) {
                XrSwapchainImageWaitInfo imageWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                imageWaitInfo.timeout = 500_xrMilliseconds;  // Call can block waiting for image to become available for writing.
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrWaitSwapchainImage(swapchain, &imageWaitInfo));

                // Another wait should fail with XR_ERROR_CALL_ORDER_INVALID.
                CHECK(xrWaitSwapchainImage(swapchain, &imageWaitInfo) == XR_ERROR_CALL_ORDER_INVALID);

                // For odd values of i we exercise that runtimes must accept NULL image release info.
                XrSwapchainImageReleaseInfo imageReleaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                const XrSwapchainImageReleaseInfo* imageReleaseInfoToUse = ((i % 2) ? &imageReleaseInfo : nullptr);
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrReleaseSwapchainImage(swapchain, imageReleaseInfoToUse));
            }

            // XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT requirement of single acquire.
            if (createInfo->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
                // In this case we can only ever acquire once.
                CHECK(xrAcquireSwapchainImage(swapchain, &imageAcquireInfo, &index) == XR_ERROR_CALL_ORDER_INVALID);
            }

            // To do: Is there a way to exercise xrWaitSwapchainImage XR_TIMEOUT_EXPIRED? It seems that the only
            // way this can happen is if the runtime is busy with an image despite successfully acquiring it.

            OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION
            {
                CHECK(xrAcquireSwapchainImage(XR_NULL_HANDLE_CPP, &imageAcquireInfo, &index) == XR_ERROR_HANDLE_INVALID);
            }
        }

        return true;
    }  // TestSwapchainHandle

    TEST_CASE("Swapchains", "")
    {
        const GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no swapchain
            return;
        }

        // Set up the session we will use for the testing
        AutoBasicSession session(AutoBasicSession::OptionFlags::beginSession);

        std::vector<int64_t> imageFormatArray;
        const int64_t imageFormatInvalid = XRC_INVALID_IMAGE_FORMAT;

        // xrEnumerateSwapchainFormats
        {
            uint32_t countOutput = 0;

            // Exercise zero input size.
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr));
            if (countOutput > 0) {
                imageFormatArray.resize(countOutput, imageFormatInvalid);
            }

            SECTION("Exercise XR_ERROR_SIZE_INSUFFICIENT")
            {
                if (countOutput >= 2) {  // Need at least two in order to exercise XR_ERROR_SIZE_INSUFFICIENT
                    CHECK_MSG(xrEnumerateSwapchainFormats(session, 1, &countOutput, imageFormatArray.data()), XR_ERROR_SIZE_INSUFFICIENT);
                    INFO("Should not overwrite input data");
                    CHECK_MSG(imageFormatArray[1] == imageFormatInvalid, "Should not overwrite input data.");
                    CHECK_MSG(countOutput == imageFormatArray.size(), "Should not change required size unexpectectedly.");
                }

                else {
                    WARN("Cannot check: not enough swapchain image formats to exercise XR_ERROR_SIZE_INSUFFICIENT");
                }
            }

            {
                // Exercise enough capacity
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(
                    xrEnumerateSwapchainFormats(session, countOutput, &countOutput, imageFormatArray.data()));

                REQUIRE_THAT(imageFormatArray, VectorHasOnlyUniqueElements<int64_t>());
                REQUIRE_THAT(imageFormatArray, !Catch::Matchers::VectorContains(imageFormatInvalid));

                SECTION("Swapchain creation test parameters")
                {
                    // At this point, session.viewConfigurationViewVector has the system's set of view configurations,
                    // and imageFormatArray has the supported set of image formats.

                    // xrCreateSwapchain / xrDestroySwapchain
                    // session.viewConfigurationViewVector may have more than one entry, and each entry has different
                    // values for recommended and max sizes/counts. There's currently no association with a
                    // swapchain and view configuration.
                    for (int64_t imageFormat : imageFormatArray) {

                        SwapchainCreateTestParameters tp;
                        REQUIRE(globalData.graphicsPlugin->GetSwapchainCreateTestParameters(session.instance, session, session.systemId,
                                                                                            imageFormat, &tp));

                        ReportF("Testing format %s", tp.imageFormatName.c_str());
                        int swapchainCreateCount = 0;

                        auto createDefaultSwapchain = [&] {
                            XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                            // Exercise presence of unrecognized extensions, which the runtime should ignore.
                            InsertUnrecognizableExtension(&createInfo);
                            CAPTURE(createInfo.faceCount = 1);  // We let cubemap extensions exercise faceCount == 6.
                            CAPTURE(createInfo.format = imageFormat);
                            CAPTURE(createInfo.createFlags = (XrSwapchainCreateFlags)tp.createFlagsVector[0]);
                            CAPTURE(createInfo.usageFlags = (XrSwapchainUsageFlags)tp.usageFlagsVector[0]);
                            CAPTURE(createInfo.sampleCount = 1);
                            CAPTURE(createInfo.width = 64);
                            CAPTURE(createInfo.height = 64);
                            CAPTURE(createInfo.arraySize = tp.arrayCountVector[0]);
                            CAPTURE(createInfo.mipCount = tp.mipCountVector[0]);
                            return createInfo;
                        };

                        auto testSwapchainCreation = [&](const XrSwapchainCreateInfo& swapchainCreateInfo) {
                            swapchainCreateCount++;

                            XrSwapchain swapchain;
                            XrResult result = xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain);
                            CHECK(ValidateResultAllowed("xrCreateSwapchain", result));
                            REQUIRE(((result == XR_SUCCESS) || (result == XR_ERROR_FEATURE_UNSUPPORTED)));

                            if (XR_SUCCEEDED(result)) {
                                TestSwapchainHandle(imageFormat, &tp, &swapchainCreateInfo, swapchain);

                                result = xrDestroySwapchain(swapchain);
                                CHECK_RESULT_SUCCEEDED(result);

                                globalData.graphicsPlugin->Flush();
                            }
                        };

                        {
                            auto createInfo = createDefaultSwapchain();
                            // Smallest compressed texture size is 4x4, use 8x8 to allow for future formats
                            CAPTURE(createInfo.width = 8);
                            CAPTURE(createInfo.height = 8);
                            testSwapchainCreation(createInfo);
                        }

                        for (const auto& size : session.viewConfigurationViewVector) {
                            {
                                auto createInfo = createDefaultSwapchain();
                                CAPTURE(createInfo.width = size.recommendedImageRectWidth);
                                CAPTURE(createInfo.height = size.recommendedImageRectHeight);
                                testSwapchainCreation(createInfo);
                            }
                            {
                                auto createInfo = createDefaultSwapchain();
                                CAPTURE(createInfo.width = size.maxImageRectWidth);
                                CAPTURE(createInfo.height = size.maxImageRectHeight);
                                testSwapchainCreation(createInfo);
                            }

                            if (!tp.compressedFormat) {
                                auto createInfo = createDefaultSwapchain();
                                {
                                    CAPTURE(createInfo.sampleCount = size.recommendedSwapchainSampleCount);
                                    testSwapchainCreation(createInfo);
                                }
                                {
                                    CAPTURE(createInfo.sampleCount = size.maxSwapchainSampleCount);
                                    testSwapchainCreation(createInfo);
                                }
                            }
                        }

                        for (const auto& cf : tp.createFlagsVector) {
                            auto createInfo = createDefaultSwapchain();
                            CAPTURE(createInfo.createFlags = (XrSwapchainCreateFlags)cf);
                            testSwapchainCreation(createInfo);
                        }

                        for (const auto& sc : tp.sampleCountVector) {
                            auto createInfo = createDefaultSwapchain();
                            CAPTURE(createInfo.sampleCount = sc);
                            testSwapchainCreation(createInfo);
                        }

                        for (const auto& uf : tp.usageFlagsVector) {
                            auto createInfo = createDefaultSwapchain();
                            CAPTURE(createInfo.usageFlags = (XrSwapchainUsageFlags)uf);
                            testSwapchainCreation(createInfo);
                        }

                        for (const auto& ac : tp.arrayCountVector) {
                            auto createInfo = createDefaultSwapchain();
                            CAPTURE(createInfo.arraySize = ac);
                            testSwapchainCreation(createInfo);
                        }

                        for (const auto& mc : tp.mipCountVector) {
                            auto createInfo = createDefaultSwapchain();
                            CAPTURE(createInfo.mipCount = mc);
                            testSwapchainCreation(createInfo);
                        }
                        ReportF("    %d cases tested", swapchainCreateCount);
                    }
                }
            }
        }
    }
}  // namespace Conformance
