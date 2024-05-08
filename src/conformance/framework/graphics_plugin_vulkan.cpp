// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include "RGBAImage.h"
#include "conformance_utils.h"
#include "gltf_helpers.h"
#include "graphics_plugin.h"
#include "graphics_plugin_impl_helpers.h"
#include "graphics_plugin_vulkan_gltf.h"
#include "report.h"
#include "swapchain_image_data.h"

#include "common/hex_and_handles.h"
#include "common/vulkan_debug_object_namer.hpp"
#include "common/xr_dependencies.h"
#include "common/xr_linear.h"
#include "pbr/PbrCommon.h"
#include "pbr/Vulkan/VkCommon.h"
#include "pbr/Vulkan/VkResources.h"
#include "pbr/Vulkan/VkTexture.h"
#include "pbr/Vulkan/VkModel.h"
#include "utilities/Geometry.h"
#include "utilities/swapchain_format_data.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"
#include "utilities/vulkan_utils.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nonstd/span.hpp>
#include <nonstd/type.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <assert.h>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <stddef.h>
#include <stdexcept>
#include <string.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifdef USE_CHECKPOINTS
#include <unordered_set>
#endif

namespace Pbr
{
    class Model;
}  // namespace Pbr

namespace Conformance
{
    struct IPlatformPlugin;

#ifdef USE_ONLINE_VULKAN_SHADERC
    constexpr char VertexShaderGlsl[] =
        R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (std140, push_constant) uniform buf
    {
        mat4 mvp;
        vec4 tintColor;
    } ubuf;

    layout (location = 0) in vec3 Position;
    layout (location = 1) in vec3 Color;

    layout (location = 0) out vec4 oColor;
    out gl_PerVertex
    {
        vec4 gl_Position;
    };

    void main()
    {
        oColor.rgb = mix(Color.rgb, ubuf.tintColor.rgb, ubuf.tintColor.a);
        oColor.a  = 1.0;
        gl_Position = ubuf.mvp * vec4(Position, 1);
    }
)_";

    constexpr char FragmentShaderGlsl[] =
        R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (location = 0) in vec4 oColor;

    layout (location = 0) out vec4 FragColor;

    void main()
    {
        FragColor = oColor;
    }
)_";
#endif  // USE_ONLINE_VULKAN_SHADERC

    struct VulkanArraySliceState
    {
        VulkanArraySliceState() = default;
        VulkanArraySliceState(const VulkanArraySliceState&) = delete;
        std::vector<RenderTarget> m_renderTarget;  // per swapchain index
        RenderPass m_rp{};
        Pipeline m_pipe{};

        void init(const VulkanDebugObjectNamer& namer, VkDevice device, uint32_t capacity, const VkExtent2D size, VkFormat colorFormat,
                  VkFormat depthFormat, VkSampleCountFlagBits sampleCount, const PipelineLayout& layout, const ShaderProgram& sp,
                  const VkVertexInputBindingDescription& bindDesc, span<const VkVertexInputAttributeDescription> attrDesc)
        {
            m_renderTarget.resize(capacity);
            m_rp.Create(namer, device, colorFormat, depthFormat, sampleCount);
            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
            m_pipe.Create(device, size, layout, m_rp, sp, bindDesc, attrDesc, dynamicStates);
        }

        void Reset()
        {
            m_pipe.Reset();
            m_rp.Reset();
            m_renderTarget.clear();
        }
    };

    /// Vulkan data used per swapchain. One per XrSwapchain handle.
    class VulkanSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageVulkanKHR>
    {
        void init(uint32_t capacity, VkFormat colorFormat, const PipelineLayout& layout, const ShaderProgram& sp,
                  const VkVertexInputBindingDescription& bindDesc, span<const VkVertexInputAttributeDescription> attrDesc)
        {
            m_depthBuffer.resize(capacity);
            for (auto& slice : m_slices) {
                slice.init(m_namer, m_vkDevice, capacity, m_size, colorFormat, m_depthFormat, m_sampleCount, layout, sp, bindDesc,
                           attrDesc);
            }
        }

    public:
        VulkanSwapchainImageData(const VulkanDebugObjectNamer& namer, uint32_t capacity, const XrSwapchainCreateInfo& swapchainCreateInfo,
                                 VkDevice device, MemoryAllocator* memAllocator, const PipelineLayout& layout, const ShaderProgram& sp,
                                 const VkVertexInputBindingDescription& bindDesc, span<const VkVertexInputAttributeDescription> attrDesc)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, capacity, swapchainCreateInfo)
            , m_namer(namer)
            , m_vkDevice(device)
            , m_memAllocator(memAllocator)
            , m_size{swapchainCreateInfo.width, swapchainCreateInfo.height}
            , m_sampleCount{(VkSampleCountFlagBits)swapchainCreateInfo.sampleCount}
            , m_slices(swapchainCreateInfo.arraySize)
        {
            init(capacity, (VkFormat)swapchainCreateInfo.format, layout, sp, bindDesc, attrDesc);
        }

        VulkanSwapchainImageData(const VulkanDebugObjectNamer& namer, uint32_t capacity, const XrSwapchainCreateInfo& swapchainCreateInfo,
                                 XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthSwapchainCreateInfo, VkDevice device,
                                 MemoryAllocator* memAllocator, const PipelineLayout& layout, const ShaderProgram& sp,
                                 const VkVertexInputBindingDescription& bindDesc, span<const VkVertexInputAttributeDescription> attrDesc)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, capacity, swapchainCreateInfo, depthSwapchain,
                                     depthSwapchainCreateInfo)
            , m_namer(namer)
            , m_vkDevice(device)
            , m_memAllocator(memAllocator)
            , m_size{swapchainCreateInfo.width, swapchainCreateInfo.height}
            , m_sampleCount{(VkSampleCountFlagBits)swapchainCreateInfo.sampleCount}
            , m_depthFormat((VkFormat)depthSwapchainCreateInfo.format)
            , m_slices(swapchainCreateInfo.arraySize)
        {
            init(capacity, (VkFormat)swapchainCreateInfo.format, layout, sp, bindDesc, attrDesc);
        }

        ~VulkanSwapchainImageData() override
        {
            // Calling a virtual function from a destructor doesn't work the way you'd expect, so we do this here.
            VulkanSwapchainImageData::Reset();
        }

        void BindRenderTarget(uint32_t index, uint32_t arraySlice, const VkRect2D& renderArea, VkImageAspectFlags secondAttachmentAspect,
                              VkRenderPassBeginInfo* renderPassBeginInfo)
        {
            RenderTarget& rt = m_slices[arraySlice].m_renderTarget[index];
            RenderPass& rp = m_slices[arraySlice].m_rp;
            if (rt.fb == VK_NULL_HANDLE) {
                rt.Create(m_namer, m_vkDevice, GetTypedImage(index).image, GetDepthImageForColorIndex(index).image, secondAttachmentAspect,
                          arraySlice, m_size, rp);
            }
            renderPassBeginInfo->renderPass = rp.pass;
            renderPassBeginInfo->framebuffer = rt.fb;
            renderPassBeginInfo->renderArea = renderArea;
        }

        void BindPipeline(VkCommandBuffer buf, uint32_t arraySlice)
        {
            vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_slices[arraySlice].m_pipe.pipe);
        }

        void TransitionLayout(uint32_t imageIndex, CmdBuffer* cmdBuffer, VkImageLayout newLayout)
        {
            m_depthBuffer[imageIndex].TransitionLayout(cmdBuffer, newLayout);
        }

        void Reset() override
        {
            for (auto& slice : m_slices) {
                slice.Reset();
            }
            m_depthBuffer.clear();
            SwapchainImageDataBase::Reset();
        }

        int64_t GetDepthFormat() const
        {
            return m_depthFormat;
        }

    protected:
        const XrSwapchainImageVulkanKHR& GetFallbackDepthSwapchainImage(uint32_t i) override
        {
            if (!m_depthBuffer[i].Allocated()) {
                m_depthBuffer[i].Allocate(m_namer, m_vkDevice, m_memAllocator, m_depthFormat, this->Width(), this->Height(),
                                          this->ArraySize(), this->SampleCount());
            }

            return m_depthBuffer[i].GetTexture();
        }

    private:
        VulkanDebugObjectNamer m_namer;
        VkDevice m_vkDevice{VK_NULL_HANDLE};
        MemoryAllocator* m_memAllocator{nullptr};
        VkExtent2D m_size{};
        VkSampleCountFlagBits m_sampleCount;
        std::vector<DepthBuffer> m_depthBuffer;  // per swapchain index
        VkFormat m_depthFormat{VK_FORMAT_D32_SFLOAT};

        std::vector<VulkanArraySliceState> m_slices;
    };

#if defined(USE_MIRROR_WINDOW)
    // Swapchain
    struct Swapchain
    {
        VkFormat format{VK_FORMAT_B8G8R8A8_SRGB};
        VkSurfaceKHR surface{VK_NULL_HANDLE};
        VkSwapchainKHR swapchain{VK_NULL_HANDLE};
        VkFence readyFence{VK_NULL_HANDLE};
        VkFence presentFence{VK_NULL_HANDLE};
        static const uint32_t maxImages = 4;
        uint32_t swapchainCount = 0;
        uint32_t renderImageIdx = 0;
        VkImage image[maxImages]{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        Swapchain()
        {
        }
        ~Swapchain()
        {
            Reset();
        }

        void Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device, uint32_t queueFamilyIndex);
        void Prepare(VkCommandBuffer buf);
        void Wait();
        void Acquire(VkSemaphore readySemaphore = VK_NULL_HANDLE);
        void Present(VkQueue queue, VkSemaphore drawComplete = VK_NULL_HANDLE);
        void Reset()
        {
            if (m_vkDevice) {
                // Flush any pending Present() calls which are using the fence
                Wait();
                if (swapchain)
                    vkDestroySwapchainKHR(m_vkDevice, swapchain, nullptr);
                if (readyFence)
                    vkDestroyFence(m_vkDevice, readyFence, nullptr);
            }

            if (m_vkInstance && surface)
                vkDestroySurfaceKHR(m_vkInstance, surface, nullptr);

            readyFence = VK_NULL_HANDLE;
            presentFence = VK_NULL_HANDLE;
            swapchain = VK_NULL_HANDLE;
            surface = VK_NULL_HANDLE;
            for (uint32_t i = 0; i < swapchainCount; ++i) {
                image[i] = VK_NULL_HANDLE;
            }
            swapchainCount = 0;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
            if (hWnd) {
                DestroyWindow(hWnd);
                hWnd = nullptr;
                UnregisterClassW(L"conformance_test", hInst);
            }
            if (hUser32Dll) {
                ::FreeLibrary(hUser32Dll);
                hUser32Dll = nullptr;
            }
#endif

            m_vkDevice = nullptr;
        }
        void Recreate()
        {
            Reset();
            Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice, m_queueFamilyIndex);
        }

    private:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        HINSTANCE hInst{NULL};
        HWND hWnd{NULL};
        HINSTANCE hUser32Dll{NULL};
#endif
        const VkExtent2D size{640, 480};
        VkInstance m_vkInstance{VK_NULL_HANDLE};
        VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
        VkDevice m_vkDevice{VK_NULL_HANDLE};
        uint32_t m_queueFamilyIndex = 0;
    };

    void Swapchain::Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device, uint32_t queueFamilyIndex)
    {
        m_vkInstance = instance;
        m_vkPhysicalDevice = physDevice;
        m_vkDevice = device;
        m_queueFamilyIndex = queueFamilyIndex;

// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        hInst = GetModuleHandle(NULL);

        WNDCLASSW wc{};
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.cbWndExtra = sizeof(this);
        wc.hInstance = hInst;
        wc.lpszClassName = L"conformance_test";
        RegisterClassW(&wc);

// adjust the window size and show at InitDevice time
#if defined(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
        typedef DPI_AWARENESS_CONTEXT(WINAPI * PFN_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
        hUser32Dll = ::LoadLibraryA("user32.dll");
        if (PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn =
                reinterpret_cast<PFN_SetThreadDpiAwarenessContext>(::GetProcAddress(hUser32Dll, "SetThreadDpiAwarenessContext"))) {
            // Make sure we're 1:1 for HMD pixels
            SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
#endif
        RECT rect{0, 0, (LONG)size.width, (LONG)size.height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
        hWnd = CreateWindowW(wc.lpszClassName, L"conformance_test (Vulkan)", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left, rect.bottom - rect.top, 0, 0, hInst, 0);
        assert(hWnd != NULL);

        SetWindowLongPtr(hWnd, 0, LONG_PTR(this));

        VkWin32SurfaceCreateInfoKHR surfCreateInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        surfCreateInfo.flags = 0;
        surfCreateInfo.hinstance = hInst;
        surfCreateInfo.hwnd = hWnd;
        XRC_CHECK_THROW_VKCMD(vkCreateWin32SurfaceKHR(m_vkInstance, &surfCreateInfo, nullptr, &surface));
#else
#error CreateSurface not supported on this OS
#endif  // defined(VK_USE_PLATFORM_WIN32_KHR)

        VkSurfaceCapabilitiesKHR surfCaps;
        XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkPhysicalDevice, surface, &surfCaps));
        XRC_CHECK_THROW(surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        uint32_t surfFmtCount = 0;
        XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysicalDevice, surface, &surfFmtCount, nullptr));
        std::vector<VkSurfaceFormatKHR> surfFmts(surfFmtCount);
        XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysicalDevice, surface, &surfFmtCount, &surfFmts[0]));
        uint32_t foundFmt;
        for (foundFmt = 0; foundFmt < surfFmtCount; ++foundFmt) {
            if (surfFmts[foundFmt].format == format)
                break;
        }

        XRC_CHECK_THROW(foundFmt < surfFmtCount);

        uint32_t presentModeCount = 0;
        XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysicalDevice, surface, &presentModeCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysicalDevice, surface, &presentModeCount, &presentModes[0]));

        // Do not use VSYNC for the mirror window, but Nvidia doesn't support IMMEDIATE so fall back to MAILBOX
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        for (uint32_t i = 0; i < presentModeCount; ++i) {
            if ((presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) || (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)) {
                presentMode = presentModes[i];
                break;
            }
        }

        VkBool32 presentable = false;
        XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceSupportKHR(m_vkPhysicalDevice, m_queueFamilyIndex, surface, &presentable));
        XRC_CHECK_THROW(presentable);

        VkSwapchainCreateInfoKHR swapchainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchainInfo.flags = 0;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = surfCaps.minImageCount;
        swapchainInfo.imageFormat = format;
        swapchainInfo.imageColorSpace = surfFmts[foundFmt].colorSpace;
        swapchainInfo.imageExtent = size;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices = nullptr;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = true;
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
        XRC_CHECK_THROW_VKCMD(vkCreateSwapchainKHR(m_vkDevice, &swapchainInfo, nullptr, &swapchain));

        // Fence to throttle host on Acquire
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        XRC_CHECK_THROW_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &readyFence));

        swapchainCount = 0;
        XRC_CHECK_THROW_VKCMD(vkGetSwapchainImagesKHR(m_vkDevice, swapchain, &swapchainCount, nullptr));
        assert(swapchainCount < maxImages);
        XRC_CHECK_THROW_VKCMD(vkGetSwapchainImagesKHR(m_vkDevice, swapchain, &swapchainCount, image));
        if (swapchainCount > maxImages) {
            //Log::Write(Log::Level::Info, "Reducing swapchain length from " + std::to_string(swapchainCount) + " to " + std::to_string(maxImages));
            swapchainCount = maxImages;
        }

        //Log::Write(Log::Level::Info, "Swapchain length " + std::to_string(swapchainCount));
    }

    void Swapchain::Prepare(VkCommandBuffer buf)
    {
        // Convert swapchain images to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        for (uint32_t i = 0; i < swapchainCount; ++i) {
            VkImageMemoryBarrier imgBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            imgBarrier.srcAccessMask = 0;  // XXX was VK_ACCESS_TRANSFER_READ_BIT wrong?
            imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imgBarrier.image = image[i];
            imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &imgBarrier);
        }
    }

    void Swapchain::Wait()
    {
        if (presentFence) {
            // Wait for the fence...
            XRC_CHECK_THROW_VKCMD(vkWaitForFences(m_vkDevice, 1, &presentFence, VK_TRUE, UINT64_MAX));
            // ...then reset the fence for future Acquire calls
            XRC_CHECK_THROW_VKCMD(vkResetFences(m_vkDevice, 1, &presentFence));
            presentFence = VK_NULL_HANDLE;
        }
    }

    void Swapchain::Acquire(VkSemaphore readySemaphore)
    {
        // If we're not using a semaphore to rate-limit the GPU, rate limit the host with a fence instead
        if (readySemaphore == VK_NULL_HANDLE) {
            Wait();
            presentFence = readyFence;
        }

        XRC_CHECK_THROW_VKCMD(vkAcquireNextImageKHR(m_vkDevice, swapchain, UINT64_MAX, readySemaphore, presentFence, &renderImageIdx));
    }

    void Swapchain::Present(VkQueue queue, VkSemaphore drawComplete)
    {
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        if (drawComplete) {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &drawComplete;
        }
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &renderImageIdx;
        auto res = vkQueuePresentKHR(queue, &presentInfo);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            Recreate();
            return;
        }
        XRC_CHECK_THROW_VKRESULT(res, "vkQueuePresentKHR");
    }
#endif  // defined(USE_MIRROR_WINDOW)

    struct VulkanMesh
    {
        static constexpr VkVertexInputAttributeDescription c_attrDesc[2] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Geometry::Vertex, Position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Geometry::Vertex, Color)}};
        static constexpr VkVertexInputBindingDescription c_bindingDesc = VertexBuffer<Geometry::Vertex>::c_bindingDesc;

        VertexBuffer<Geometry::Vertex> m_DrawBuffer;

        VulkanMesh(VkDevice device, const MemoryAllocator* memAllocator,  //
                   const uint16_t* idx_data, uint32_t idx_count,          //
                   const Geometry::Vertex* vtx_data, uint32_t vtx_count)
        {
            std::vector<VkVertexInputAttributeDescription> attrDesc(std::begin(c_attrDesc), std::end(c_attrDesc));
            m_DrawBuffer.Init(device, memAllocator, attrDesc);
            m_DrawBuffer.Create(idx_count, vtx_count);
            m_DrawBuffer.UpdateIndices(nonstd::span<const uint16_t>(idx_data, idx_count), 0);
            m_DrawBuffer.UpdateVertices(nonstd::span<const Geometry::Vertex>(vtx_data, vtx_count), 0);
        }

        VulkanMesh(VulkanMesh&& other) noexcept
        {
            using std::swap;
            swap(m_DrawBuffer, other.m_DrawBuffer);
        }

        VulkanMesh(const VulkanMesh&) = delete;
    };
    constexpr VkVertexInputAttributeDescription VulkanMesh::c_attrDesc[];
    constexpr VkVertexInputBindingDescription VulkanMesh::c_bindingDesc;

    struct VulkanGraphicsPlugin : public IGraphicsPlugin
    {
        VulkanGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/);

        ~VulkanGraphicsPlugin() override;

        void Flush() override
        {
            if (m_vkDevice != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(m_vkDevice);
            }
        }

        std::vector<std::string> GetInstanceExtensions() const override
        {
            return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
        }
        virtual XrStructureType GetGraphicsBindingType() const
        {
            return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
        }
        virtual XrStructureType GetSwapchainImageType() const
        {
            return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
        }
        virtual XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId,
                                                           XrGraphicsRequirementsVulkan2KHR* graphicsRequirements);
        virtual XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo,
                                                 VkInstance* vulkanInstance, VkResult* vulkanResult);
        virtual XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
                                                     VkPhysicalDevice* vulkanPhysicalDevice);
        virtual XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo, VkDevice* vulkanDevice,
                                               VkResult* vulkanResult);

        // Note: The output must not outlive the input - this modifies the input and returns a collection of views into that modified
        // input!
        std::vector<const char*> ParseExtensionString(char* names)
        {
            std::vector<const char*> list;
            while (*names != 0) {
                list.push_back(names);
                while (*(++names) != 0) {
                    if (*names == ' ') {
                        *names++ = '\0';
                        break;
                    }
                }
            }
            return list;
        }

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

#ifdef USE_ONLINE_VULKAN_SHADERC
        std::vector<uint32_t> CompileGlslShader(const std::string& name, shaderc_shader_kind kind, const std::string& source);
#endif

        void InitializeResources();

        void ClearSwapchainCache() override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(XrInstance instance, XrSession session, XrSystemId systemId, int64_t imageFormat,
                                              SwapchainCreateTestParameters* swapchainTestParameters) override;

        bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                     uint32_t* imageCount) const override;
        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectMotionVectorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        // Format required by RGBAImage type.
        int64_t GetSRGBA8Format() const override;

        ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo) override;

        ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(size_t size,
                                                                          const XrSwapchainCreateInfo& colorSwapchainCreateInfo,
                                                                          XrSwapchain depthSwapchain,
                                                                          const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override;

        // ISwapchainImageData * EnumerateSwapchainImageData(XrSwapchain colorSwapchain,
        //                                                                  const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImageBase, uint32_t arraySlice, const RGBAImage& image) override;

        void SetViewportAndScissor(const VkRect2D& rect);

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex, XrColor4f color) override;

        MeshHandle MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx) override;

        GLTFModelHandle LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel) override;
        std::shared_ptr<Pbr::Model> GetPbrModel(GLTFModelHandle handle) const override;
        GLTFModelInstanceHandle CreateGLTFModelInstance(GLTFModelHandle handle) override;
        Pbr::ModelInstance& GetModelInstance(GLTFModelInstanceHandle handle) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        const RenderParams& params) override;

        /// Get data on a known swapchain format
        const SwapchainFormatData& FindFormatData(int64_t format) const;

#if defined(USE_CHECKPOINTS)
        void Checkpoint(std::string msg)
        {
            auto check = checkpoints.emplace(std::move(msg));
            vkCmdSetCheckpointNV(m_cmdBuffer.buf, check.first->c_str());
        }

        void ShowCheckpoints()
        {
            if (m_vkQueue != VK_NULL_HANDLE) {
                uint32_t count = 0;
                vkGetQueueCheckpointDataNV(m_vkQueue, &count, nullptr);
                ReportF("ShowCheckpoints found %u checkpoints", count);
                if (count > 0) {
                    std::vector<VkCheckpointDataNV> data(count);
                    vkGetQueueCheckpointDataNV(m_vkQueue, &count, &data[0]);
                    for (uint32_t i = 0; i < count; ++i) {
                        auto& c = data[i];
                        std::string stages = GetPipelineStages(c.stage);
                        ReportF("%3d: %s -%s", i, (const char*)c.pCheckpointMarker, stages.c_str());
                    }
                }
            }
        }
#endif  // defined(USE_CHECKPOINTS)

    protected:
        bool initialized{false};
        VkInstance m_vkInstance{VK_NULL_HANDLE};
        VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};

    private:
        XrGraphicsBindingVulkan2KHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        SwapchainImageDataMap<VulkanSwapchainImageData> m_swapchainImageDataMap;

        VkDevice m_vkDevice{VK_NULL_HANDLE};
        VulkanDebugObjectNamer m_namer{};
        uint32_t m_queueFamilyIndex = 0;
        VkQueue m_vkQueue{VK_NULL_HANDLE};
        VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

        MemoryAllocator m_memAllocator{};
        ShaderProgram m_shaderProgram{};
        CmdBuffer m_cmdBuffer{};
        PipelineLayout m_pipelineLayout{};
        MeshHandle m_cubeMesh{};
        VectorWithGenerationCountedHandles<VulkanMesh, MeshHandle> m_meshes;
        // This is fine to be a shared_ptr because Model doesn't directly hold any graphics state.
        VectorWithGenerationCountedHandles<std::shared_ptr<Pbr::Model>, GLTFModelHandle> m_gltfModels;
        VectorWithGenerationCountedHandles<VulkanGLTF, GLTFModelInstanceHandle> m_gltfInstances;
        std::unique_ptr<Pbr::VulkanResources> m_pbrResources;

#if defined(USE_MIRROR_WINDOW)
        Swapchain m_swapchain{};
#endif

#if defined(USE_CHECKPOINTS)
        PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV{};
        PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV{};
        std::unordered_set<std::string> checkpoints{};
#endif

        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT{nullptr};
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT{nullptr};
        VkDebugUtilsMessengerEXT m_vkDebugUtilsMessenger{VK_NULL_HANDLE};

        static std::string vkObjectTypeToString(VkObjectType objectType)
        {
            std::string objName;

#define LIST_OBJECT_TYPES(_)          \
    _(UNKNOWN)                        \
    _(INSTANCE)                       \
    _(PHYSICAL_DEVICE)                \
    _(DEVICE)                         \
    _(QUEUE)                          \
    _(SEMAPHORE)                      \
    _(COMMAND_BUFFER)                 \
    _(FENCE)                          \
    _(DEVICE_MEMORY)                  \
    _(BUFFER)                         \
    _(IMAGE)                          \
    _(EVENT)                          \
    _(QUERY_POOL)                     \
    _(BUFFER_VIEW)                    \
    _(IMAGE_VIEW)                     \
    _(SHADER_MODULE)                  \
    _(PIPELINE_CACHE)                 \
    _(PIPELINE_LAYOUT)                \
    _(RENDER_PASS)                    \
    _(PIPELINE)                       \
    _(DESCRIPTOR_SET_LAYOUT)          \
    _(SAMPLER)                        \
    _(DESCRIPTOR_POOL)                \
    _(DESCRIPTOR_SET)                 \
    _(FRAMEBUFFER)                    \
    _(COMMAND_POOL)                   \
    _(SURFACE_KHR)                    \
    _(SWAPCHAIN_KHR)                  \
    _(DISPLAY_KHR)                    \
    _(DISPLAY_MODE_KHR)               \
    _(DESCRIPTOR_UPDATE_TEMPLATE_KHR) \
    _(DEBUG_UTILS_MESSENGER_EXT)

            switch (objectType) {
            default:
#define MK_OBJECT_TYPE_CASE(name) \
    case VK_OBJECT_TYPE_##name:   \
        objName = #name;          \
        break;
                LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)
            }

            return objName;
        }
        VkBool32 debugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData)
        {

            std::string flagNames;
            std::string objName;

            if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0u) {
                flagNames += "DEBUG:";
            }
            if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u) {
                flagNames += "INFO:";
            }
            if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0u) {
                flagNames += "WARN:";
            }
            if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
                flagNames += "ERROR:";
            }
            if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
                flagNames += "PERF:";
            }

            uint64_t object = 0;
            // skip loader messages about device extensions
            if (pCallbackData->objectCount > 0) {
                auto objectType = pCallbackData->pObjects[0].objectType;
                if ((objectType == VK_OBJECT_TYPE_INSTANCE) && (strncmp(pCallbackData->pMessage, "Device Extension:", 17) == 0)) {
                    return VK_FALSE;
                }
                objName = vkObjectTypeToString(objectType);
                object = pCallbackData->pObjects[0].objectHandle;
                if (pCallbackData->pObjects[0].pObjectName != nullptr) {
                    objName += " " + std::string(pCallbackData->pObjects[0].pObjectName);
                }
            }

            ReportF("%s (%s 0x%llx) %s", flagNames.c_str(), objName.c_str(), object, pCallbackData->pMessage);

            return VK_FALSE;
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageThunk(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                                VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
        {
            return static_cast<VulkanGraphicsPlugin*>(pUserData)->debugMessage(messageSeverity, messageTypes, pCallbackData);
        }
    };

    struct VulkanGraphicsPluginLegacy : public VulkanGraphicsPlugin
    {
        VulkanGraphicsPluginLegacy(const std::shared_ptr<IPlatformPlugin>& platformPlugin);

        std::vector<std::string> GetInstanceExtensions() const override
        {
            return {XR_KHR_VULKAN_ENABLE_EXTENSION_NAME};
        }
        XrStructureType GetGraphicsBindingType() const override
        {
            return XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
        }
        XrStructureType GetSwapchainImageType() const override
        {
            return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        }
        XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId,
                                                   XrGraphicsRequirementsVulkan2KHR* graphicsRequirements) override;
        XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo, VkInstance* vulkanInstance,
                                         VkResult* vulkanResult) override;
        XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
                                             VkPhysicalDevice* vulkanPhysicalDevice) override;
        XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo, VkDevice* vulkanDevice,
                                       VkResult* vulkanResult) override;

        // do not override ShutdownDevice or Shutdown here!
    };

    VulkanGraphicsPlugin::VulkanGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/)
    {
    }

    VulkanGraphicsPlugin::~VulkanGraphicsPlugin()
    {
        ShutdownDevice();
        Shutdown();
    }

    VulkanGraphicsPluginLegacy::VulkanGraphicsPluginLegacy(const std::shared_ptr<IPlatformPlugin>& platformPlugin)
        : VulkanGraphicsPlugin(platformPlugin)
    {
    }

    XrResult VulkanGraphicsPlugin::GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId,
                                                                     XrGraphicsRequirementsVulkan2KHR* graphicsRequirements)
    {
        PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirements2KHR)));

        return pfnGetVulkanGraphicsRequirements2KHR(instance, systemId, graphicsRequirements);
    }

    XrResult VulkanGraphicsPluginLegacy::GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId,
                                                                           XrGraphicsRequirementsVulkan2KHR* graphicsRequirements)
    {
        PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirementsKHR)));

        XrGraphicsRequirementsVulkanKHR legacyRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, &legacyRequirements));

        graphicsRequirements->maxApiVersionSupported = legacyRequirements.maxApiVersionSupported;
        graphicsRequirements->minApiVersionSupported = legacyRequirements.minApiVersionSupported;

        return XR_SUCCESS;
    }

    XrResult VulkanGraphicsPlugin::CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo,
                                                           VkInstance* vulkanInstance, VkResult* vulkanResult)
    {
        PFN_xrCreateVulkanInstanceKHR pfnCreateVulkanInstanceKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanInstanceKHR)));

        return pfnCreateVulkanInstanceKHR(instance, createInfo, vulkanInstance, vulkanResult);
    }

    XrResult VulkanGraphicsPluginLegacy::CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo,
                                                                 VkInstance* vulkanInstance, VkResult* vulkanResult)
    {
        PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanInstanceExtensionsKHR)));

        uint32_t extensionNamesSize = 0;
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, 0, &extensionNamesSize, nullptr));

        std::vector<char> extensionNames(extensionNamesSize);
        XRC_CHECK_THROW_XRCMD(
            pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, extensionNamesSize, &extensionNamesSize, &extensionNames[0]));
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions = ParseExtensionString(&extensionNames[0]);

            // Merge the runtime's request with the applications requests
            for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
                extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
            }

            VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            memcpy(&instInfo, createInfo->vulkanCreateInfo, sizeof(instInfo));
            instInfo.enabledExtensionCount = (uint32_t)extensions.size();
            instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

            auto pfnCreateInstance = (PFN_vkCreateInstance)createInfo->pfnGetInstanceProcAddr(nullptr, "vkCreateInstance");
            *vulkanResult = pfnCreateInstance(&instInfo, createInfo->vulkanAllocator, vulkanInstance);
        }

        return XR_SUCCESS;
    }

    XrResult VulkanGraphicsPlugin::GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
                                                               VkPhysicalDevice* vulkanPhysicalDevice)
    {
        PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDevice2KHR)));

        return pfnGetVulkanGraphicsDevice2KHR(instance, getInfo, vulkanPhysicalDevice);
    }

    XrResult VulkanGraphicsPluginLegacy::GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
                                                                     VkPhysicalDevice* vulkanPhysicalDevice)
    {
        PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDeviceKHR)));

        if (getInfo->next != nullptr) {
            return XR_ERROR_FEATURE_UNSUPPORTED;
        }

        XRC_CHECK_THROW_XRCMD(pfnGetVulkanGraphicsDeviceKHR(instance, getInfo->systemId, getInfo->vulkanInstance, vulkanPhysicalDevice));

        return XR_SUCCESS;
    }

    XrResult VulkanGraphicsPlugin::CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo,
                                                         VkDevice* vulkanDevice, VkResult* vulkanResult)
    {
        PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(
            xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanDeviceKHR)));

        return pfnCreateVulkanDeviceKHR(instance, createInfo, vulkanDevice, vulkanResult);
    }

    XrResult VulkanGraphicsPluginLegacy::CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo,
                                                               VkDevice* vulkanDevice, VkResult* vulkanResult)
    {
        PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanDeviceExtensionsKHR)));

        uint32_t deviceExtensionNamesSize = 0;
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, createInfo->systemId, 0, &deviceExtensionNamesSize, nullptr));
        std::vector<char> deviceExtensionNames(deviceExtensionNamesSize);
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, createInfo->systemId, deviceExtensionNamesSize,
                                                              &deviceExtensionNamesSize, &deviceExtensionNames[0]));
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions = ParseExtensionString(&deviceExtensionNames[0]);

            // Merge the runtime's request with the applications requests
            for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
                extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
            }

            VkPhysicalDeviceFeatures features{};
            memcpy(&features, createInfo->vulkanCreateInfo->pEnabledFeatures, sizeof(features));

#if !defined(XR_USE_PLATFORM_ANDROID)
            VkPhysicalDeviceFeatures availableFeatures{};
            vkGetPhysicalDeviceFeatures(m_vkPhysicalDevice, &availableFeatures);
            if (availableFeatures.shaderStorageImageMultisample == VK_TRUE) {
                // Setting this quiets down a validation error triggered by the Oculus runtime
                features.shaderStorageImageMultisample = VK_TRUE;
            }
#endif

            VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            memcpy(&deviceInfo, createInfo->vulkanCreateInfo, sizeof(deviceInfo));
            deviceInfo.pEnabledFeatures = &features;
            deviceInfo.enabledExtensionCount = (uint32_t)extensions.size();
            deviceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

            auto pfnCreateDevice = (PFN_vkCreateDevice)createInfo->pfnGetInstanceProcAddr(m_vkInstance, "vkCreateDevice");
            *vulkanResult = pfnCreateDevice(m_vkPhysicalDevice, &deviceInfo, createInfo->vulkanAllocator, vulkanDevice);
        }

        return XR_SUCCESS;
    }

    bool VulkanGraphicsPlugin::Initialize()
    {
        if (initialized) {
            return false;
        }
        m_graphicsBinding.type = GetGraphicsBindingType();

        // To do.
        initialized = true;
        return initialized;
    }

    bool VulkanGraphicsPlugin::IsInitialized() const
    {
        return initialized;
    }

    void VulkanGraphicsPlugin::Shutdown()
    {
        if (initialized) {
            // To do.
            initialized = false;
        }
    }

    std::string VulkanGraphicsPlugin::DescribeGraphics() const
    {
        std::string gpu = "";
        if (m_vkPhysicalDevice != VK_NULL_HANDLE) {
            PFN_vkGetPhysicalDeviceProperties2KHR pfnvkGetPhysicalDeviceProperties2KHR =
                (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(m_vkInstance, "vkGetPhysicalDeviceProperties2KHR");
            if (pfnvkGetPhysicalDeviceProperties2KHR) {
                VkPhysicalDeviceIDPropertiesKHR gpuDevID{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR};
                VkPhysicalDeviceProperties2KHR gpuProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, &gpuDevID};
                pfnvkGetPhysicalDeviceProperties2KHR(m_vkPhysicalDevice, &gpuProps);
                std::string deviceType = "unknown";
                switch (gpuProps.properties.deviceType) {
                case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                    deviceType = "<other>";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    deviceType = "<integrated>";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    deviceType = "<discrete>";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    deviceType = "<virtual>";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    deviceType = "<cpu>";
                    break;
                // VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE was removed from vulkan headers
                // case VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE:
                case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
                default:
                    break;
                }

                gpu += "\nGPU: " + std::string(gpuProps.properties.deviceName) + " " + deviceType;
#if !defined(NDEBUG)  // CMAKE defines this
                gpu += "\nLUID: " + (gpuDevID.deviceLUIDValid ? to_hex(gpuDevID.deviceLUID) : std::string("<invalid>"));
#endif
            }
        }

        return std::string("Vulkan" + gpu);
    }

    bool VulkanGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                                uint32_t deviceCreationFlags)
    {
        // Create the Vulkan device for the adapter associated with the system.
        // Extension function must be loaded by name

        if (checkGraphicsRequirements) {
            XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
            XRC_CHECK_THROW_XRCMD(GetVulkanGraphicsRequirements2KHR(instance, systemId, &graphicsRequirements));
            const XrVersion vulkanVersion = XR_MAKE_VERSION(VK_VERSION_MAJOR(VK_API_VERSION_1_0), VK_VERSION_MINOR(VK_API_VERSION_1_0), 0);
            if ((vulkanVersion < graphicsRequirements.minApiVersionSupported) ||
                (vulkanVersion > graphicsRequirements.maxApiVersionSupported)) {
                // Log?
                return false;
            }
        }

        VkResult err;
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions;
            {
                uint32_t extensionCount = 0;
                XRC_CHECK_THROW_VKCMD(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

                std::vector<VkExtensionProperties> availableExtensions(extensionCount);
                XRC_CHECK_THROW_VKCMD(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()));
                const auto b = availableExtensions.begin();
                const auto e = availableExtensions.end();

                auto isExtSupported = [&](const char* extName) -> bool {
                    auto it = std::find_if(
                        b, e, [&](const VkExtensionProperties& properties) { return (0 == strcmp(extName, properties.extensionName)); });
                    return (it != e);
                };

                // Debug utils is optional and not always available
                if (isExtSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                }
                // TODO add back VK_EXT_debug_report code for compatibility with older systems? (Android)
            }

            std::vector<const char*> layers;
#if !defined(NDEBUG)
            auto GetValidationLayerName = []() -> const char* {
                uint32_t layerCount;
                vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
                std::vector<VkLayerProperties> availableLayers(layerCount);
                vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

                // Enable only one validation layer, prefer KHRONOS.
                for (auto validationLayerName : {"VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_standard_validation"}) {
                    for (const auto& layerProperties : availableLayers) {
                        if (0 == strcmp(validationLayerName, layerProperties.layerName)) {
                            return validationLayerName;
                        }
                    }
                }

                return nullptr;
            };
            const char* validationLayerName = GetValidationLayerName();
            if (validationLayerName)
                layers.push_back(validationLayerName);
            else
                ReportF("No Vulkan validation layers found, running without them");
#endif
#if defined(USE_CHECKPOINTS)
            layers.push_back("VK_NV_device_diagnostic_checkpoints");
#endif

            VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            appInfo.pApplicationName = "conformance_test";
            appInfo.applicationVersion = 1;
            appInfo.pEngineName = "conformance_test";
            appInfo.engineVersion = 1;
            appInfo.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            instInfo.pApplicationInfo = &appInfo;
            instInfo.enabledLayerCount = (uint32_t)layers.size();
            instInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
            instInfo.enabledExtensionCount = (uint32_t)extensions.size();
            instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

            XrVulkanInstanceCreateInfoKHR createInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
            createInfo.systemId = systemId;
            createInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
            createInfo.vulkanCreateInfo = &instInfo;
            createInfo.vulkanAllocator = nullptr;
            XRC_CHECK_THROW_XRCMD(CreateVulkanInstanceKHR(instance, &createInfo, &m_vkInstance, &err));
            XRC_CHECK_THROW_VKCMD(err);
        }

#if defined(USE_CHECKPOINTS)
        vkCmdSetCheckpointNV = (PFN_vkCmdSetCheckpointNV)vkGetInstanceProcAddr(m_vkInstance, "vkCmdSetCheckpointNV");
        vkGetQueueCheckpointDataNV = (PFN_vkGetQueueCheckpointDataNV)vkGetInstanceProcAddr(m_vkInstance, "vkGetQueueCheckpointDataNV");
#endif

        vkCreateDebugUtilsMessengerEXT =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugUtilsMessengerEXT");
        vkDestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkDestroyDebugUtilsMessengerEXT");

        if (vkCreateDebugUtilsMessengerEXT != nullptr && vkDestroyDebugUtilsMessengerEXT != nullptr) {
            VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
#if !defined(NDEBUG)
            debugInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
#endif
            debugInfo.pfnUserCallback = debugMessageThunk;
            debugInfo.pUserData = this;
            XRC_CHECK_THROW_VKCMD(vkCreateDebugUtilsMessengerEXT(m_vkInstance, &debugInfo, nullptr, &m_vkDebugUtilsMessenger));
        }

        XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        deviceGetInfo.systemId = systemId;
        deviceGetInfo.vulkanInstance = m_vkInstance;
        XRC_CHECK_THROW_XRCMD(GetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &m_vkPhysicalDevice));

        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        float queuePriorities = 0;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriorities;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, &queueFamilyProps[0]);

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            // Only need graphics (not presentation) for draw queue
            if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
                m_queueFamilyIndex = queueInfo.queueFamilyIndex = i;
                break;
            }
        }

        std::vector<const char*> deviceExtensions;

        VkPhysicalDeviceFeatures features{};
        // features.samplerAnisotropy = VK_TRUE;
        // Setting this quiets down a validation error triggered by the Oculus runtime
        // features.shaderStorageImageMultisample = VK_TRUE;

        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.flags = VkDeviceCreateFlags(deviceCreationFlags);
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledLayerCount = 0;
        deviceInfo.ppEnabledLayerNames = nullptr;
        deviceInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
        deviceInfo.pEnabledFeatures = &features;

        XrVulkanDeviceCreateInfoKHR deviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        deviceCreateInfo.systemId = systemId;
        deviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
        deviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
        deviceCreateInfo.vulkanAllocator = nullptr;
        XRC_CHECK_THROW_XRCMD(CreateVulkanDeviceKHR(instance, &deviceCreateInfo, &m_vkDevice, &err));
        XRC_CHECK_THROW_VKCMD(err);

        m_namer.Init(m_vkInstance, m_vkDevice);

        vkGetDeviceQueue(m_vkDevice, queueInfo.queueFamilyIndex, 0, &m_vkQueue);

        m_memAllocator.Init(m_vkPhysicalDevice, m_vkDevice);

        InitializeResources();

        m_graphicsBinding.instance = m_vkInstance;
        m_graphicsBinding.physicalDevice = m_vkPhysicalDevice;
        m_graphicsBinding.device = m_vkDevice;
        m_graphicsBinding.queueFamilyIndex = queueInfo.queueFamilyIndex;
        m_graphicsBinding.queueIndex = 0;

        return true;
    }

#ifdef USE_ONLINE_VULKAN_SHADERC
    // Compile a shader to a SPIR-V binary.
    std::vector<uint32_t> VulkanGraphicsPlugin::CompileGlslShader(const std::string& name, shaderc_shader_kind kind,
                                                                  const std::string& source)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetOptimizationLevel(shaderc_optimization_level_size);

        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, name.c_str(), options);

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            XRC_THROW("Shader " + name + " compilation failed: " + module.GetErrorMessage());
        }

        return {module.cbegin(), module.cend()};
    }
#endif

    void VulkanGraphicsPlugin::InitializeResources()
    {
#ifdef USE_ONLINE_VULKAN_SHADERC
        auto vertexSPIRV = CompileGlslShader("vertex", shaderc_glsl_default_vertex_shader, VertexShaderGlsl);
        auto fragmentSPIRV = CompileGlslShader("fragment", shaderc_glsl_default_fragment_shader, FragmentShaderGlsl);
#else
        std::vector<uint32_t> vertexSPIRV = SPV_PREFIX
#include "vert.spv"  // IWYU pragma: keep
            SPV_SUFFIX;
        std::vector<uint32_t> fragmentSPIRV = SPV_PREFIX
#include "frag.spv"  // IWYU pragma: keep
            SPV_SUFFIX;
#endif
        if (vertexSPIRV.empty())
            XRC_THROW("Failed to compile vertex shader");
        if (fragmentSPIRV.empty())
            XRC_THROW("Failed to compile fragment shader");

        m_shaderProgram.Init(m_vkDevice);
        m_shaderProgram.LoadVertexShader(vertexSPIRV);
        m_shaderProgram.LoadFragmentShader(fragmentSPIRV);

        // Semaphore to block on draw complete
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        XRC_CHECK_THROW_VKCMD(vkCreateSemaphore(m_vkDevice, &semInfo, nullptr, &m_vkDrawDone));
        XRC_CHECK_THROW_VKCMD(m_namer.SetName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)m_vkDrawDone, "CTS draw done semaphore"));

        if (!m_cmdBuffer.Init(m_namer, m_vkDevice, m_queueFamilyIndex))
            XRC_THROW("Failed to create command buffer");

        m_pipelineLayout.Create(m_vkDevice);

        static_assert(sizeof(Geometry::Vertex) == 24, "Unexpected Vertex size");

        m_cubeMesh = MakeCubeMesh();

        m_pbrResources = std::make_unique<Pbr::VulkanResources>(m_namer, m_vkPhysicalDevice, m_vkDevice, m_queueFamilyIndex);
        m_pbrResources->SetLight({0.0f, 0.7071067811865475f, 0.7071067811865475f}, Pbr::RGB::White);

        auto blackCubeMap = std::make_shared<Pbr::VulkanTextureBundle>(
            Pbr::VulkanTexture::CreateFlatCubeTexture(*m_pbrResources, Pbr::RGBA::Black, VK_FORMAT_R8G8B8A8_UNORM));
        m_pbrResources->SetEnvironmentMap(blackCubeMap, blackCubeMap);

        // Read the BRDF Lookup Table used by the PBR system into a DirectX texture.
        std::vector<unsigned char> brdfLutFileData = ReadFileBytes("brdf_lut.png");
        auto brdLutResourceView = std::make_shared<Pbr::VulkanTextureBundle>(
            Pbr::VulkanTexture::LoadTextureImage(*m_pbrResources, brdfLutFileData.data(), (uint32_t)brdfLutFileData.size()));
        m_pbrResources->SetBrdfLut(brdLutResourceView);

#if defined(USE_MIRROR_WINDOW)
        m_swapchain.Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice, m_graphicsBinding.queueFamilyIndex);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();
        m_swapchain.Prepare(m_cmdBuffer.buf);
        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        m_cmdBuffer.Wait();
#endif
    }

    void VulkanGraphicsPlugin::ClearSwapchainCache()
    {
        m_swapchainImageDataMap.Reset();
    }

    void VulkanGraphicsPlugin::ShutdownDevice()
    {
        if (m_vkDevice != VK_NULL_HANDLE) {
            // Make sure we're idle.
            vkDeviceWaitIdle(m_vkDevice);

            // Reset the swapchains to avoid calling Vulkan functions in the dtors after
            // we've shut down the device.

            m_pbrResources.reset();

            m_swapchainImageDataMap.Reset();
            m_cubeMesh = {};
            m_meshes.clear();
            m_gltfInstances.clear();
            m_gltfModels.clear();
            m_pbrResources.reset();

            m_queueFamilyIndex = 0;
            m_vkQueue = VK_NULL_HANDLE;
            if (m_vkDrawDone) {
                vkDestroySemaphore(m_vkDevice, m_vkDrawDone, nullptr);
                m_vkDrawDone = VK_NULL_HANDLE;
            }

            m_cmdBuffer.Reset();
            m_pipelineLayout.Reset();
            m_shaderProgram.Reset();
            m_memAllocator.Reset();

#if defined(USE_MIRROR_WINDOW)
            m_swapchain.Reset();
            m_swapchainImageData.clear();
#endif
            vkDestroyDevice(m_vkDevice, nullptr);
            m_vkDevice = VK_NULL_HANDLE;
        }
        if (m_vkDebugUtilsMessenger != VK_NULL_HANDLE && vkDestroyDebugUtilsMessengerEXT != nullptr) {
            vkDestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugUtilsMessenger, nullptr);
            m_vkDebugUtilsMessenger = VK_NULL_HANDLE;
        }
        if (m_vkInstance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_vkInstance, nullptr);
            m_vkInstance = VK_NULL_HANDLE;
        }
    }

    const XrBaseInStructure* VulkanGraphicsPlugin::GetGraphicsBinding() const
    {
        if (m_graphicsBinding.device) {
            return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
        }
        return nullptr;
    }

    static const SwapchainFormatDataMap& GetSwapchainFormatData()
    {

        // Add SwapchainCreateTestParameters for other Vulkan formats if they are supported by a runtime
        static SwapchainFormatDataMap map{{

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8A8_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8A8_SRGB).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_B8G8R8A8_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_B8G8R8A8_SRGB).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8_SRGB).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_B8G8R8_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_B8G8R8_SRGB).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8_UNORM).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8_UNORM).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8_SNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8_SNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8_SNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8A8_SNORM).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8A8_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8B8A8_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8_SRGB).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R8G8_SRGB).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16_UNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16A16_UNORM).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16_SNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16_SNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16_SNORM).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16A16_SNORM).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16A16_UINT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16A16_SINT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16_SFLOAT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16_SFLOAT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16_SFLOAT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16A16_SFLOAT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32B32_SINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32B32A32_SINT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32B32_UINT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32B32A32_UINT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32_SFLOAT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32_SFLOAT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32B32_SFLOAT).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R32G32B32A32_SFLOAT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R5G5B5A1_UNORM_PACK16).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R5G6B5_UNORM_PACK16).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_A2B10G10R10_UNORM_PACK32).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R4G4B4A4_UNORM_PACK16).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_A1R5G5B5_UNORM_PACK16).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_A2R10G10B10_UNORM_PACK32).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_A2R10G10B10_UINT_PACK32).ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_A2B10G10R10_UNORM_PACK32).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_A2B10G10R10_UINT_PACK32).ToPair(),
            // Runtimes with D3D11 back-ends map VK_FORMAT_B10G11R11_UFLOAT_PACK32 to DXGI_FORMAT_R11G11B10_FLOAT and that format doesn't have a TYPELESS equivalent.
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_B10G11R11_UFLOAT_PACK32).NotMutable().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_R16G16B16A16_SFLOAT).ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_D16_UNORM).Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_D24_UNORM_S8_UINT).DepthStencil().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_X8_D24_UNORM_PACK32).Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_S8_UINT).Stencil().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_D32_SFLOAT).Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_D32_SFLOAT_S8_UINT).DepthStencil().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_EAC_R11_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_EAC_R11G11_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_EAC_R11_SNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_EAC_R11G11_SNORM_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_4x4_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_5x4_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_5x5_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_6x5_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_6x6_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_8x5_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_8x6_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_8x8_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x5_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x6_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x8_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x10_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_12x10_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_12x12_UNORM_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_4x4_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_5x4_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_5x5_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_6x5_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_6x6_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_8x5_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_8x6_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_8x8_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x5_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x6_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x8_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_10x10_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_12x10_SRGB_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_ASTC_12x12_SRGB_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC1_RGBA_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC1_RGBA_SRGB_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC2_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC2_SRGB_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC3_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC3_SRGB_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC6H_UFLOAT_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC6H_SFLOAT_BLOCK).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC7_UNORM_BLOCK).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(VK_FORMAT_BC7_SRGB_BLOCK).Compressed().ToPair(),
        }};
        return map;
    }

    std::string VulkanGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        return ::Conformance::GetImageFormatName(GetSwapchainFormatData(), imageFormat);
    }

    bool VulkanGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        return ::Conformance::IsImageFormatKnown(GetSwapchainFormatData(), imageFormat);
    }

    bool VulkanGraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                                int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        *swapchainTestParameters = ::Conformance::GetSwapchainCreateTestParameters(GetSwapchainFormatData(), imageFormat);
        return true;
    }

    bool VulkanGraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* /*tp*/,
                                                       XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
        *imageCount = 0;  // Zero until set below upon success.

        std::vector<XrSwapchainImageVulkanKHR> swapchainImageVector;
        uint32_t countOutput;

        XrResult result = xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr);
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput > 0);

        swapchainImageVector.resize(countOutput, XrSwapchainImageVulkanKHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});

        // Exercise XR_ERROR_SIZE_INSUFFICIENT
        if (countOutput >= 2) {  // Need at least two in order to exercise XR_ERROR_SIZE_INSUFFICIENT
            result = xrEnumerateSwapchainImages(swapchain, 1, &countOutput,
                                                reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
            CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
            CHECK(result == XR_ERROR_SIZE_INSUFFICIENT);
            CHECK(countOutput == swapchainImageVector.size());
            // Contents of swapchainImageVector is undefined, so nothing to validate about the output.
        }

        countOutput = (uint32_t)swapchainImageVector.size();  // Restore countOutput if it was (mistakenly) modified.
        swapchainImageVector.clear();                         // Who knows what the runtime may have mistakely written into our vector.
        swapchainImageVector.resize(countOutput, XrSwapchainImageVulkanKHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        result = xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput == swapchainImageVector.size());
        REQUIRE(ValidateStructVectorType(swapchainImageVector, XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR));

        for (const XrSwapchainImageVulkanKHR& image : swapchainImageVector) {
            // Verify that the image is a valid handle
            CHECK(image.image != VK_NULL_HANDLE);
        }

        *imageCount = countOutput;
        return true;
    }

    bool VulkanGraphicsPlugin::ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
    {
        // OK to use CHECK and REQUIRE in here because this is always called from within a test.
        // TODO: check VK_ACCESS_*? Mandate this in the spec?
        return true;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t VulkanGraphicsPlugin::SelectColorSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported color swapchain formats.
        const std::array<VkFormat, 4> f{VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_FORMAT_B8G8R8A8_UNORM};

        span<const int64_t> formatArraySpan{formatArray, count};
        auto it = std::find_first_of(formatArraySpan.begin(), formatArraySpan.end(), f.begin(), f.end());

        if (it == formatArraySpan.end()) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t VulkanGraphicsPlugin::SelectDepthSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported depth swapchain formats.
        const std::array<VkFormat, 4> f{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM,
                                        VK_FORMAT_D32_SFLOAT_S8_UINT};

        span<const int64_t> formatArraySpan{formatArray, count};
        auto it = std::find_first_of(formatArraySpan.begin(), formatArraySpan.end(), f.begin(), f.end());

        if (it == formatArraySpan.end()) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t VulkanGraphicsPlugin::SelectMotionVectorSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported swapchain formats suitable for motion vectors.
        const std::array<VkFormat, 2> f{VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT};

        span<const int64_t> formatArraySpan{formatArray, count};
        auto it = std::find_first_of(formatArraySpan.begin(), formatArraySpan.end(), f.begin(), f.end());

        if (it == formatArraySpan.end()) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    int64_t VulkanGraphicsPlugin::GetSRGBA8Format() const
    {
        return VK_FORMAT_R8G8B8A8_SRGB;
    }

    ISwapchainImageData* VulkanGraphicsPlugin::AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto typedResult = std::make_unique<VulkanSwapchainImageData>(m_namer, uint32_t(size), swapchainCreateInfo, m_vkDevice,
                                                                      &m_memAllocator, m_pipelineLayout, m_shaderProgram,
                                                                      VulkanMesh::c_bindingDesc, VulkanMesh::c_attrDesc);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* VulkanGraphicsPlugin::AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo)
    {

        auto typedResult = std::make_unique<VulkanSwapchainImageData>(
            m_namer, uint32_t(size), colorSwapchainCreateInfo, depthSwapchain, depthSwapchainCreateInfo, m_vkDevice, &m_memAllocator,
            m_pipelineLayout, m_shaderProgram, VulkanMesh::c_bindingDesc, VulkanMesh::c_attrDesc);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    void VulkanGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImageBase, uint32_t arraySlice,
                                             const RGBAImage& image)
    {
        const XrSwapchainImageVulkanKHR* swapchainImageVk = reinterpret_cast<const XrSwapchainImageVulkanKHR*>(swapchainImageBase);

        VulkanSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImageBase);

        uint32_t w = image.width;
        uint32_t h = image.height;

        // Create a linear staging buffer
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imgInfo.imageType = VK_IMAGE_TYPE_2D;

        int64_t imageFormat = swapchainData->GetCreateInfo().format;
        XRC_CHECK_THROW(imageFormat == GetSRGBA8Format());

        imgInfo.format = static_cast<VkFormat>(imageFormat);
        imgInfo.extent = {w, h, 1};
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        VkImage stagingImage{VK_NULL_HANDLE};
        XRC_CHECK_THROW_VKCMD(vkCreateImage(m_vkDevice, &imgInfo, nullptr, &stagingImage));

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(m_vkDevice, stagingImage, &memReq);
        VkDeviceMemory stagingMemory{VK_NULL_HANDLE};
        m_memAllocator.Allocate(memReq, &stagingMemory, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        XRC_CHECK_THROW_VKCMD(vkBindImageMemory(m_vkDevice, stagingImage, stagingMemory, 0));

        VkImageSubresource imgSubRes{};
        imgSubRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgSubRes.mipLevel = 0;
        imgSubRes.arrayLayer = 0;
        VkSubresourceLayout layout{};
        vkGetImageSubresourceLayout(m_vkDevice, stagingImage, &imgSubRes, &layout);

        uint8_t* data{nullptr};
        XRC_CHECK_THROW_VKCMD(vkMapMemory(m_vkDevice, stagingMemory, layout.offset, layout.size, 0, (void**)&data));
        image.CopyWithStride(data, static_cast<uint32_t>(layout.rowPitch), static_cast<uint32_t>(layout.offset));
        vkUnmapMemory(m_vkDevice, stagingMemory);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();

        // Switch the staging buffer from PREINITIALIZED -> TRANSFER_SRC_OPTIMAL
        VkImageMemoryBarrier imgBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imgBarrier.srcQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.dstQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.image = stagingImage;
        imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(m_cmdBuffer.buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &imgBarrier);

        // Switch the destination image from COLOR_ATTACHMENT_OPTIMAL -> TRANSFER_DST_OPTIMAL
        //
        // XR_KHR_vulkan_enable / XR_KHR_vulkan_enable2
        // When an application acquires a swapchain image by calling xrAcquireSwapchainImage
        // in a session created using XrGraphicsBindingVulkanKHR, the OpenXR runtime must
        // guarantee that:
        // - The image has a memory layout compatible with VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        //   for color images, or VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL for depth images.
        // - The VkQueue specified in XrGraphicsBindingVulkanKHR has ownership of the image.
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.srcQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.dstQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.image = swapchainImageVk->image;
        imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, arraySlice, 1};
        vkCmdPipelineBarrier(m_cmdBuffer.buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &imgBarrier);

        // Blit staging -> swapchain
        VkImageBlit blit = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                            {{0, 0, 0}, {(int32_t)w, (int32_t)h, 1}},
                            {VK_IMAGE_ASPECT_COLOR_BIT, 0, arraySlice, 1},
                            {{0, 0, 0}, {(int32_t)w, (int32_t)h, 1}}};
        vkCmdBlitImage(m_cmdBuffer.buf, stagingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImageVk->image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

        // Switch the destination image from TRANSFER_DST_OPTIMAL -> COLOR_ATTACHMENT_OPTIMAL
        //
        // XR_KHR_vulkan_enable / XR_KHR_vulkan_enable2:
        // When an application releases a swapchain image by calling xrReleaseSwapchainImage,
        // in a session created using XrGraphicsBindingVulkanKHR, the OpenXR runtime must
        // interpret the image as:
        //  - Having a memory layout compatible with VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        //    for color images, or VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL for depth
        //    images.
        //  - Being owned by the VkQueue specified in XrGraphicsBindingVulkanKHR.
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imgBarrier.srcQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.dstQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.image = swapchainImageVk->image;
        imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, arraySlice, 1};
        vkCmdPipelineBarrier(m_cmdBuffer.buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &imgBarrier);

        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        m_cmdBuffer.Wait();

        vkDestroyImage(m_vkDevice, stagingImage, nullptr);
        vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
    }

    void VulkanGraphicsPlugin::SetViewportAndScissor(const VkRect2D& rect)
    {
        VkViewport viewport{float(rect.offset.x), float(rect.offset.y), float(rect.extent.width), float(rect.extent.height), 0.0f, 1.0f};
        vkCmdSetViewport(m_cmdBuffer.buf, 0, 1, &viewport);
        vkCmdSetScissor(m_cmdBuffer.buf, 0, 1, &rect);
    }

    /// Compute image layout for the "second image" format (depth and/or stencil)
    static inline VkImageLayout ComputeLayout(const SwapchainFormatData& secondFormatData)
    {
        // can't use VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL /
        //           VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL
        // because they're Vulkan 1.2
        if (secondFormatData.GetDepthFormat() || secondFormatData.GetStencilFormat()) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        if (secondFormatData.GetColorFormat()) {
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        throw std::runtime_error("No idea what layout to use for depth/stencil image");
    }

    /// Compute image aspect flags for the "second image" format (depth and/or stencil)
    static inline VkImageAspectFlags ComputeAspectFlags(const SwapchainFormatData& secondFormatData)
    {
        VkImageAspectFlags secondAttachmentAspect = 0;
        if (secondFormatData.GetDepthFormat()) {
            secondAttachmentAspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (secondFormatData.GetStencilFormat()) {
            secondAttachmentAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        if (secondFormatData.GetColorFormat()) {
            throw std::logic_error("this is just for the second image");
        }
        return secondAttachmentAspect;
    }

    const SwapchainFormatData& VulkanGraphicsPlugin::FindFormatData(int64_t format) const
    {
        auto& vkFormatMap = GetSwapchainFormatData();
        auto it = vkFormatMap.find(format);
        assert(it != vkFormatMap.end());
        return it->second;
    }

    void VulkanGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                               XrColor4f color)
    {
        VulkanSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();

        VkRect2D renderArea = {{0, 0}, {swapchainData->Width(), swapchainData->Height()}};
        SetViewportAndScissor(renderArea);

        // may be depth, stencil, or both
        int64_t secondImageFormat = swapchainData->GetDepthFormat();
        const SwapchainFormatData& secondFormatData = FindFormatData(secondImageFormat);

        VkImageAspectFlags secondAttachmentAspect = ComputeAspectFlags(secondFormatData);

        // Bind eye render target
        VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        swapchainData->BindRenderTarget(imageIndex, imageArrayIndex, renderArea, secondAttachmentAspect, &renderPassBeginInfo);

        if (!swapchainData->DepthSwapchainEnabled()) {
            // Ensure self-made fallback depth is in the right layout
            VkImageLayout layout = ComputeLayout(secondFormatData);
            swapchainData->TransitionLayout(imageIndex, &m_cmdBuffer, layout);
        }

        vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        swapchainData->BindPipeline(m_cmdBuffer.buf, imageArrayIndex);

        // Clear the buffers
        static std::array<VkClearValue, 2> clearValues;
        clearValues[0].color.float32[0] = color.r;
        clearValues[0].color.float32[1] = color.g;
        clearValues[0].color.float32[2] = color.b;
        clearValues[0].color.float32[3] = color.a;
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        std::array<VkClearAttachment, 2> clearAttachments{{
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, clearValues[0]},
            {secondAttachmentAspect, 0, clearValues[1]},
        }};
        // imageArrayIndex already included in the VkImageView
        VkClearRect clearRect{renderArea, 0, 1};
        vkCmdClearAttachments(m_cmdBuffer.buf, 2, &clearAttachments[0], 1, &clearRect);

        vkCmdEndRenderPass(m_cmdBuffer.buf);

        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        // XXX Should double-buffer the command buffers, for now just flush
        m_cmdBuffer.Wait();
    }

    MeshHandle VulkanGraphicsPlugin::MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx)
    {
        auto handle =
            m_meshes.emplace_back(m_vkDevice, &m_memAllocator, idx.data(), (uint32_t)idx.size(), vtx.data(), (uint32_t)vtx.size());

        return handle;
    }

    GLTFModelHandle VulkanGraphicsPlugin::LoadGLTF(std::shared_ptr<tinygltf::Model> tinygltfModel)
    {
        std::shared_ptr<Pbr::Model> pbrModel = Gltf::FromGltfObject(*m_pbrResources, *tinygltfModel);
        auto handle = m_gltfModels.emplace_back(std::move(pbrModel));
        return handle;
    }

    std::shared_ptr<Pbr::Model> VulkanGraphicsPlugin::GetPbrModel(GLTFModelHandle handle) const
    {
        return m_gltfModels[handle];
    }

    GLTFModelInstanceHandle VulkanGraphicsPlugin::CreateGLTFModelInstance(GLTFModelHandle handle)
    {
        auto pbrModelInstance = Pbr::VulkanModelInstance(*m_pbrResources, GetPbrModel(handle));
        auto instanceHandle = m_gltfInstances.emplace_back(std::move(pbrModelInstance));
        return instanceHandle;
    }

    Pbr::ModelInstance& VulkanGraphicsPlugin::GetModelInstance(GLTFModelInstanceHandle handle)
    {
        return m_gltfInstances[handle].GetModelInstance();
    }

    void VulkanGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                          const XrSwapchainImageBaseHeader* colorSwapchainImage, const RenderParams& params)
    {
        VulkanSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();

        CHECKPOINT();

        const XrRect2Di& r = layerView.subImage.imageRect;
        VkRect2D renderArea = {{r.offset.x, r.offset.y}, {uint32_t(r.extent.width), uint32_t(r.extent.height)}};
        SetViewportAndScissor(renderArea);

        // may be depth, stencil, or both
        int64_t secondImageFormat = swapchainData->GetDepthFormat();
        const SwapchainFormatData& secondFormatData = FindFormatData(secondImageFormat);
        VkImageAspectFlags secondAttachmentAspect = ComputeAspectFlags(secondFormatData);

        // Just bind the eye render target, ClearImageSlice will have cleared it.
        VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

        // aka slice
        auto imageArrayIndex = layerView.subImage.imageArrayIndex;

        swapchainData->BindRenderTarget(imageIndex, imageArrayIndex, renderArea, secondAttachmentAspect, &renderPassBeginInfo);

        vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        CHECKPOINT();

        swapchainData->BindPipeline(m_cmdBuffer.buf, imageArrayIndex);

        CHECKPOINT();

        // Compute the view-projection transform.
        // Note all matrixes (including OpenXR's) are column-major, right-handed.
        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrMatrix4x4f_CreateFromRigidTransform(&toView, &pose);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);
        MeshHandle lastMeshHandle;

        const auto drawMesh = [this, &vp, &lastMeshHandle](const MeshDrawable mesh) {
            VulkanMesh& vkMesh = m_meshes[mesh.handle];
            if (mesh.handle != lastMeshHandle) {
                // We are now rendering a new mesh

                // Bind index and vertex buffers
                vkCmdBindIndexBuffer(m_cmdBuffer.buf, vkMesh.m_DrawBuffer.idx.buf, 0, VK_INDEX_TYPE_UINT16);

                CHECKPOINT();

                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(m_cmdBuffer.buf, 0, 1, &vkMesh.m_DrawBuffer.vtx.buf, &offset);

                CHECKPOINT();
                lastMeshHandle = mesh.handle;
            }

            // Compute the model-view-projection transform and push it.
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &mesh.params.pose.position, &mesh.params.pose.orientation,
                                                        &mesh.params.scale);
            VulkanUniformBuffer ubuf;
            ubuf.tintColor = mesh.tintColor;
            XrMatrix4x4f_Multiply(&ubuf.mvp, &vp, &model);
            vkCmdPushConstants(m_cmdBuffer.buf, m_pipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VulkanUniformBuffer), &ubuf);

            CHECKPOINT();

            // Draw the mesh.
            vkCmdDrawIndexed(m_cmdBuffer.buf, vkMesh.m_DrawBuffer.count.idx, 1, 0, 0, 0);

            CHECKPOINT();
        };

        // Render each cube
        for (const Cube& cube : params.cubes) {
            drawMesh(MeshDrawable{m_cubeMesh, cube.params.pose, cube.params.scale, cube.tintColor});
        }

        // Render each mesh
        for (const auto& mesh : params.meshes) {
            drawMesh(mesh);
        }

        // Render each gltf
        for (const auto& gltfDrawable : params.glTFs) {
            VulkanGLTF& gltf = m_gltfInstances[gltfDrawable.handle];
            // Compute and update the model transform.

            XrMatrix4x4f modelToWorld;
            XrMatrix4x4f_CreateTranslationRotationScale(&modelToWorld, &gltfDrawable.params.pose.position,
                                                        &gltfDrawable.params.pose.orientation, &gltfDrawable.params.scale);
            // XrMatrix4x4f viewMatrix;
            // XrVector3f unitScale = {1, 1, 1};
            // XrMatrix4x4f_CreateTranslationRotationScale(&viewMatrix, &layerView.pose.position, &layerView.pose.orientation, &unitScale);
            // XrMatrix4x4f viewMatrixInverse;
            // XrMatrix4x4f_Invert(&viewMatrixInverse, &viewMatrix);
            m_pbrResources->SetViewProjection(view, proj);

            gltf.Render(m_cmdBuffer, *m_pbrResources, modelToWorld, renderPassBeginInfo.renderPass,
                        (VkSampleCountFlagBits)swapchainData->GetCreateInfo().sampleCount);
        }

        vkCmdEndRenderPass(m_cmdBuffer.buf);

        CHECKPOINT();

        m_pbrResources->SubmitFrameResources(m_vkQueue);

        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        // XXX Should double-buffer the command buffers, for now just flush
        m_cmdBuffer.Wait();

        m_pbrResources->Wait();

#if defined(USE_MIRROR_WINDOW)
        // Cycle the window's swapchain on the last view rendered
        if (swapchainData == &m_swapchainImageData.back()) {
            m_swapchain.Acquire();
            m_swapchain.Present(m_vkQueue);
        }
#endif
    }

#if defined(USE_CHECKPOINTS)
    static void ShowCheckpoints()
    {
        VulkanGraphicsPlugin* graphics = static_cast<VulkanGraphicsPlugin*>(GetGlobalData().graphicsPlugin.get());
        if (graphics)
            graphics->ShowCheckpoints();
    }
#endif

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan2(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<VulkanGraphicsPlugin>(std::move(platformPlugin));
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<VulkanGraphicsPluginLegacy>(std::move(platformPlugin));
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_VULKAN
