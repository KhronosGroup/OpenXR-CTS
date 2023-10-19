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

#pragma once

#include "platform_plugin.h"
#include "conformance_utils.h"
#include "conformance_framework.h"
#include "utilities/Geometry.h"
#include "utilities/throw_helpers.h"
#include "gltf.h"
#include "RGBAImage.h"
#include "pbr/PbrModel.h"

#include <openxr/openxr.h>
#include <nonstd/span.hpp>
#include <nonstd/type.hpp>
#include <cstdint>
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

#if defined(_WIN32)
#include <windows.h>
#endif  // defined(_WIN32)

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include "common/gfxwrapper_opengl.h"
#endif  // defined(__APPLE__)

#endif  // defined(XR_USE_GRAPHICS_API_OPENGL)

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
    // Import a backported implementation of std::span, or std::span itself if available.

    using nonstd::span;

    class ISwapchainImageData;

    /// Color constant used as the default clear color.
    constexpr XrColor4f DarkSlateGrey = {0.184313729f, 0.309803933f, 0.309803933f, 1.0f};

    /// Parameters for a particular copy of a drawable.
    struct DrawableParams
    {
        XrPosef pose = XrPosefCPP{};
        XrVector3f scale = {1.f, 1.f, 1.f};

        DrawableParams(XrPosef pose_, XrVector3f scale_) : pose(pose_), scale(scale_)
        {
        }
    };

    /// A drawable cube, consisting of pose and scale for a nominally 1m x 1m x 1m cube
    struct Cube
    {
        static inline Cube Make(XrVector3f position, float scale = 0.25f, XrQuaternionf orientation = {0, 0, 0, 1})
        {
            return Cube{/* pose */ {orientation, position}, /* scale: */ {scale, scale, scale}};
        }

        Cube(XrPosef pose, XrVector3f scale) : params(pose, scale)
        {
        }
        DrawableParams params;
    };

    namespace detail
    {
        // Policy to default-init to max so we can tell that a "null" handle is bad.
        // Otherwise, a default-init would be 0 which is often a perfectly fine index.
        using custom_default_max_uint64 = nonstd::custom_default_t<uint64_t, std::numeric_limits<uint64_t>::max()>;
    }  // namespace detail

    /// Handle returned by a graphics plugin, used to reference plugin-internal data for a mesh.
    ///
    /// They expire at IGraphicsPlugin::Shutdown() and IGraphicsPlugin::ShutdownDevice() calls,
    /// so must not be persisted past those calls.
    ///
    /// They are "null" by default, so may be tested for validity by comparison against a default-constructed instance.
    using MeshHandle = nonstd::equality<uint64_t, struct MeshHandleTag, detail::custom_default_max_uint64>;

    /// A drawable mesh, consisting of a reference to plugin-specific data for a mesh, plus pose and scale.
    struct MeshDrawable
    {
        MeshHandle handle;
        DrawableParams params;

        MeshDrawable(MeshHandle handle, XrPosef pose = XrPosefCPP{}, XrVector3f scale = {1.0, 1.0, 1.0})
            : handle(handle), params(pose, scale)
        {
        }
    };

    /// Handle returned by a graphics plugin, used to reference plugin-internal data for a loaded GLTF model.
    ///
    /// They expire at IGraphicsPlugin::Shutdown() and IGraphicsPlugin::ShutdownDevice() calls,
    /// so must not be persisted past those calls.
    ///
    /// They are "null" by default, so may be tested for validity by comparison against a default-constructed instance.
    using GLTFHandle = nonstd::equality<uint64_t, struct GLTFHandleTag, detail::custom_default_max_uint64>;

    using NodeHandle = nonstd::ordered<uint64_t, struct NodeHandleTag, detail::custom_default_max_uint64>;

    struct NodeParams
    {
        XrPosef pose;
        bool visible;
    };

    static inline std::shared_ptr<tinygltf::Model> LoadGLTF(span<const uint8_t> data)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        bool loadedModel = loader.LoadBinaryFromMemory(&model, &err, &warn, data.data(), (unsigned int)data.size());
        if (!warn.empty()) {
            // ReportF("glTF WARN: %s", &warn);
        }

        if (!err.empty()) {
            XRC_THROW("glTF ERR: " + err);
        }

        if (!loadedModel) {
            XRC_THROW("Failed to load glTF model provided.");
        }

        return std::make_shared<tinygltf::Model>(std::move(model));
    }

    /// A drawable GLTF model, consisting of a reference to plugin-specific data for a GLTF model, plus pose and scale.
    struct GLTFDrawable
    {
        GLTFHandle handle;
        DrawableParams params;

        // or unordered_map, probably not significant
        std::map<NodeHandle, NodeParams> nodesAndParams;

        GLTFDrawable(GLTFHandle handle, XrPosef pose = XrPosefCPP{}, XrVector3f scale = {1.0, 1.0, 1.0})
            : handle(handle), params(pose, scale)
        {
        }
    };

    // Forward-declare
    struct SwapchainCreateTestParameters;

    /// Structure using the Builder pattern for IGraphicsPlugin::RenderView parameters, to make it less painful.
    struct RenderParams
    {
        RenderParams& Draw(span<const Cube> cubes_)
        {
            cubes = cubes_;
            return *this;
        }

        RenderParams& Draw(span<const MeshDrawable> meshes_)
        {
            meshes = meshes_;
            return *this;
        }

        RenderParams& Draw(span<const GLTFDrawable> glTFs_)
        {
            glTFs = glTFs_;
            return *this;
        }

        span<const Cube> cubes{};
        span<const MeshDrawable> meshes{};
        span<const GLTFDrawable> glTFs{};
    };

#define IGRAPHICSPLUGIN_UNIMPLEMENTED_METHOD() \
    throw std::runtime_error(std::string(__FUNCTION__) + " is not implemented for the current graphics plugin")

    /// Wraps a graphics API so the main openxr program can be graphics API-independent.
    struct IGraphicsPlugin
    {
        virtual ~IGraphicsPlugin() = default;

        /// Required before use of any member functions as described for each function.
        virtual bool Initialize() = 0;

        /// Identifies if the IGraphicsPlugin has successfully initialized.
        /// May be called regardless of initialization state.
        virtual bool IsInitialized() const = 0;

        /// Matches Initialize.
        /// May be called only if successfully initialized.
        virtual void Shutdown() = 0;

        /// Returns a string describing the platform.
        /// May be called regardless of initialization state.
        /// Example returned string: "OpenGL"
        virtual std::string DescribeGraphics() const = 0;

        /// OpenXR extensions required by this graphics API.
        virtual std::vector<std::string> GetInstanceExtensions() const = 0;

        /// Create an instance of this graphics api for the provided XrInstance and XrSystemId.
        /// If checkGraphicsRequirements is false then InitializeDevice intentionally doesn't call
        /// xrGetxxxxGraphicsRequirementsKHR before initializing a device.
        virtual bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements = true,
                                      uint32_t deviceCreationFlags = 0) = 0;

        /// Clear any memory associated with swapchains, particularly auto-created accompanying depth buffers.
        virtual void ClearSwapchainCache() = 0;

        /// Some graphics devices can accumulate memory usage unless you flush them, and some of our
        /// tests create and destroy large amounts of memory.
        virtual void Flush()
        {
            // Default no-op implementation for APIs which don't need flushing.
        }

        /// Call to check the validity of the graphics state (useful when checking for interactions with OpenXR calls).
        virtual void CheckState(const char* /*file_line*/) const
        {
            // Default no-op implementation for APIs which don't need checking.
        }

        /// Called when changing graphics interaction thread.
        virtual void MakeCurrent(bool /*bindToThread*/)
        {
            // Default no-op implementation for APIs which don't need binding.
        }

        /// Shuts down the device initialized by InitializeDevice. Restores to the same state as prior to
        /// the call to InitializeDevice.
        virtual void ShutdownDevice() = 0;

        /// Get the graphics binding header for session creation.
        /// Must have successfully called InitializeDevice before calling this or else this returns nullptr.
        virtual const XrBaseInStructure* GetGraphicsBinding() const = 0;

        virtual void CopyRGBAImage(const XrSwapchainImageBaseHeader* /*swapchainImage*/, uint32_t /*arraySlice*/,
                                   const RGBAImage& /*image*/) = 0;

        /// Returns a name for an image format. Returns "unknown" for unknown formats.
        virtual std::string GetImageFormatName(int64_t /*imageFormat*/) const = 0;

        /// Returns true if the format is known to the plugin. Can be false if the runtime supports extra formats unknown to the conformance tests
        /// (e.g. in APIs which have optional extensions).
        virtual bool IsImageFormatKnown(int64_t /*imageFormat*/) const = 0;

        /// Retrieves SwapchainCreateTestParameters for the caller, handling platform-specific functionality
        /// internally.
        /// Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
        virtual bool GetSwapchainCreateTestParameters(XrInstance /*unused*/, XrSession /*unused*/, XrSystemId /*unused*/,
                                                      int64_t /*unused*/, SwapchainCreateTestParameters* /*unused*/) noexcept(false) = 0;

        /// Given an imageFormat and its test parameters and the XrSwapchain resulting from xrCreateSwapchain,
        /// validate the images in any platform-specific way.
        /// Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
        virtual bool ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* /*tp*/,
                                             XrSwapchain /*swapchain*/, uint32_t* /*imageCount*/) const noexcept(false) = 0;

        /// Given an swapchain and an image index, validate the resource state in any platform-specific way.
        /// Executes testing CHECK/REQUIRE directives, and may throw a Catch2 failure exception.
        virtual bool ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
            noexcept(false) = 0;

        /// Implementation must select a format with alpha unless there are none with alpha.
        virtual int64_t SelectColorSwapchainFormat(const int64_t* /*imageFormatArray*/, size_t /*count*/) const = 0;

        /// Select the preferred swapchain format from the list of available formats.
        virtual int64_t SelectDepthSwapchainFormat(const int64_t* /*imageFormatArray*/, size_t /*count*/) const = 0;

        /// Select the preferred swapchain format.
        virtual int64_t GetSRGBA8Format() const = 0;

        /// Allocates an object owning (among other things) an array of XrSwapchainImage* in a portable way and
        /// returns an **observing** pointer to an interface providing generic access to the associated pointers.
        /// (The object remains owned by the graphics plugin, and will be destroyed on @ref ShutdownDevice())
        /// This is all for the purpose of being able to call the xrEnumerateSwapchainImages function
        /// in a platform-independent way. The user of this must not use the images beyond @ref ShutdownDevice()
        ///
        /// Example usage:
        ///
        /// ```c++
        /// ISwapchainImageData * p = graphicsPlugin->AllocateSwapchainImageData(3, swapchainCreateInfo);
        /// xrEnumerateSwapchainImages(swapchain, 3, &count, p->GetColorImageArray());
        /// ```
        virtual ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) = 0;

        /// Allocates an object owning (among other things) an array of XrSwapchainImage* in a portable way and
        /// returns an **observing** pointer to an interface providing generic access to the associated pointers.
        ///
        /// Signals that we will use a depth swapchain allocated by the runtime, instead of a fallback depth
        /// allocated by the plugin.
        virtual ISwapchainImageData*
        AllocateSwapchainImageDataWithDepthSwapchain(size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo,
                                                     XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthSwapchainCreateInfo) = 0;

        /// Clears a slice to an arbitrary color
        virtual void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex, XrColor4f color) = 0;

        /// Clears to the background color which varies depending on the environment blend mode that is active.
        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex = 0)
        {
            GlobalData& globalData = GetGlobalData();
            ClearImageSlice(colorSwapchainImage, imageArrayIndex, globalData.GetClearColorForBackground());
        }

        /// Create internal data for a mesh, returning a handle to refer to it.
        /// This handle expires when the internal data is cleared in Shutdown() and ShutdownDevice().
        virtual MeshHandle MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx) = 0;

        /// Create internal data for a glTF model, returning a handle to refer to it.
        /// This handle expires when the internal data is cleared in Shutdown() and ShutdownDevice().
        virtual GLTFHandle LoadGLTF(span<const uint8_t> data) = 0;

        virtual std::shared_ptr<Pbr::Model> GetModel(GLTFHandle handle) const = 0;

        /// Convenience helper function to make a mesh that is our standard cube (with R, G, B faces along X, Y, Z, respectively)
        MeshHandle MakeCubeMesh()
        {
            return MakeSimpleMesh(Geometry::c_cubeIndices, Geometry::c_cubeVertices);
        }

        /// Convenience helper function to make a mesh that is "coordinate axes" also called a "gnomon"
        MeshHandle MakeGnomonMesh()
        {
            return MakeSimpleMesh(Geometry::AxisIndicator::GetInstance().indices, Geometry::AxisIndicator::GetInstance().vertices);
        };

        virtual void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                                const RenderParams& params) = 0;
    };

    /// Create a graphics plugin for the graphics API specified in the options.
    /// Throws std::invalid_argument if the graphics API is empty, unknown, or unsupported.
    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin(const char* graphicsAPI,
                                                          std::shared_ptr<IPlatformPlugin> platformPlugin) noexcept(false);

}  // namespace Conformance
