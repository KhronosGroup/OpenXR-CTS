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

#pragma once

#include "platform_plugin.h"
#include "RGBAImage.h"
#include <openxr/openxr.h>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>

// We #include all the possible graphics system headers here because openxr_platform.h assumes that
// they are all visible when it is compiled.

#if defined(XR_USE_GRAPHICS_API_VULKAN)
#ifdef XR_USE_PLATFORM_WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifdef XR_USE_PLATFORM_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>
#endif

#if defined(XR_USE_GRAPHICS_API_OPENGL)
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#endif

#ifdef XR_USE_GRAPHICS_API_D3D11
#include <d3d11_4.h>
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
#include <d3d12.h>
#endif

namespace Conformance
{
    struct Cube
    {
        XrPosef Pose;
        XrVector3f Scale;

        static inline Cube Make(XrVector3f position, float scale = 0.25f, XrQuaternionf orientation = {0, 0, 0, 1})
        {
            return Cube{/* pose */ {orientation, position}, /* scale: */ {scale, scale, scale}};
        }
    };

    // Forward-declare
    struct SwapchainCreateTestParameters;

    // Wraps a graphics API so the main openxr program can be graphics API-independent.
    struct IGraphicsPlugin
    {
        virtual ~IGraphicsPlugin() = default;

        // Required before use of any member functions as described for each function.
        virtual bool Initialize() = 0;

        // Identifies if the IGraphicsPlugin has successfully initialized.
        // May be called regardless of initialization state.
        virtual bool IsInitialized() const = 0;

        // Matches Initialize.
        // May be called only if successfully initialized.
        virtual void Shutdown() = 0;

        // Returns a string describing the platform.
        // May be called regardless of initialization state.
        // Example returned string: "OpenGL"
        virtual std::string DescribeGraphics() const = 0;

        // OpenXR extensions required by this graphics API.
        virtual std::vector<std::string> GetInstanceExtensions() const = 0;

        // Create an instance of this graphics api for the provided XrInstance and XrSystemId.
        // If checkGraphicsRequirements is false then InitializeDevice intentionally doesn't call
        // xrGetxxxxGraphicsRequirementsKHR before initializing a device.
        virtual bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements = true,
                                      uint32_t deviceCreationFlags = 0) = 0;

        // Some graphics devices can accumulate memory usage unless you flush them, and some of our
        // tests create and destroy large amounts of memory.
        virtual void Flush()
        {
            // Default no-op implementation for APIs which don't need flushing.
        }

        // Call to check the validity of the graphics state (useful when checking for interactions with OpenXR calls).
        virtual void CheckState(const char* /*file_line*/) const
        {
            // Default no-op implementation for APIs which don't need checking.
        }

        // Called when changing graphics interaction thread.
        virtual void MakeCurrent(bool /*bindToThread*/)
        {
            // Default no-op implementation for APIs which don't need binding.
        }

        // Shuts down the device initialized by InitializeDevice. Restores to the same state as prior to
        // the call to InitializeDevice.
        virtual void ShutdownDevice() = 0;

        // Get the graphics binding header for session creation.
        // Must have successfully called InitializeDevice before calling this or else this returns nullptr.
        virtual const XrBaseInStructure* GetGraphicsBinding() const = 0;

        virtual void CopyRGBAImage(const XrSwapchainImageBaseHeader* /*swapchainImage*/, int64_t /*imageFormat*/, uint32_t /*arraySlice*/,
                                   const RGBAImage& /*image*/) = 0;

        // Returns a name for an image format. Returns "unknown" for unknown formats.
        virtual std::string GetImageFormatName(int64_t /*imageFormat*/) const = 0;

        // Returns true if the format is known to the plugin. Can be false if the runtime supports extra formats unknown to the conformance tests
        // (e.g. in APIs which have optional extensions).
        virtual bool IsImageFormatKnown(int64_t /*imageFormat*/) const = 0;

        // Retrieves SwapchainCreateTestParameters for the caller, handling plaform-specific functionality
        // internally.
        // Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
        virtual bool GetSwapchainCreateTestParameters(XrInstance /*unused*/, XrSession /*unused*/, XrSystemId /*unused*/,
                                                      int64_t /*unused*/, SwapchainCreateTestParameters* /*unused*/) noexcept(false) = 0;

        // Given an imageFormat and its test parameters and the XrSwapchain resulting from xrCreateSwapchain,
        // validate the images in any platform-specific way.
        // Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
        virtual bool ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* /*tp*/,
                                             XrSwapchain /*swapchain*/, uint32_t* /*imageCount*/) const noexcept(false) = 0;

        // Given an swapchain and an image index, validate the resource state in any platform-specific way.
        // Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
        virtual bool ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
            noexcept(false) = 0;

        // Implementation must select a format with alpha unless there are none with alpha.
        virtual int64_t SelectColorSwapchainFormat(const int64_t* /*imageFormatArray*/, size_t /*count*/) const = 0;

        // Select the preferred swapchain format from the list of available formats.
        virtual int64_t SelectDepthSwapchainFormat(const int64_t* /*imageFormatArray*/, size_t /*count*/) const = 0;

        // Select the preferred swapchain format.
        virtual int64_t GetRGBA8Format(bool sRGB = false) const = 0;

        struct SwapchainImageStructs
        {
            virtual ~SwapchainImageStructs() = default;

            std::vector<XrSwapchainImageBaseHeader*> imagePtrVector;
        };

        // Allocates an array of XrSwapchainImages in a portable way and returns a pointer to a base
        // class which is pointers into that array. This is all for the purpose of being able to call
        // the xrEnumerateSwapchainImages function in a platform-independent way. The user of this
        // must not use the images beyond the lifetime of the shared_ptr.
        //
        // Example usage:
        //   std::shared_ptr<SwapchainImageStructs> p = graphicsPlugin->AllocateSwapchainImageStructs(3, swapchainCreateInfo);
        //   xrEnumerateSwapchainImages(swapchain, 3, nullptr, p->imagePtrVector.data());
        //
        virtual std::shared_ptr<SwapchainImageStructs> AllocateSwapchainImageStructs(size_t size,
                                                                                     const XrSwapchainCreateInfo& swapchainCreateInfo) = 0;

        virtual void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                     int64_t colorSwapchainFormat) = 0;

        virtual void RenderView(const XrCompositionLayerProjectionView& /*layerView*/,
                                const XrSwapchainImageBaseHeader* /*colorSwapchainImage*/, int64_t /*colorSwapchainFormat*/,
                                const std::vector<Cube>& /*cubes*/) = 0;
    };

#define IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD() \
    throw std::runtime_error(std::string(__FUNCTION__) + " is not implemented for the current graphics plugin")

    // Create a graphics plugin for the graphics API specified in the options.
    // Throws std::invalid_argument if the graphics API is empty, unknown, or unsupported.
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin(const char* graphicsAPI,
                                                          std::shared_ptr<IPlatformPlugin> platformPlugin) noexcept(false);

}  // namespace Conformance
