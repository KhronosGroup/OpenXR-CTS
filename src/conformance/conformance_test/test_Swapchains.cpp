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

#include "bitmask_to_string.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "graphics_plugin.h"
#include "matchers.h"
#include "report.h"
#include "swapchain_image_data.h"
#include "swapchain_parameters.h"
#include "types_and_constants.h"
#include "utils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <openxr/openxr.h>

#include <vector>

XRC_DISABLE_MSVC_WARNING(4505)  // unreferenced local function has been removed

namespace Conformance
{

    static void DoRenderTest(ISwapchainImageData* swapchainImages, uint32_t colorImageCount, const XrSwapchainCreateInfo& colorCreateInfo,
                             XrSwapchain colorSwapchain)
    {

        XrCompositionLayerProjectionView projectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projectionView.pose = XrPosef{{0, 0, 0, 1}, {0, 0, 0}};
        projectionView.fov = {1, 1, 1, 1};
        projectionView.subImage.swapchain = colorSwapchain;
        projectionView.subImage.imageRect = {{0, 0}, {(int32_t)colorCreateInfo.width, (int32_t)colorCreateInfo.height}};
        projectionView.subImage.imageArrayIndex = 0;

        XrSwapchainImageWaitInfo imageWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        imageWaitInfo.timeout = 500_xrMilliseconds;

        // acquire/wait/render/release all the images.
        for (uint32_t i = 0; i < colorImageCount; ++i) {
            CAPTURE(i);
            uint32_t colorImageIndex = UINT32_MAX;

            {
                INFO("Acquire the color image");
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrAcquireSwapchainImage(colorSwapchain, nullptr, &colorImageIndex));

                REQUIRE(
                    GetGlobalData().graphicsPlugin->ValidateSwapchainImageState(colorSwapchain, colorImageIndex, colorCreateInfo.format));
            }
            {
                INFO("Wait on the color image");
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrWaitSwapchainImage(colorSwapchain, &imageWaitInfo));
            }
            {
                INFO("Acquire the depth image if applicable");
                swapchainImages->AcquireAndWaitDepthSwapchainImage(colorImageIndex);
            }
            {
                INFO("Rendering to the swapchain(s)");
                const XrSwapchainImageBaseHeader* image = swapchainImages->GetGenericColorImage(i);
                GetGlobalData().graphicsPlugin->ClearImageSlice(image, 0);
                GetGlobalData().graphicsPlugin->RenderView(projectionView, image, {});
            }
            {
                INFO("Release the depth image if applicable");
                swapchainImages->ReleaseDepthSwapchainImage();
            }
            {
                INFO("Release the color image");
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrReleaseSwapchainImage(colorSwapchain, nullptr));
            }
        }
    }

    static bool TestSwapchainHandle(int64_t imageFormat, const SwapchainCreateTestParameters* tp, const XrSwapchainCreateInfo& createInfo,
                                    XrSwapchain swapchain)
    {
        uint32_t imageCount = 0;  // Not known until we first call xrEnumerateSwapchainImages.

        {
            INFO("ValidateSwapchainImages internally exercises xrEnumerateSwapchainImages.");
            REQUIRE(GetGlobalData().graphicsPlugin->ValidateSwapchainImages(createInfo.format, tp, swapchain, &imageCount));
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
                INFO("Verify that a wait on a non-acquired swapchain image results in XR_ERROR_CALL_ORDER_INVALID.");
                XrSwapchainImageWaitInfo imageWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                imageWaitInfo.timeout = 0;
                CHECK(xrWaitSwapchainImage(swapchain, &imageWaitInfo) == XR_ERROR_CALL_ORDER_INVALID);
            }

            {
                INFO("Verify that a release on a non-acquired swapchain image results in XR_ERROR_CALL_ORDER_INVALID.");
                XrSwapchainImageReleaseInfo imageReleaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK(xrReleaseSwapchainImage(swapchain, &imageReleaseInfo) == XR_ERROR_CALL_ORDER_INVALID);
            }
            {
                INFO("Acquiring all swapchain images");
                for (uint32_t i = 0; i < imageCount; ++i) {
                    // For odd values of i we exercise that runtimes must accept NULL image acquire info.
                    const XrSwapchainImageAcquireInfo* imageAcquireInfoToUse = ((i % 2) ? &imageAcquireInfo : nullptr);
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrAcquireSwapchainImage(swapchain, imageAcquireInfoToUse, &indexVector[i]));

                    REQUIRE(GetGlobalData().graphicsPlugin->ValidateSwapchainImageState(swapchain, indexVector[i], imageFormat));

                    // Verify that a release on a non-waited swapchain image results in XR_ERROR_CALL_ORDER_INVALID.
                    XrSwapchainImageReleaseInfo imageReleaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    CHECK(xrReleaseSwapchainImage(swapchain, &imageReleaseInfo) == XR_ERROR_CALL_ORDER_INVALID);
                }
            }
            {
                // At this point, all images should be acquired, but we've wait/released none of them.
                // Another acquire should result in XR_ERROR_CALL_ORDER_INVALID.
                INFO("An extra acquire once we acquired all should be XR_ERROR_CALL_ORDER_INVALID");
                CHECK(xrAcquireSwapchainImage(swapchain, &imageAcquireInfo, &index) == XR_ERROR_CALL_ORDER_INVALID);
            }
            {
                INFO("Wait then release all the images in turn");
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
            }

            // XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT requirement of single acquire.
            if (createInfo.createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
                // In this case we can only ever acquire once.
                INFO("In the case of XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT we must only allow a single acquire");
                CHECK(xrAcquireSwapchainImage(swapchain, &imageAcquireInfo, &index) == XR_ERROR_CALL_ORDER_INVALID);
            }
            else {
                // Real apps will acquire images more than once (though they will probably call xrEndFrame too)
                // and this tends to trigger annoying-to-fix Vulkan validation errors in the runtime.
                INFO("Acquire, wait, then release all the images in turn a second time");
                for (uint32_t i = 0; i < imageCount; ++i) {
                    CAPTURE(i);
                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrAcquireSwapchainImage(swapchain, nullptr, &indexVector[i]));
                }
                // Wait/release all the images.
                for (uint32_t i = 0; i < imageCount; ++i) {
                    REQUIRE(GetGlobalData().graphicsPlugin->ValidateSwapchainImageState(swapchain, indexVector[i], imageFormat));
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

    const int64_t kImageFormatInvalid = XRC_INVALID_IMAGE_FORMAT;

    static XrSwapchainCreateInfo MakeDefaultSwapchainCreateInfo(int64_t imageFormat, const SwapchainCreateTestParameters& tp)
    {
        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        CAPTURE(createInfo.faceCount = 1);  // We let cubemap extensions exercise faceCount == 6.
        CAPTURE(createInfo.format = imageFormat);
        CAPTURE(XrSwapchainCreateFlagsRefCPP(createInfo.createFlags) = tp.createFlagsVector[0]);
        CAPTURE(XrSwapchainUsageFlagsRefCPP(createInfo.usageFlags) = tp.usageFlagsVector[0]);
        CAPTURE(createInfo.sampleCount = 1);
        CAPTURE(createInfo.width = 64);
        CAPTURE(createInfo.height = 64);
        CAPTURE(createInfo.arraySize = tp.arrayCountVector[0]);
        CAPTURE(createInfo.mipCount = tp.mipCountVector[0]);
        return createInfo;
    }

    static XrSwapchainCreateInfo FindDefaultColorSwapchainCreateInfo(const std::vector<int64_t>& imageFormatArray, XrInstance instance,
                                                                     XrSystemId systemId, XrSession session)
    {

        // Find a color format to use "by default" when testing a depth format.
        XrSwapchainCreateInfo defaultColorCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        bool foundColorCreateInfo = false;
        for (int64_t imageFormat : imageFormatArray) {
            SwapchainCreateTestParameters tp;
            REQUIRE(GetGlobalData().graphicsPlugin->GetSwapchainCreateTestParameters(instance, session, systemId, imageFormat, &tp));
            if (tp.colorFormat) {
                XrSwapchainCreateInfo swapchainCreateInfo = MakeDefaultSwapchainCreateInfo(imageFormat, tp);
                SwapchainScoped swapchain;
                XrResult result = xrCreateSwapchain(session, &swapchainCreateInfo, swapchain.resetAndGetAddress());
                if (result == XR_SUCCESS) {
                    defaultColorCreateInfo = swapchainCreateInfo;
                    foundColorCreateInfo = true;
                    break;
                }
            }
        }
        if (!foundColorCreateInfo) {
            FAIL("Could not find color swapchain format to use for render tests");
        }
        return defaultColorCreateInfo;
    }

    struct SwapchainTestData
    {
        int swapchainCreateCount{0};
        int unsupportedCount{0};
    };

    static void testSwapchainCreation(XrSession session, SwapchainTestData& data, const XrSwapchainCreateInfo& swapchainCreateInfo,
                                      const SwapchainCreateTestParameters& tp)
    {
        data.swapchainCreateCount++;
        CAPTURE(XrSwapchainCreateFlagsCPP(swapchainCreateInfo.createFlags));
        CAPTURE(swapchainCreateInfo.usageFlags);
        CAPTURE(swapchainCreateInfo.format);
        CAPTURE(swapchainCreateInfo.sampleCount);
        CAPTURE(swapchainCreateInfo.width);
        CAPTURE(swapchainCreateInfo.height);
        CAPTURE(swapchainCreateInfo.faceCount);
        CAPTURE(swapchainCreateInfo.arraySize);
        CAPTURE(swapchainCreateInfo.mipCount);
        XrSwapchain swapchain;
        // A runtime is allowed to fail swapchain creation due to a unsupported creation flag.
        XrResult resultOfXrCreateSwapchain = xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain);
        REQUIRE_THAT(resultOfXrCreateSwapchain, In<XrResult>({XR_SUCCESS, XR_ERROR_FEATURE_UNSUPPORTED}));
        if (resultOfXrCreateSwapchain == XR_ERROR_FEATURE_UNSUPPORTED) {
            WARN("Unsupported config found");
            CAPTURE(resultOfXrCreateSwapchain);
            data.unsupportedCount++;
        }

        if (XR_SUCCEEDED(resultOfXrCreateSwapchain)) {
            TestSwapchainHandle(swapchainCreateInfo.format, &tp, swapchainCreateInfo, swapchain);

            CHECK_RESULT_SUCCEEDED(xrDestroySwapchain(swapchain));

            GetGlobalData().graphicsPlugin->ClearSwapchainCache();
            GetGlobalData().graphicsPlugin->Flush();
        }
    }

    static std::vector<std::pair<std::string, XrSwapchainCreateInfo>> MakeSwapchainCreateInfoCases(const AutoBasicSession& session,
                                                                                                   int64_t imageFormat,
                                                                                                   const SwapchainCreateTestParameters& tp)
    {
        std::vector<std::pair<std::string, XrSwapchainCreateInfo>> ret;
        const auto defaultCreateInfo = MakeDefaultSwapchainCreateInfo(imageFormat, tp);
        {
            auto createInfo = defaultCreateInfo;
            // Smallest compressed texture size is 4x4, use 8x8 to allow for future formats
            createInfo.width = 8;
            createInfo.height = 8;
            ret.emplace_back("Very small texture, but larger than minimum compressed texture size", createInfo);
        }

        for (const auto& viewConfigView : session.viewConfigurationViewVector) {
            {
                auto createInfo = defaultCreateInfo;
                createInfo.width = viewConfigView.recommendedImageRectWidth;
                createInfo.height = viewConfigView.recommendedImageRectHeight;
                ret.emplace_back("Recommended image size for view", createInfo);
            }
            {
                auto createInfo = defaultCreateInfo;
                createInfo.width = viewConfigView.maxImageRectWidth;
                createInfo.height = viewConfigView.maxImageRectHeight;
                ret.emplace_back("Max image size for view", createInfo);
            }

            if (!tp.compressedFormat) {
                auto createInfo = defaultCreateInfo;
                {
                    createInfo.sampleCount = viewConfigView.recommendedSwapchainSampleCount;
                    ret.emplace_back("Recommended sample count", createInfo);
                }
                {
                    createInfo.sampleCount = viewConfigView.maxSwapchainSampleCount;
                    ret.emplace_back("Max sample count", createInfo);
                }
            }
        }

        for (const auto& cf : tp.createFlagsVector) {
            auto createInfo = defaultCreateInfo;
            createInfo.createFlags = cf;
            ret.emplace_back("Non-default create flags", createInfo);
        }

        for (const auto& sc : tp.sampleCountVector) {
            auto createInfo = defaultCreateInfo;
            createInfo.sampleCount = sc;
            ret.emplace_back("Non-default sample count", createInfo);
        }

        for (const auto& uf : tp.usageFlagsVector) {
            auto createInfo = defaultCreateInfo;
            createInfo.usageFlags = (XrSwapchainUsageFlags)uf;
            ret.emplace_back("Non-default usage flags", createInfo);
        }

        for (const auto& ac : tp.arrayCountVector) {
            auto createInfo = defaultCreateInfo;
            createInfo.arraySize = ac;
            ret.emplace_back("Non-default array size", createInfo);
        }

        for (const auto& mc : tp.mipCountVector) {
            auto createInfo = defaultCreateInfo;
            createInfo.mipCount = mc;
            ret.emplace_back("Non-default mip count", createInfo);
        }

        return ret;
    }

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

        // xrEnumerateSwapchainFormats
        {
            uint32_t countOutput = 0;

            // Exercise zero input size.
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr));
            if (countOutput > 0) {
                imageFormatArray.resize(countOutput, kImageFormatInvalid);
            }

            SECTION("Exercise XR_ERROR_SIZE_INSUFFICIENT")
            {
                if (countOutput >= 2) {  // Need at least two in order to exercise XR_ERROR_SIZE_INSUFFICIENT
                    CHECK_MSG(xrEnumerateSwapchainFormats(session, 1, &countOutput, imageFormatArray.data()), XR_ERROR_SIZE_INSUFFICIENT);
                    INFO("Should not overwrite input data");
                    CHECK_MSG(imageFormatArray[1] == kImageFormatInvalid, "Should not overwrite input data.");
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
                REQUIRE_THAT(imageFormatArray, !Catch::Matchers::VectorContains(kImageFormatInvalid));

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
                        CAPTURE(tp.imageFormatName);
                        SwapchainTestData data;

                        for (const auto& nameAndCreateInfo : MakeSwapchainCreateInfoCases(session, imageFormat, tp)) {

                            INFO("XrSwapchainCreateInfo case: " << nameAndCreateInfo.first);
                            auto createInfo = nameAndCreateInfo.second;
                            testSwapchainCreation(session, data, createInfo, tp);
                        }

                        ReportF("    %d cases tested (%d unsupported)", data.swapchainCreateCount, data.unsupportedCount);
                        CAPTURE(data.swapchainCreateCount);
                        CAPTURE(data.unsupportedCount);
                    }
                }
            }
        }
    }

    TEST_CASE("SwapchainsRender", "")
    {
        const GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            // Nothing to check - no graphics plugin means no swapchain
            return;
        }

        // Set up the session we will use for the testing
        AutoBasicSession session(AutoBasicSession::OptionFlags::beginSession);

        // Enumerate formats
        std::vector<int64_t> imageFormatArray;
        {
            uint32_t countOutput = 0;
            REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrEnumerateSwapchainFormats(session, 0, &countOutput, nullptr));
            if (countOutput > 0) {
                imageFormatArray.resize(countOutput, kImageFormatInvalid);
                REQUIRE_RESULT_UNQUALIFIED_SUCCESS(
                    xrEnumerateSwapchainFormats(session, countOutput, &countOutput, imageFormatArray.data()));
            }
        }
        const XrSwapchainCreateInfo defaultColorCreateInfo =
            FindDefaultColorSwapchainCreateInfo(imageFormatArray, session.instance, session.systemId, session);

        for (int64_t imageFormat : imageFormatArray) {

            SwapchainCreateTestParameters tp;
            REQUIRE(
                globalData.graphicsPlugin->GetSwapchainCreateTestParameters(session.instance, session, session.systemId, imageFormat, &tp));

            if (!tp.supportsRendering) {
                // skip this format
                continue;
            }
            ReportF("Testing format %s", tp.imageFormatName.c_str());
            CAPTURE(tp.imageFormatName);

            const std::vector<std::pair<std::string, XrSwapchainCreateInfo>> cases = MakeSwapchainCreateInfoCases(session, imageFormat, tp);
            for (const auto& nameAndCreateInfo : cases) {

                INFO("XrSwapchainCreateInfo case: " << nameAndCreateInfo.first);
                auto createInfo = nameAndCreateInfo.second;
                if ((createInfo.createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) != 0) {
                    // Do not try rendering to static swapchains for now, complicated
                    continue;
                }

                CAPTURE(XrSwapchainCreateFlagsCPP(createInfo.createFlags));
                CAPTURE(XrSwapchainUsageFlagsCPP(createInfo.usageFlags));
                CAPTURE(createInfo.format);
                CAPTURE(createInfo.sampleCount);
                CAPTURE(createInfo.width);
                CAPTURE(createInfo.height);
                CAPTURE(createInfo.faceCount);
                CAPTURE(createInfo.arraySize);
                CAPTURE(createInfo.mipCount);
                SwapchainScoped swapchain;
                XrResult resultOfXrCreateSwapchain = xrCreateSwapchain(session, &createInfo, swapchain.resetAndGetAddress());
                REQUIRE_THAT(resultOfXrCreateSwapchain, In<XrResult>({XR_SUCCESS, XR_ERROR_FEATURE_UNSUPPORTED}));
                if (resultOfXrCreateSwapchain == XR_ERROR_FEATURE_UNSUPPORTED) {
                    continue;
                }

                SwapchainScoped colorSwapchain;
                SwapchainScoped depthSwapchain;

                uint32_t colorImageCount = 0;
                XrSwapchainCreateInfo colorCreateInfo;
                if (tp.colorFormat) {
                    colorSwapchain = std::move(swapchain);
                    colorCreateInfo = createInfo;
                }
                else {
                    depthSwapchain = std::move(swapchain);

                    colorCreateInfo = defaultColorCreateInfo;
                    colorCreateInfo.width = createInfo.width;
                    colorCreateInfo.height = createInfo.height;
                    colorCreateInfo.sampleCount = createInfo.sampleCount;
                    colorCreateInfo.arraySize = createInfo.arraySize;

                    REQUIRE_RESULT_UNQUALIFIED_SUCCESS(xrCreateSwapchain(session, &colorCreateInfo, colorSwapchain.resetAndGetAddress()));
                }

                XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(colorSwapchain.get(), 0, &colorImageCount, nullptr));
                ISwapchainImageData* swapchainImages = nullptr;

                if (depthSwapchain) {
                    uint32_t depthImageCount = 0;
                    XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(depthSwapchain.get(), 0, &depthImageCount, nullptr));

                    {
                        INFO("Artificial requirement of test right now: depth and color swapchains must be the same size");

                        REQUIRE(colorImageCount >= depthImageCount);
                    }
                    swapchainImages = GetGlobalData().graphicsPlugin->AllocateSwapchainImageDataWithDepthSwapchain(
                        colorImageCount, colorCreateInfo, depthSwapchain.get(), createInfo);

                    XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(depthSwapchain.get(), depthImageCount, &depthImageCount,
                                                                     swapchainImages->GetDepthImageArray()));
                }
                else {
                    swapchainImages = GetGlobalData().graphicsPlugin->AllocateSwapchainImageData(colorImageCount, colorCreateInfo);
                }
                XRC_CHECK_THROW_XRCMD(xrEnumerateSwapchainImages(colorSwapchain.get(), colorImageCount, &colorImageCount,
                                                                 swapchainImages->GetColorImageArray()));

                DoRenderTest(swapchainImages, colorImageCount, colorCreateInfo, colorSwapchain.get());
            }
        }
    }
}  // namespace Conformance
