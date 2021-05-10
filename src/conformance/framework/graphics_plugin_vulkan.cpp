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

#include "graphics_plugin.h"

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include <list>
#include <unordered_set>
#include "report.h"
#include "hex_and_handles.h"
#include "swapchain_parameters.h"
#include "conformance_framework.h"
#include "xr_dependencies.h"
#include "Geometry.h"
#include <common/xr_linear.h>
#include <openxr/openxr_platform.h>

//#define USE_ONLINE_VULKAN_SHADERC
#ifdef USE_ONLINE_VULKAN_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#if defined(VK_USE_PLATFORM_WIN32_KHR)
// Define USE_MIRROR_WINDOW to open an otherwise-unused window for e.g. RenderDoc
//#define USE_MIRROR_WINDOW
#endif

// Define USE_CHECKPOINTS to use the nvidia checkpoint extension
//#define USE_CHECKPOINTS

// glslangValidator doesn't wrap its output in brackets if you don't have it define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

namespace Conformance
{
    static std::string vkResultString(VkResult res)
    {
        switch (res) {
        case VK_SUCCESS:
            return "SUCCESS";
        case VK_NOT_READY:
            return "NOT_READY";
        case VK_TIMEOUT:
            return "TIMEOUT";
        case VK_EVENT_SET:
            return "EVENT_SET";
        case VK_EVENT_RESET:
            return "EVENT_RESET";
        case VK_INCOMPLETE:
            return "INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV:
            return "ERROR_INVALID_SHADER_NV";
        default:
            return std::to_string(res);
        }
    }

#define LIST_PIPE_STAGES(_)           \
    _(TOP_OF_PIPE)                    \
    _(DRAW_INDIRECT)                  \
    _(VERTEX_INPUT)                   \
    _(VERTEX_SHADER)                  \
    _(TESSELLATION_CONTROL_SHADER)    \
    _(TESSELLATION_EVALUATION_SHADER) \
    _(GEOMETRY_SHADER)                \
    _(FRAGMENT_SHADER)                \
    _(EARLY_FRAGMENT_TESTS)           \
    _(LATE_FRAGMENT_TESTS)            \
    _(COLOR_ATTACHMENT_OUTPUT)        \
    _(COMPUTE_SHADER)                 \
    _(TRANSFER)                       \
    _(BOTTOM_OF_PIPE)                 \
    _(HOST)                           \
    _(ALL_GRAPHICS)                   \
    _(ALL_COMMANDS)

    std::string GetPipelineStages(VkPipelineStageFlags stages)
    {
        std::string desc;
#define MK_PIPE_STAGE_CHECK(n)                \
    if (stages & VK_PIPELINE_STAGE_##n##_BIT) \
        desc += " " #n;
        LIST_PIPE_STAGES(MK_PIPE_STAGE_CHECK)
#undef MK_PIPE_STAGE_CHECK
        return desc;
    }

    [[noreturn]] inline void ThrowVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr)
    {
        Throw("VkResult failure " + vkResultString(res), originator, sourceLocation);
    }

#if defined(USE_CHECKPOINTS)
#define _CHECKPOINT(line) Checkpoint(__FUNCTION__ ":" #line)
#define CHECKPOINT() _CHECKPOINT(__LINE__)
#define SHOW_CHECKPOINTS() ShowCheckpoints()
    static void ShowCheckpoints();
#else
#define CHECKPOINT() \
    do               \
        ;            \
    while (0)
#define SHOW_CHECKPOINTS() \
    do                     \
        ;                  \
    while (0)
#endif

    inline VkResult CheckThrowVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr)
    {
        if ((res) < VK_SUCCESS) {
            SHOW_CHECKPOINTS();
            ThrowVkResult(res, originator, sourceLocation);
        }

        return res;
    }

// XXX These really shouldn't have trailing ';'s
#define XRC_THROW_VK(res, cmd) ThrowVkResult(res, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_VKCMD(cmd) CheckThrowVkResult(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_VKRESULT(res, cmdStr) CheckThrowVkResult(res, cmdStr, XRC_FILE_AND_LINE);

#ifdef USE_ONLINE_VULKAN_SHADERC
    constexpr char VertexShaderGlsl[] =
        R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (std140, push_constant) uniform buf
    {
        mat4 mvp;
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
        oColor.rgba  = Color.rgba;
        gl_Position = ubuf.mvp * Position;
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

    struct MemoryAllocator
    {
        void Init(VkPhysicalDevice physicalDevice, VkDevice device)
        {
            m_vkDevice = device;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memProps);
        }

        void Reset()
        {
            m_memProps = {};
            m_vkDevice = VK_NULL_HANDLE;
        }

        static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        void Allocate(VkMemoryRequirements const& memReqs, VkDeviceMemory* mem, VkFlags flags = defaultFlags,
                      const void* pNext = nullptr) const
        {
            // Search memtypes to find first index with those properties
            for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
                if ((memReqs.memoryTypeBits & (1 << i)) != 0u) {
                    // Type is available, does it match user properties?
                    if ((m_memProps.memoryTypes[i].propertyFlags & flags) == flags) {
                        VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, pNext};
                        memAlloc.allocationSize = memReqs.size;
                        memAlloc.memoryTypeIndex = i;
                        XRC_CHECK_THROW_VKCMD(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, mem));
                        return;
                    }
                }
            }
            XRC_THROW("Memory format not supported");
        }

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
        VkPhysicalDeviceMemoryProperties m_memProps{};
    };

    // CmdBuffer - manage VkCommandBuffer state
    struct CmdBuffer
    {
        enum class CmdBufferState
        {
            Undefined,
            Initialized,
            Recording,
            Executable,
            Executing
        };

        CmdBufferState state{CmdBufferState::Undefined};
        VkCommandPool pool{VK_NULL_HANDLE};
        VkCommandBuffer buf{VK_NULL_HANDLE};
        VkFence execFence{VK_NULL_HANDLE};

        CmdBuffer() = default;

        CmdBuffer(const CmdBuffer&) = delete;
        CmdBuffer& operator=(const CmdBuffer&) = delete;
        CmdBuffer(CmdBuffer&&) = delete;
        CmdBuffer& operator=(CmdBuffer&&) = delete;

        void Reset()
        {
            SetState(CmdBufferState::Undefined);
            if (m_vkDevice != nullptr) {
                if (buf != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
                }
                if (pool != VK_NULL_HANDLE) {
                    vkDestroyCommandPool(m_vkDevice, pool, nullptr);
                }
                if (execFence != VK_NULL_HANDLE) {
                    vkDestroyFence(m_vkDevice, execFence, nullptr);
                }
            }
            buf = VK_NULL_HANDLE;
            pool = VK_NULL_HANDLE;
            execFence = VK_NULL_HANDLE;
            m_vkDevice = nullptr;
        }

        ~CmdBuffer()
        {
            Reset();
        }

        bool Init(VkDevice device, uint32_t queueFamilyIndex)
        {
            XRC_CHECK_THROW((state == CmdBufferState::Undefined) || (state == CmdBufferState::Initialized))

            m_vkDevice = device;

            // Create a command pool to allocate our command buffer from
            VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
            XRC_CHECK_THROW_VKCMD(vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &pool));

            // Create the command buffer from the command pool
            VkCommandBufferAllocateInfo cmd{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cmd.commandPool = pool;
            cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmd.commandBufferCount = 1;
            XRC_CHECK_THROW_VKCMD(vkAllocateCommandBuffers(m_vkDevice, &cmd, &buf));

            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            XRC_CHECK_THROW_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &execFence));

            SetState(CmdBufferState::Initialized);
            return true;
        }

        bool Begin()
        {
            XRC_CHECK_THROW(state == CmdBufferState::Initialized);
            VkCommandBufferBeginInfo cmdBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            XRC_CHECK_THROW_VKCMD(vkBeginCommandBuffer(buf, &cmdBeginInfo));
            SetState(CmdBufferState::Recording);
            return true;
        }

        bool End()
        {
            XRC_CHECK_THROW(state == CmdBufferState::Recording);
            XRC_CHECK_THROW_VKCMD(vkEndCommandBuffer(buf));
            SetState(CmdBufferState::Executable);
            return true;
        }

        bool Exec(VkQueue queue)
        {
            XRC_CHECK_THROW(state == CmdBufferState::Executable);

            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &buf;
            XRC_CHECK_THROW_VKCMD(vkQueueSubmit(queue, 1, &submitInfo, execFence));

            SetState(CmdBufferState::Executing);
            return true;
        }

        bool Wait()
        {
            // Waiting on a not-in-flight command buffer is a no-op
            if (state == CmdBufferState::Initialized) {
                return true;
            }

            XRC_CHECK_THROW(state == CmdBufferState::Executing);

            const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
            for (int i = 0; i < 5; ++i) {
                auto res = vkWaitForFences(m_vkDevice, 1, &execFence, VK_TRUE, timeoutNs);
                if (res == VK_SUCCESS) {
                    // Buffer can be executed multiple times...
                    SetState(CmdBufferState::Executable);
                    return true;
                }
                //Log::Write(Log::Level::Info, "Waiting for CmdBuffer fence timed out, retrying...");
            }

            return false;
        }

        bool Clear()
        {
            if (state != CmdBufferState::Initialized) {
                XRC_CHECK_THROW(state == CmdBufferState::Executable);

                XRC_CHECK_THROW_VKCMD(vkResetFences(m_vkDevice, 1, &execFence));
                XRC_CHECK_THROW_VKCMD(vkResetCommandBuffer(buf, 0));

                SetState(CmdBufferState::Initialized);
            }

            return true;
        }

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};

        void SetState(CmdBufferState newState)
        {
            state = newState;
        }
    };

    // ShaderProgram to hold a pair of vertex & fragment shaders
    struct ShaderProgram
    {
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderInfo{
            {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}}};

        ShaderProgram() = default;

        void Reset()
        {
            if (m_vkDevice != nullptr) {
                for (auto& si : shaderInfo) {
                    if (si.module != VK_NULL_HANDLE) {
                        vkDestroyShaderModule(m_vkDevice, shaderInfo[0].module, nullptr);
                    }
                    si.module = VK_NULL_HANDLE;
                }
            }
            shaderInfo = {};
            m_vkDevice = nullptr;
        }

        ~ShaderProgram()
        {
            Reset();
        }

        ShaderProgram(const ShaderProgram&) = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;
        ShaderProgram(ShaderProgram&&) = delete;
        ShaderProgram& operator=(ShaderProgram&&) = delete;

        void LoadVertexShader(const std::vector<uint32_t>& code)
        {
            Load(0, code);
        }

        void LoadFragmentShader(const std::vector<uint32_t>& code)
        {
            Load(1, code);
        }

        void Init(VkDevice device)
        {
            m_vkDevice = device;
        }

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};

        void Load(uint32_t index, const std::vector<uint32_t>& code)
        {
            VkShaderModuleCreateInfo modInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

            auto& si = shaderInfo[index];
            si.pName = "main";
            std::string name;

            switch (index) {
            case 0:
                si.stage = VK_SHADER_STAGE_VERTEX_BIT;
                name = "vertex";
                break;
            case 1:
                si.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                name = "fragment";
                break;
            default:
                XRC_THROW("Unknown code index " + std::to_string(index));
            }

            modInfo.codeSize = code.size() * sizeof(code[0]);
            modInfo.pCode = &code[0];
            XRC_CHECK_THROW_MSG((modInfo.codeSize > 0) && modInfo.pCode, "Invalid shader " + name);

            XRC_CHECK_THROW_VKCMD(vkCreateShaderModule(m_vkDevice, &modInfo, nullptr, &si.module));

            //Log::Write(Log::Level::Info, Fmt("Loaded %s shader", name.c_str()));
        }
    };

    // VertexBuffer base class
    struct VertexBufferBase
    {
        VkBuffer idxBuf{VK_NULL_HANDLE};
        VkDeviceMemory idxMem{VK_NULL_HANDLE};
        VkBuffer vtxBuf{VK_NULL_HANDLE};
        VkDeviceMemory vtxMem{VK_NULL_HANDLE};
        VkVertexInputBindingDescription bindDesc{};
        std::vector<VkVertexInputAttributeDescription> attrDesc{};
        struct
        {
            uint32_t idx;
            uint32_t vtx;
        } count = {0, 0};

        VertexBufferBase() = default;

        void Reset()
        {
            if (m_vkDevice != nullptr) {
                if (idxBuf != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_vkDevice, idxBuf, nullptr);
                }
                if (idxMem != VK_NULL_HANDLE) {
                    vkFreeMemory(m_vkDevice, idxMem, nullptr);
                }
                if (vtxBuf != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_vkDevice, vtxBuf, nullptr);
                }
                if (vtxMem != VK_NULL_HANDLE) {
                    vkFreeMemory(m_vkDevice, vtxMem, nullptr);
                }
            }
            idxBuf = VK_NULL_HANDLE;
            idxMem = VK_NULL_HANDLE;
            vtxBuf = VK_NULL_HANDLE;
            vtxMem = VK_NULL_HANDLE;
            bindDesc = {};
            attrDesc.clear();
            count = {0, 0};
            m_vkDevice = nullptr;
        }

        ~VertexBufferBase()
        {
            Reset();
        }

        VertexBufferBase(const VertexBufferBase&) = delete;
        VertexBufferBase& operator=(const VertexBufferBase&) = delete;
        VertexBufferBase(VertexBufferBase&&) = delete;
        VertexBufferBase& operator=(VertexBufferBase&&) = delete;
        void Init(VkDevice device, const MemoryAllocator* memAllocator, const std::vector<VkVertexInputAttributeDescription>& attr)
        {
            m_vkDevice = device;
            m_memAllocator = memAllocator;
            attrDesc = attr;
        }

    protected:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
        void AllocateBufferMemory(VkBuffer buf, VkDeviceMemory* mem) const
        {
            VkMemoryRequirements memReq = {};
            vkGetBufferMemoryRequirements(m_vkDevice, buf, &memReq);
            m_memAllocator->Allocate(memReq, mem);
        }

    private:
        const MemoryAllocator* m_memAllocator{nullptr};
    };

    // VertexBuffer template to wrap the indices and vertices
    template <typename T>
    struct VertexBuffer : public VertexBufferBase
    {
        bool Create(uint32_t idxCount, uint32_t vtxCount)
        {
            VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            bufInfo.size = sizeof(uint16_t) * idxCount;
            XRC_CHECK_THROW_VKCMD(vkCreateBuffer(m_vkDevice, &bufInfo, nullptr, &idxBuf));
            AllocateBufferMemory(idxBuf, &idxMem);
            XRC_CHECK_THROW_VKCMD(vkBindBufferMemory(m_vkDevice, idxBuf, idxMem, 0));

            bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufInfo.size = sizeof(T) * vtxCount;
            XRC_CHECK_THROW_VKCMD(vkCreateBuffer(m_vkDevice, &bufInfo, nullptr, &vtxBuf));
            AllocateBufferMemory(vtxBuf, &vtxMem);
            XRC_CHECK_THROW_VKCMD(vkBindBufferMemory(m_vkDevice, vtxBuf, vtxMem, 0));

            bindDesc.binding = 0;
            bindDesc.stride = sizeof(T);
            bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            count = {idxCount, vtxCount};

            return true;
        }

        void UpdateIndicies(const uint16_t* data, uint32_t elements, uint32_t offset = 0)
        {
            uint16_t* map = nullptr;
            XRC_CHECK_THROW_VKCMD(vkMapMemory(m_vkDevice, idxMem, sizeof(map[0]) * offset, sizeof(map[0]) * elements, 0, (void**)&map));
            for (size_t i = 0; i < elements; ++i) {
                map[i] = data[i];
            }
            vkUnmapMemory(m_vkDevice, idxMem);
        }

        void UpdateVertices(const T* data, uint32_t elements, uint32_t offset = 0)
        {
            T* map = nullptr;
            XRC_CHECK_THROW_VKCMD(vkMapMemory(m_vkDevice, vtxMem, sizeof(map[0]) * offset, sizeof(map[0]) * elements, 0, (void**)&map));
            for (size_t i = 0; i < elements; ++i) {
                map[i] = data[i];
            }
            vkUnmapMemory(m_vkDevice, vtxMem);
        }
    };

    // RenderPass wrapper
    struct RenderPass
    {
        VkFormat colorFmt{};
        VkFormat depthFmt{};
        VkRenderPass pass{VK_NULL_HANDLE};

        RenderPass() = default;

        bool Create(VkDevice device, VkFormat aColorFmt, VkFormat aDepthFmt)
        {
            m_vkDevice = device;
            colorFmt = aColorFmt;
            depthFmt = aDepthFmt;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

            std::array<VkAttachmentDescription, 2> at = {};

            VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            rpInfo.attachmentCount = 0;
            rpInfo.pAttachments = at.data();
            rpInfo.subpassCount = 1;
            rpInfo.pSubpasses = &subpass;

            if (colorFmt != VK_FORMAT_UNDEFINED) {
                colorRef.attachment = rpInfo.attachmentCount++;

                at[colorRef.attachment].format = colorFmt;
                at[colorRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
                at[colorRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                at[colorRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                at[colorRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                at[colorRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                at[colorRef.attachment].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                at[colorRef.attachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &colorRef;
            }

            if (depthFmt != VK_FORMAT_UNDEFINED) {
                depthRef.attachment = rpInfo.attachmentCount++;

                at[depthRef.attachment].format = depthFmt;
                at[depthRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
                at[depthRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                at[depthRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                at[depthRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                at[depthRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                at[depthRef.attachment].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                at[depthRef.attachment].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                subpass.pDepthStencilAttachment = &depthRef;
            }

            XRC_CHECK_THROW_VKCMD(vkCreateRenderPass(m_vkDevice, &rpInfo, nullptr, &pass));

            return true;
        }

        void Reset()
        {
            if (m_vkDevice != nullptr) {
                if (pass != VK_NULL_HANDLE) {
                    vkDestroyRenderPass(m_vkDevice, pass, nullptr);
                }
            }
            pass = VK_NULL_HANDLE;
            m_vkDevice = nullptr;
        }

        ~RenderPass()
        {
            Reset();
        }

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        RenderPass& operator=(RenderPass&&) = delete;

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
    };

    // VkImage + framebuffer wrapper
    struct RenderTarget
    {
        VkImage colorImage{VK_NULL_HANDLE};
        VkImage depthImage{VK_NULL_HANDLE};
        VkImageView colorView{VK_NULL_HANDLE};
        VkImageView depthView{VK_NULL_HANDLE};
        VkFramebuffer fb{VK_NULL_HANDLE};

        RenderTarget() = default;

        ~RenderTarget()
        {
            if (m_vkDevice != VK_NULL_HANDLE) {
                if (fb != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(m_vkDevice, fb, nullptr);
                }
                if (colorView != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_vkDevice, colorView, nullptr);
                }
                if (depthView != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_vkDevice, depthView, nullptr);
                }
            }

            // Note we don't own color/depthImage, it will get destroyed when xrDestroySwapchain is called
            colorImage = VK_NULL_HANDLE;
            depthImage = VK_NULL_HANDLE;
            colorView = VK_NULL_HANDLE;
            depthView = VK_NULL_HANDLE;
            fb = VK_NULL_HANDLE;
            m_vkDevice = VK_NULL_HANDLE;
        }

        RenderTarget(RenderTarget&& other) : RenderTarget()
        {
            using std::swap;
            swap(colorImage, other.colorImage);
            swap(depthImage, other.depthImage);
            swap(colorView, other.colorView);
            swap(depthView, other.depthView);
            swap(fb, other.fb);
            swap(m_vkDevice, other.m_vkDevice);
        }
        RenderTarget& operator=(RenderTarget&& other)
        {
            if (&other == this) {
                return *this;
            }
            // Clean up ourselves.
            this->~RenderTarget();
            using std::swap;
            swap(colorImage, other.colorImage);
            swap(depthImage, other.depthImage);
            swap(colorView, other.colorView);
            swap(depthView, other.depthView);
            swap(fb, other.fb);
            swap(m_vkDevice, other.m_vkDevice);
            return *this;
        }
        void Create(VkDevice device, VkImage aColorImage, VkImage aDepthImage, uint32_t baseArrayLayer, VkExtent2D size,
                    RenderPass& renderPass)
        {
            m_vkDevice = device;

            colorImage = aColorImage;
            depthImage = aDepthImage;

            std::array<VkImageView, 2> attachments{};
            uint32_t attachmentCount = 0;

            // Create color image view
            if (colorImage != VK_NULL_HANDLE) {
                VkImageViewCreateInfo colorViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                colorViewInfo.image = colorImage;
                colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                colorViewInfo.format = renderPass.colorFmt;
                colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                colorViewInfo.subresourceRange.baseMipLevel = 0;
                colorViewInfo.subresourceRange.levelCount = 1;
                colorViewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
                colorViewInfo.subresourceRange.layerCount = 1;
                XRC_CHECK_THROW_VKCMD(vkCreateImageView(m_vkDevice, &colorViewInfo, nullptr, &colorView));
                attachments[attachmentCount++] = colorView;
            }

            // Create depth image view
            if (depthImage != VK_NULL_HANDLE) {
                VkImageViewCreateInfo depthViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                depthViewInfo.image = depthImage;
                depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                depthViewInfo.format = renderPass.depthFmt;
                depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                depthViewInfo.subresourceRange.baseMipLevel = 0;
                depthViewInfo.subresourceRange.levelCount = 1;
                depthViewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
                depthViewInfo.subresourceRange.layerCount = 1;
                XRC_CHECK_THROW_VKCMD(vkCreateImageView(m_vkDevice, &depthViewInfo, nullptr, &depthView));
                attachments[attachmentCount++] = depthView;
            }

            VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbInfo.renderPass = renderPass.pass;
            fbInfo.attachmentCount = attachmentCount;
            fbInfo.pAttachments = attachments.data();
            fbInfo.width = size.width;
            fbInfo.height = size.height;
            fbInfo.layers = 1;
            XRC_CHECK_THROW_VKCMD(vkCreateFramebuffer(m_vkDevice, &fbInfo, nullptr, &fb));
        }

        RenderTarget(const RenderTarget&) = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
    };

    // Simple vertex MVP xform & color fragment shader layout
    struct PipelineLayout
    {
        VkPipelineLayout layout{VK_NULL_HANDLE};

        PipelineLayout() = default;

        void Reset()
        {
            if (m_vkDevice != nullptr) {
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(m_vkDevice, layout, nullptr);
                }
            }
            layout = VK_NULL_HANDLE;
            m_vkDevice = nullptr;
        }

        ~PipelineLayout()
        {
            Reset();
        }

        void Create(VkDevice device)
        {
            m_vkDevice = device;

            // MVP matrix is a push_constant
            VkPushConstantRange pcr = {};
            pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pcr.offset = 0;
            pcr.size = 4 * 4 * sizeof(float);

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
            pipelineLayoutCreateInfo.pPushConstantRanges = &pcr;
            XRC_CHECK_THROW_VKCMD(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &layout));
        }

        PipelineLayout(const PipelineLayout&) = delete;
        PipelineLayout& operator=(const PipelineLayout&) = delete;
        PipelineLayout(PipelineLayout&&) = delete;
        PipelineLayout& operator=(PipelineLayout&&) = delete;

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
    };

    // Pipeline wrapper for rendering pipeline state
    struct Pipeline
    {
        VkPipeline pipe{VK_NULL_HANDLE};
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        std::vector<VkDynamicState> dynamicStateEnables;

        Pipeline() = default;
        ~Pipeline()
        {
            Reset();
        }

        void Dynamic(VkDynamicState state)
        {
            dynamicStateEnables.emplace_back(state);
        }

        void Create(VkDevice device, VkExtent2D /*size*/, const PipelineLayout& layout, const RenderPass& rp, const ShaderProgram& sp,
                    const VertexBufferBase& vb)
        {
            m_vkDevice = device;

            VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
            dynamicState.pDynamicStates = dynamicStateEnables.data();

            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vi.vertexBindingDescriptionCount = 1;
            vi.pVertexBindingDescriptions = &vb.bindDesc;
            vi.vertexAttributeDescriptionCount = (uint32_t)vb.attrDesc.size();
            vi.pVertexAttributeDescriptions = vb.attrDesc.data();

            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            ia.primitiveRestartEnable = VK_FALSE;
            ia.topology = topology;

            VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.cullMode = VK_CULL_MODE_BACK_BIT;
            rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rs.depthClampEnable = VK_FALSE;
            rs.rasterizerDiscardEnable = VK_FALSE;
            rs.depthBiasEnable = VK_FALSE;
            rs.depthBiasConstantFactor = 0;
            rs.depthBiasClamp = 0;
            rs.depthBiasSlopeFactor = 0;
            rs.lineWidth = 1.0f;

            VkPipelineColorBlendAttachmentState attachState{};
            attachState.blendEnable = 0;
            attachState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachState.colorBlendOp = VK_BLEND_OP_ADD;
            attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachState.alphaBlendOp = VK_BLEND_OP_ADD;
            attachState.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            cb.attachmentCount = 1;
            cb.pAttachments = &attachState;
            cb.logicOpEnable = VK_FALSE;
            cb.logicOp = VK_LOGIC_OP_NO_OP;
            cb.blendConstants[0] = 1.0f;
            cb.blendConstants[1] = 1.0f;
            cb.blendConstants[2] = 1.0f;
            cb.blendConstants[3] = 1.0f;

            // Use dynamic scissor and viewport
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            vp.viewportCount = 1;
            vp.scissorCount = 1;

            VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            ds.depthTestEnable = VK_TRUE;
            ds.depthWriteEnable = VK_TRUE;
            ds.depthCompareOp = VK_COMPARE_OP_LESS;
            ds.depthBoundsTestEnable = VK_FALSE;
            ds.stencilTestEnable = VK_FALSE;
            ds.front.failOp = VK_STENCIL_OP_KEEP;
            ds.front.passOp = VK_STENCIL_OP_KEEP;
            ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
            ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
            ds.back = ds.front;
            ds.minDepthBounds = 0.0f;
            ds.maxDepthBounds = 1.0f;

            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkGraphicsPipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipeInfo.stageCount = (uint32_t)sp.shaderInfo.size();
            pipeInfo.pStages = sp.shaderInfo.data();
            pipeInfo.pVertexInputState = &vi;
            pipeInfo.pInputAssemblyState = &ia;
            pipeInfo.pTessellationState = nullptr;
            pipeInfo.pViewportState = &vp;
            pipeInfo.pRasterizationState = &rs;
            pipeInfo.pMultisampleState = &ms;
            pipeInfo.pDepthStencilState = &ds;
            pipeInfo.pColorBlendState = &cb;
            if (dynamicState.dynamicStateCount > 0) {
                pipeInfo.pDynamicState = &dynamicState;
            }
            pipeInfo.layout = layout.layout;
            pipeInfo.renderPass = rp.pass;
            pipeInfo.subpass = 0;
            XRC_CHECK_THROW_VKCMD(vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipe));
        }

        void Reset()
        {
            if (m_vkDevice != nullptr) {
                if (pipe != VK_NULL_HANDLE) {
                    vkDestroyPipeline(m_vkDevice, pipe, nullptr);
                }
            }
            pipe = VK_NULL_HANDLE;
            m_vkDevice = nullptr;
        }

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
    };

    struct DepthBuffer
    {
        VkDeviceMemory depthMemory{VK_NULL_HANDLE};
        VkImage depthImage{VK_NULL_HANDLE};

        DepthBuffer() = default;
        ~DepthBuffer()
        {
            Reset();
        }

        void Reset()
        {
            if (m_vkDevice != nullptr) {
                if (depthImage != VK_NULL_HANDLE) {
                    vkDestroyImage(m_vkDevice, depthImage, nullptr);
                }
                if (depthMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_vkDevice, depthMemory, nullptr);
                }
            }
            depthImage = VK_NULL_HANDLE;
            depthMemory = VK_NULL_HANDLE;
            m_vkDevice = nullptr;
        }

        DepthBuffer(DepthBuffer&& other) : DepthBuffer()
        {
            using std::swap;

            swap(depthImage, other.depthImage);
            swap(depthMemory, other.depthMemory);
            swap(m_vkDevice, other.m_vkDevice);
        }
        DepthBuffer& operator=(DepthBuffer&& other)
        {
            if (&other == this) {
                return *this;
            }
            // clean up self
            this->~DepthBuffer();
            using std::swap;

            swap(depthImage, other.depthImage);
            swap(depthMemory, other.depthMemory);
            swap(m_vkDevice, other.m_vkDevice);
            return *this;
        }

        void Create(VkDevice device, MemoryAllocator* memAllocator, VkFormat depthFormat, const XrSwapchainCreateInfo& swapchainCreateInfo)
        {
            m_vkDevice = device;

            VkExtent2D size = {swapchainCreateInfo.width, swapchainCreateInfo.height};

            // Create a D32 depthbuffer
            VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = size.width;
            imageInfo.extent.height = size.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = swapchainCreateInfo.arraySize;
            imageInfo.format = depthFormat;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            imageInfo.samples = (VkSampleCountFlagBits)swapchainCreateInfo.sampleCount;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            XRC_CHECK_THROW_VKCMD(vkCreateImage(device, &imageInfo, nullptr, &depthImage));

            VkMemoryRequirements memRequirements{};
            vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
            memAllocator->Allocate(memRequirements, &depthMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            XRC_CHECK_THROW_VKCMD(vkBindImageMemory(device, depthImage, depthMemory, 0));
        }

        void TransitionLayout(CmdBuffer* cmdBuffer, VkImageLayout newLayout)
        {
            if (newLayout == m_vkLayout) {
                return;
            }

            VkImageMemoryBarrier depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            depthBarrier.oldLayout = m_vkLayout;
            depthBarrier.newLayout = newLayout;
            depthBarrier.image = depthImage;
            depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(cmdBuffer->buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &depthBarrier);

            m_vkLayout = newLayout;
        }
        DepthBuffer(const DepthBuffer&) = delete;
        DepthBuffer& operator=(const DepthBuffer&) = delete;

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
        VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct SwapchainImageContext : public IGraphicsPlugin::SwapchainImageStructs
    {
        // A packed array of XrSwapchainImageVulkanKHR's for xrEnumerateSwapchainImages
        std::vector<XrSwapchainImageVulkanKHR> swapchainImages;
        VkExtent2D size{};
        class ArraySliceState
        {
        public:
            ArraySliceState()
            {
            }
            ArraySliceState(const ArraySliceState&)
            {
                ReportF("ArraySliceState copy ctor called");
            }
            std::vector<RenderTarget> renderTarget;  // per swapchain index
            DepthBuffer depthBuffer{};
            RenderPass rp{};
            Pipeline pipe{};
        };
        std::vector<ArraySliceState> slice{};

        SwapchainImageContext() = default;
        ~SwapchainImageContext()
        {
            Reset();
        }

        std::vector<XrSwapchainImageBaseHeader*> Create(VkDevice device, MemoryAllocator* memAllocator, uint32_t capacity,
                                                        const XrSwapchainCreateInfo& swapchainCreateInfo, const PipelineLayout& layout,
                                                        const ShaderProgram& sp, const VertexBuffer<Geometry::Vertex>& vb)
        {
            m_vkDevice = device;

            size = {swapchainCreateInfo.width, swapchainCreateInfo.height};
            VkFormat colorFormat = (VkFormat)swapchainCreateInfo.format;
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
            // XXX handle swapchainCreateInfo.sampleCount

            swapchainImages.resize(capacity);
            std::vector<XrSwapchainImageBaseHeader*> bases(capacity);
            for (uint32_t i = 0; i < capacity; ++i) {
                swapchainImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR};
                bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&swapchainImages[i]);
            }

            // Array slices could probably be handled via subpasses, but use a pipe/renderpass per slice for now
            slice.resize(swapchainCreateInfo.arraySize);
            for (auto& s : slice) {
                s.renderTarget.resize(capacity);
                s.depthBuffer.Create(m_vkDevice, memAllocator, depthFormat, swapchainCreateInfo);
                s.rp.Create(m_vkDevice, colorFormat, depthFormat);
                s.pipe.Dynamic(VK_DYNAMIC_STATE_SCISSOR);
                s.pipe.Dynamic(VK_DYNAMIC_STATE_VIEWPORT);
                s.pipe.Create(m_vkDevice, size, layout, s.rp, sp, vb);
            }

            return bases;
        }

        void Reset()
        {
            if (m_vkDevice) {
                swapchainImages.clear();
                size = {};
                slice.clear();
                m_vkDevice = VK_NULL_HANDLE;
            }
        }

        uint32_t ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader)
        {
            auto p = reinterpret_cast<const XrSwapchainImageVulkanKHR*>(swapchainImageHeader);
            return (uint32_t)(p - &swapchainImages[0]);
        }

        void BindRenderTarget(uint32_t index, uint32_t arraySlice, const VkRect2D& renderArea, VkRenderPassBeginInfo* renderPassBeginInfo)
        {
            auto& s = slice[arraySlice];
            RenderTarget& rt = s.renderTarget[index];
            if (rt.fb == VK_NULL_HANDLE) {
                rt.Create(m_vkDevice, swapchainImages[index].image, s.depthBuffer.depthImage, arraySlice, size, s.rp);
            }
            renderPassBeginInfo->renderPass = s.rp.pass;
            renderPassBeginInfo->framebuffer = rt.fb;
            renderPassBeginInfo->renderArea = renderArea;
        }

        void BindPipeline(VkCommandBuffer buf, uint32_t arraySlice)
        {
            vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, slice[arraySlice].pipe.pipe);
        }

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};
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
        // Make sure we're 1:1 for HMD pixels
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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

        CHECK(foundFmt < surfFmtCount);

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
        CHECK(presentable);

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

    struct VulkanGraphicsPlugin : public IGraphicsPlugin
    {
        VulkanGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/);

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

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

#ifdef USE_ONLINE_VULKAN_SHADERC
        std::vector<uint32_t> CompileGlslShader(const std::string& name, shaderc_shader_kind kind, const std::string& source);
#endif

        void InitializeResources();

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

        // Format required by RGBAImage type.
        int64_t GetSRGBA8Format() const override;

        std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
        AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImageBase, int64_t imageFormat, uint32_t arraySlice,
                           const RGBAImage& image) override;

        void SetViewportAndScissor(const VkRect2D& rect);

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                             int64_t colorSwapchainFormat) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        int64_t colorSwapchainFormat, const std::vector<Cube>& cubes) override;

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
        bool initialized;

    private:
        XrGraphicsBindingVulkanKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
#if defined(USE_MIRROR_WINDOW)
        std::list<std::shared_ptr<SwapchainImageContext>> m_swapchainImageContexts;
#endif
        std::map<const XrSwapchainImageBaseHeader*, std::shared_ptr<SwapchainImageContext>> m_swapchainImageContextMap;

        VkInstance m_vkInstance{VK_NULL_HANDLE};
        VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
        VkDevice m_vkDevice{VK_NULL_HANDLE};
        uint32_t m_queueFamilyIndex = 0;
        VkQueue m_vkQueue{VK_NULL_HANDLE};
        VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

        MemoryAllocator m_memAllocator{};
        ShaderProgram m_shaderProgram{};
        CmdBuffer m_cmdBuffer{};
        PipelineLayout m_pipelineLayout{};
        VertexBuffer<Geometry::Vertex> m_drawBuffer{};

#if defined(USE_MIRROR_WINDOW)
        Swapchain m_swapchain{};
#endif

#if defined(USE_CHECKPOINTS)
        PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV{};
        PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV{};
        std::unordered_set<std::string> checkpoints{};
#endif

        PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT{nullptr};
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT{nullptr};
        VkDebugReportCallbackEXT m_vkDebugReporter{VK_NULL_HANDLE};

        VkBool32 debugReport(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t /*object*/, size_t /*location*/,
                             int32_t /*messageCode*/, const char* pLayerPrefix, const char* pMessage)
        {
            std::string flagNames;
            std::string objName;
            //Log::Level level = Log::Level::Error;

            if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0u) {
                flagNames += "DEBUG:";
                //level = Log::Level::Verbose;
            }
            if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0u) {
                flagNames += "INFO:";
                //level = Log::Level::Info;
            }
            if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0u) {
                flagNames += "PERF:";
                //level = Log::Level::Warning;
            }
            if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0u) {
                flagNames += "WARN:";
                //level = Log::Level::Warning;
            }
            if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0u) {
                flagNames += "ERROR:";
                //level = Log::Level::Error;
            }

#define LIST_OBJECT_TYPES(_) \
    _(UNKNOWN)               \
    _(INSTANCE)              \
    _(PHYSICAL_DEVICE)       \
    _(DEVICE)                \
    _(QUEUE)                 \
    _(SEMAPHORE)             \
    _(COMMAND_BUFFER)        \
    _(FENCE)                 \
    _(DEVICE_MEMORY)         \
    _(BUFFER)                \
    _(IMAGE)                 \
    _(EVENT)                 \
    _(QUERY_POOL)            \
    _(BUFFER_VIEW)           \
    _(IMAGE_VIEW)            \
    _(SHADER_MODULE)         \
    _(PIPELINE_CACHE)        \
    _(PIPELINE_LAYOUT)       \
    _(RENDER_PASS)           \
    _(PIPELINE)              \
    _(DESCRIPTOR_SET_LAYOUT) \
    _(SAMPLER)               \
    _(DESCRIPTOR_POOL)       \
    _(DESCRIPTOR_SET)        \
    _(FRAMEBUFFER)           \
    _(COMMAND_POOL)          \
    _(SURFACE_KHR)           \
    _(SWAPCHAIN_KHR)         \
    _(DISPLAY_KHR)           \
    _(DISPLAY_MODE_KHR)

            switch (objectType) {
            default:
#define MK_OBJECT_TYPE_CASE(name)                  \
    case VK_DEBUG_REPORT_OBJECT_TYPE_##name##_EXT: \
        objName = #name;                           \
        break;
                LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)

#if VK_HEADER_VERSION >= 46
                MK_OBJECT_TYPE_CASE(DESCRIPTOR_UPDATE_TEMPLATE_KHR)
#endif
#if VK_HEADER_VERSION >= 70
                MK_OBJECT_TYPE_CASE(DEBUG_REPORT_CALLBACK_EXT)
#endif
            }

            if ((objectType == VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT) && (strcmp(pLayerPrefix, "Loader Message") == 0) &&
                (strncmp(pMessage, "Device Extension:", 17) == 0)) {
                return VK_FALSE;
            }

            //Log::Write(level, Fmt("%s (%s 0x%llx) [%s] %s", flagNames.c_str(), objName.c_str(), object, pLayerPrefix, pMessage));
            if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0u) {
                return VK_FALSE;
            }
            if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0u) {
                return VK_FALSE;
            }
            return VK_FALSE;
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportThunk(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                                               uint64_t object, size_t location, int32_t messageCode,
                                                               const char* pLayerPrefix, const char* pMessage, void* pUserData)
        {
            return static_cast<VulkanGraphicsPlugin*>(pUserData)->debugReport(flags, objectType, object, location, messageCode,
                                                                              pLayerPrefix, pMessage);
        }
    };

    VulkanGraphicsPlugin::VulkanGraphicsPlugin(const std::shared_ptr<IPlatformPlugin>& /*unused*/) : initialized(false)
    {
    }

    bool VulkanGraphicsPlugin::Initialize()
    {
        if (initialized) {
            return false;
        }

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
                }

                gpu += "\nGPU: " + std::string(gpuProps.properties.deviceName) + " " + deviceType;
#if !defined(NDEBUG)  // CMAKE defines this
                gpu += "\nLUID: " + (gpuDevID.deviceLUIDValid ? to_hex(gpuDevID.deviceLUID) : std::string("<invalid>"));
#endif
            }
        }

        return std::string("Vulkan" + gpu);
    }

    std::vector<std::string> VulkanGraphicsPlugin::GetInstanceExtensions() const
    {
        return {XR_KHR_VULKAN_ENABLE_EXTENSION_NAME};
    }

    bool VulkanGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                                uint32_t deviceCreationFlags)
    {
        // Create the Vulkan device for the adapter associated with the system.
        // Extension function must be loaded by name
        PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirementsKHR)));

        PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanInstanceExtensionsKHR)));

        if (checkGraphicsRequirements) {
            XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
            XRC_CHECK_THROW_XRCMD(pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));
            const XrVersion vulkanVersion = XR_MAKE_VERSION(VK_VERSION_MAJOR(VK_API_VERSION_1_0), VK_VERSION_MINOR(VK_API_VERSION_1_0), 0);
            if ((vulkanVersion < graphicsRequirements.minApiVersionSupported) ||
                (vulkanVersion > graphicsRequirements.maxApiVersionSupported)) {
                // Log?
                return false;
            }
        }

        uint32_t extensionNamesSize = 0;
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, systemId, 0, &extensionNamesSize, nullptr));
        std::vector<char> extensionNames(extensionNamesSize);
        XRC_CHECK_THROW_XRCMD(
            pfnGetVulkanInstanceExtensionsKHR(instance, systemId, extensionNamesSize, &extensionNamesSize, &extensionNames[0]));

        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions = ParseExtensionString(&extensionNames[0]);
            extensions.push_back("VK_EXT_debug_report");

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
                ReportF("No validation layers found, running without them");
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

            XRC_CHECK_THROW_VKCMD(vkCreateInstance(&instInfo, nullptr, &m_vkInstance));
        }

#if defined(USE_CHECKPOINTS)
        vkCmdSetCheckpointNV = (PFN_vkCmdSetCheckpointNV)vkGetInstanceProcAddr(m_vkInstance, "vkCmdSetCheckpointNV");
        vkGetQueueCheckpointDataNV = (PFN_vkGetQueueCheckpointDataNV)vkGetInstanceProcAddr(m_vkInstance, "vkGetQueueCheckpointDataNV");
#endif

        vkCreateDebugReportCallbackEXT =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_vkInstance, "vkDestroyDebugReportCallbackEXT");
        VkDebugReportCallbackCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT};
        debugInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
#if !defined(NDEBUG)
        debugInfo.flags |=
            VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
#endif
        debugInfo.pfnCallback = debugReportThunk;
        debugInfo.pUserData = this;
        XRC_CHECK_THROW_VKCMD(vkCreateDebugReportCallbackEXT(m_vkInstance, &debugInfo, nullptr, &m_vkDebugReporter));

        PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDeviceKHR)));

        XRC_CHECK_THROW_XRCMD(pfnGetVulkanGraphicsDeviceKHR(instance, systemId, m_vkInstance, &m_vkPhysicalDevice));

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

        PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = nullptr;
        XRC_CHECK_THROW_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanDeviceExtensionsKHR)));

        uint32_t deviceExtensionNamesSize = 0;
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, systemId, 0, &deviceExtensionNamesSize, nullptr));
        std::vector<char> deviceExtensionNames(deviceExtensionNamesSize);
        XRC_CHECK_THROW_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, systemId, deviceExtensionNamesSize, &deviceExtensionNamesSize,
                                                              &deviceExtensionNames[0]));
        std::vector<const char*> deviceExtensions = ParseExtensionString(&deviceExtensionNames[0]);

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

        XRC_CHECK_THROW_VKCMD(vkCreateDevice(m_vkPhysicalDevice, &deviceInfo, nullptr, &m_vkDevice));

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
#include "vert.spv"
            SPV_SUFFIX;
        std::vector<uint32_t> fragmentSPIRV = SPV_PREFIX
#include "frag.spv"
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

        if (!m_cmdBuffer.Init(m_vkDevice, m_queueFamilyIndex))
            XRC_THROW("Failed to create command buffer");

        m_pipelineLayout.Create(m_vkDevice);

        static_assert(sizeof(Geometry::Vertex) == 24, "Unexpected Vertex size");
        m_drawBuffer.Init(m_vkDevice, &m_memAllocator,
                          {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Geometry::Vertex, Position)},
                           {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Geometry::Vertex, Color)}});
        uint32_t numCubeIdicies = sizeof(Geometry::c_cubeIndices) / sizeof(Geometry::c_cubeIndices[0]);
        uint32_t numCubeVerticies = sizeof(Geometry::c_cubeVertices) / sizeof(Geometry::c_cubeVertices[0]);
        m_drawBuffer.Create(numCubeIdicies, numCubeVerticies);
        m_drawBuffer.UpdateIndicies(Geometry::c_cubeIndices.data(), numCubeIdicies, 0);
        m_drawBuffer.UpdateVertices(Geometry::c_cubeVertices.data(), numCubeVerticies, 0);

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

    void VulkanGraphicsPlugin::ShutdownDevice()
    {
        if (m_vkDevice != VK_NULL_HANDLE) {
            // Make sure we're idle.
            vkDeviceWaitIdle(m_vkDevice);

            // Reset the swapchains to avoid calling Vulkan functions in the dtors after
            // we've shut down the device.
            for (auto& ctx : m_swapchainImageContextMap) {
                ctx.second->Reset();
            }
            m_swapchainImageContextMap.clear();

            m_queueFamilyIndex = 0;
            m_vkQueue = VK_NULL_HANDLE;
            if (m_vkDrawDone) {
                vkDestroySemaphore(m_vkDevice, m_vkDrawDone, nullptr);
                m_vkDrawDone = VK_NULL_HANDLE;
            }

            m_drawBuffer.Reset();
            m_cmdBuffer.Reset();
            m_pipelineLayout.Reset();
            m_shaderProgram.Reset();
            m_memAllocator.Reset();

#if defined(USE_MIRROR_WINDOW)
            m_swapchain.Reset();
            m_swapchainImageContexts.clear();
#endif
            vkDestroyDevice(m_vkDevice, nullptr);
            m_vkDevice = VK_NULL_HANDLE;
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

    // Shorthand constants for usage below.
    static const uint64_t XRC_COLOR_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_MUTABLE = (XRC_COLOR_TEXTURE_USAGE | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_COMPRESSED =
        (XR_SWAPCHAIN_USAGE_SAMPLED_BIT);  // Compressed textures can't be rendered to, so no XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT.

    static const uint64_t XRC_DEPTH_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

#define XRC_COLOR_CREATE_FLAGS                                                             \
    {                                                                                      \
        0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

#define XRC_DEPTH_CREATE_FLAGS                                                             \
    {                                                                                      \
        0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

    // clang-format off
    // Add SwapchainCreateTestParameters for other Vulkan formats if they are supported by a runtime
    typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;
    SwapchainTestMap vkSwapchainTestMap{
        {{VK_FORMAT_R8G8B8A8_UNORM}, {"VK_FORMAT_R8G8B8A8_UNORM", false, true, true, false, VK_FORMAT_R8G8B8A8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8A8_SRGB}, {"VK_FORMAT_R8G8B8A8_SRGB", false, true, true, false, VK_FORMAT_R8G8B8A8_SRGB, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_B8G8R8A8_UNORM}, {"VK_FORMAT_B8G8R8A8_UNORM", false, true, true, false, VK_FORMAT_B8G8R8A8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_B8G8R8A8_SRGB}, {"VK_FORMAT_B8G8R8A8_SRGB", false, true, true, false, VK_FORMAT_B8G8R8A8_SRGB, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R8G8B8_UNORM}, {"VK_FORMAT_R8G8B8_UNORM", false, true, true, false, VK_FORMAT_R8G8B8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8_SRGB}, {"VK_FORMAT_R8G8B8_SRGB", false, true, true, false, VK_FORMAT_R8G8B8_SRGB, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_B8G8R8_UNORM}, {"VK_FORMAT_B8G8R8_UNORM", false, true, true, false, VK_FORMAT_B8G8R8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_B8G8R8_SRGB}, {"VK_FORMAT_B8G8R8_SRGB", false, true, true, false, VK_FORMAT_B8G8R8_SRGB, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R8G8_UNORM}, {"VK_FORMAT_R8G8_UNORM", false, true, true, false, VK_FORMAT_R8G8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R8_UNORM}, {"VK_FORMAT_R8_UNORM", false, true, true, false, VK_FORMAT_R8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R8_SNORM}, {"VK_FORMAT_R8_SNORM", false, true, true, false, VK_FORMAT_R8_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8_SNORM}, {"VK_FORMAT_R8G8_SNORM", false, true, true, false, VK_FORMAT_R8G8_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8_SNORM}, {"VK_FORMAT_R8G8B8_SNORM", false, true, true, false, VK_FORMAT_R8G8B8_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8A8_SNORM}, {"VK_FORMAT_R8G8B8A8_SNORM", false, true, true, false, VK_FORMAT_R8G8B8A8_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R8_UINT}, {"VK_FORMAT_R8_UINT", false, true, true, false, VK_FORMAT_R8_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8_UINT}, {"VK_FORMAT_R8G8_UINT", false, true, true, false, VK_FORMAT_R8G8_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8_UINT}, {"VK_FORMAT_R8G8B8_UINT", false, true, true, false, VK_FORMAT_R8G8B8_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8A8_UINT}, {"VK_FORMAT_R8G8B8A8_UINT", false, true, true, false, VK_FORMAT_R8G8B8A8_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8_SINT}, {"VK_FORMAT_R8_SINT", false, true, true, false, VK_FORMAT_R8_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8_SINT}, {"VK_FORMAT_R8G8_SINT", false, true, true, false, VK_FORMAT_R8G8_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8_SINT}, {"VK_FORMAT_R8G8B8_SINT", false, true, true, false, VK_FORMAT_R8G8B8_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8_UNORM}, {"VK_FORMAT_R8_UNORM", false, true, true, false, VK_FORMAT_R8_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8G8B8A8_SINT}, {"VK_FORMAT_R8G8B8A8_SINT", false, true, true, false, VK_FORMAT_R8G8B8A8_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R8_SRGB}, {"VK_FORMAT_R8_SRGB", false, true, true, false, VK_FORMAT_R8_SRGB, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R16_UNORM}, {"VK_FORMAT_R16_UNORM", false, true, true, false, VK_FORMAT_R16_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16_UNORM}, {"VK_FORMAT_R16G16_UNORM", false, true, true, false, VK_FORMAT_R16G16_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16_UNORM}, {"VK_FORMAT_R16G16B16_UNORM", false, true, true, false, VK_FORMAT_R16G16B16_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16A16_UNORM}, {"VK_FORMAT_R16G16B16A16_UNORM", false, true, true, false, VK_FORMAT_R16G16B16A16_UNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R16_SNORM}, {"VK_FORMAT_R16_SNORM", false, true, true, false, VK_FORMAT_R16_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16_SNORM}, {"VK_FORMAT_R16G16_SNORM", false, true, true, false, VK_FORMAT_R16G16_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16_SNORM}, {"VK_FORMAT_R16G16B16_SNORM", false, true, true, false, VK_FORMAT_R16G16B16_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16A16_SNORM}, {"VK_FORMAT_R16G16B16A16_SNORM", false, true, true, false, VK_FORMAT_R16G16B16A16_SNORM, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R16_UINT}, {"VK_FORMAT_R16_UINT", false, true, true, false, VK_FORMAT_R16_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16_UINT}, {"VK_FORMAT_R16G16_UINT", false, true, true, false, VK_FORMAT_R16G16_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16_UINT}, {"VK_FORMAT_R16G16B16_UINT", false, true, true, false, VK_FORMAT_R16G16B16_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16A16_UINT}, {"VK_FORMAT_R16G16B16A16_UINT", false, true, true, false, VK_FORMAT_R16G16B16A16_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R16_SINT}, {"VK_FORMAT_R16_SINT", false, true, true, false, VK_FORMAT_R16_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16_SINT}, {"VK_FORMAT_R16G16_SINT", false, true, true, false, VK_FORMAT_R16G16_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16_SINT}, {"VK_FORMAT_R16G16B16_SINT", false, true, true, false, VK_FORMAT_R16G16B16_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16A16_SINT}, {"VK_FORMAT_R16G16B16A16_SINT", false, true, true, false, VK_FORMAT_R16G16B16A16_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R16_SFLOAT}, {"VK_FORMAT_R16_SFLOAT", false, true, true, false, VK_FORMAT_R16_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16_SFLOAT}, {"VK_FORMAT_R16G16_SFLOAT", false, true, true, false, VK_FORMAT_R16G16_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16_SFLOAT}, {"VK_FORMAT_R16G16B16_SFLOAT", false, true, true, false, VK_FORMAT_R16G16B16_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R16G16B16A16_SFLOAT}, {"VK_FORMAT_R16G16B16A16_SFLOAT", false, true, true, false, VK_FORMAT_R16G16B16A16_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R32_SINT}, {"VK_FORMAT_R32_SINT", false, true, true, false, VK_FORMAT_R32_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32_SINT}, {"VK_FORMAT_R32G32_SINT", false, true, true, false, VK_FORMAT_R32G32_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32B32_SINT}, {"VK_FORMAT_R32G32B32_SINT", false, true, true, false, VK_FORMAT_R32G32B32_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32B32A32_SINT}, {"VK_FORMAT_R32G32B32A32_SINT", false, true, true, false, VK_FORMAT_R32G32B32A32_SINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R32_UINT}, {"VK_FORMAT_R32_UINT", false, true, true, false, VK_FORMAT_R32_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32_UINT}, {"VK_FORMAT_R32G32_UINT", false, true, true, false, VK_FORMAT_R32G32_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32B32_UINT}, {"VK_FORMAT_R32G32B32_UINT", false, true, true, false, VK_FORMAT_R32G32B32_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32B32A32_UINT}, {"VK_FORMAT_R32G32B32A32_UINT", false, true, true, false, VK_FORMAT_R32G32B32A32_UINT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R32_SFLOAT}, {"VK_FORMAT_R32_SFLOAT", false, true, true, false, VK_FORMAT_R32_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32_SFLOAT}, {"VK_FORMAT_R32G32_SFLOAT", false, true, true, false, VK_FORMAT_R32G32_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32B32_SFLOAT}, {"VK_FORMAT_R32G32B32_SFLOAT", false, true, true, false, VK_FORMAT_R32G32B32_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R32G32B32A32_SFLOAT}, {"VK_FORMAT_R32G32B32A32_SFLOAT", false, true, true, false, VK_FORMAT_R32G32B32A32_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R5G5B5A1_UNORM_PACK16}, {"VK_FORMAT_R5G5B5A1_UNORM_PACK16", false, true, true, false, VK_FORMAT_R5G5B5A1_UNORM_PACK16, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_R5G6B5_UNORM_PACK16}, {"VK_FORMAT_R5G6B5_UNORM_PACK16", false, true, true, false, VK_FORMAT_R5G6B5_UNORM_PACK16, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_A2B10G10R10_UNORM_PACK32}, {"VK_FORMAT_A2B10G10R10_UNORM_PACK32", false, true, true, false, VK_FORMAT_A2B10G10R10_UNORM_PACK32, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
	    
        {{VK_FORMAT_R4G4B4A4_UNORM_PACK16}, {"VK_FORMAT_R4G4B4A4_UNORM_PACK16", false, true, true, false, VK_FORMAT_R4G4B4A4_UNORM_PACK16, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_A1R5G5B5_UNORM_PACK16}, {"VK_FORMAT_A1R5G5B5_UNORM_PACK16", false, true, true, false, VK_FORMAT_A1R5G5B5_UNORM_PACK16, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_A2R10G10B10_UINT_PACK32}, {"VK_FORMAT_A2R10G10B10_UINT_PACK32", false, true, true, false, VK_FORMAT_A2R10G10B10_UINT_PACK32, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_A2B10G10R10_UNORM_PACK32}, {"VK_FORMAT_A2B10G10R10_UNORM_PACK32", false, true, true, false, VK_FORMAT_A2B10G10R10_UNORM_PACK32, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
	    
        {{VK_FORMAT_A2B10G10R10_UINT_PACK32}, {"VK_FORMAT_A2B10G10R10_UINT_PACK32", false, true, true, false, VK_FORMAT_A2B10G10R10_UINT_PACK32, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        // Runtimes with D3D11 back-ends map VK_FORMAT_B10G11R11_UFLOAT_PACK32 to DXGI_FORMAT_R11G11B10_FLOAT and that format doesn't have a TYPELESS equivalent.
        //{{VK_FORMAT_B10G11R11_UFLOAT_PACK32}, {"VK_FORMAT_B10G11R11_UFLOAT_PACK32", false, true, true, false, VK_FORMAT_B10G11R11_UFLOAT_PACK32, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_B10G11R11_UFLOAT_PACK32}, {"VK_FORMAT_B10G11R11_UFLOAT_PACK32", false, false, true, false, VK_FORMAT_B10G11R11_UFLOAT_PACK32, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_E5B9G9R9_UFLOAT_PACK32}, {"VK_FORMAT_E5B9G9R9_UFLOAT_PACK32", false, true, true, false, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_R16G16B16A16_SFLOAT}, {"VK_FORMAT_R16G16B16A16_SFLOAT", false, true, true, false, VK_FORMAT_R16G16B16A16_SFLOAT, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_D16_UNORM}, {"VK_FORMAT_D16_UNORM", false, true, true, false, VK_FORMAT_D16_UNORM, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_D24_UNORM_S8_UINT}, {"VK_FORMAT_D24_UNORM_S8_UINT", false, true, true, false, VK_FORMAT_D24_UNORM_S8_UINT, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_X8_D24_UNORM_PACK32}, {"VK_FORMAT_X8_D24_UNORM_PACK32", false, true, true, false, VK_FORMAT_X8_D24_UNORM_PACK32, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_S8_UINT}, {"VK_FORMAT_S8_UINT", false, true, true, false, VK_FORMAT_S8_UINT, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_D32_SFLOAT}, {"VK_FORMAT_D32_SFLOAT", false, true, true, false, VK_FORMAT_D32_SFLOAT, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_D32_SFLOAT_S8_UINT}, {"VK_FORMAT_D32_SFLOAT_S8_UINT", false, true, true, false, VK_FORMAT_D32_SFLOAT_S8_UINT, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK}, {"VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK}, {"VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK}, {"VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK}, {"VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK}, {"VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK}, {"VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_EAC_R11_UNORM_BLOCK}, {"VK_FORMAT_EAC_R11_UNORM_BLOCK", false, true, true, true, VK_FORMAT_EAC_R11_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_EAC_R11G11_UNORM_BLOCK}, {"VK_FORMAT_EAC_R11G11_UNORM_BLOCK", false, true, true, true, VK_FORMAT_EAC_R11G11_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_EAC_R11_SNORM_BLOCK}, {"VK_FORMAT_EAC_R11_SNORM_BLOCK", false, true, true, true, VK_FORMAT_EAC_R11_SNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_EAC_R11G11_SNORM_BLOCK}, {"VK_FORMAT_EAC_R11G11_SNORM_BLOCK", false, true, true, true, VK_FORMAT_EAC_R11G11_SNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_ASTC_4x4_UNORM_BLOCK}, {"VK_FORMAT_ASTC_4x4_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_5x4_UNORM_BLOCK}, {"VK_FORMAT_ASTC_5x4_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_5x4_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_5x5_UNORM_BLOCK}, {"VK_FORMAT_ASTC_5x5_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_5x5_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_6x5_UNORM_BLOCK}, {"VK_FORMAT_ASTC_6x5_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_6x5_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_6x6_UNORM_BLOCK}, {"VK_FORMAT_ASTC_6x6_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_6x6_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_8x5_UNORM_BLOCK}, {"VK_FORMAT_ASTC_8x5_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_8x5_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_8x6_UNORM_BLOCK}, {"VK_FORMAT_ASTC_8x6_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_8x6_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_8x8_UNORM_BLOCK}, {"VK_FORMAT_ASTC_8x8_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_8x8_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x5_UNORM_BLOCK}, {"VK_FORMAT_ASTC_10x5_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x5_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x6_UNORM_BLOCK}, {"VK_FORMAT_ASTC_10x6_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x6_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x8_UNORM_BLOCK}, {"VK_FORMAT_ASTC_10x8_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x8_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x10_UNORM_BLOCK}, {"VK_FORMAT_ASTC_10x10_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x10_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_12x10_UNORM_BLOCK}, {"VK_FORMAT_ASTC_12x10_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_12x10_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_12x12_UNORM_BLOCK}, {"VK_FORMAT_ASTC_12x12_UNORM_BLOCK", false, true, true, true, VK_FORMAT_ASTC_12x12_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_ASTC_4x4_SRGB_BLOCK}, {"VK_FORMAT_ASTC_4x4_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_4x4_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_5x4_SRGB_BLOCK}, {"VK_FORMAT_ASTC_5x4_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_5x4_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_5x5_SRGB_BLOCK}, {"VK_FORMAT_ASTC_5x5_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_5x5_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_6x5_SRGB_BLOCK}, {"VK_FORMAT_ASTC_6x5_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_6x5_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_6x6_SRGB_BLOCK}, {"VK_FORMAT_ASTC_6x6_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_6x6_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_8x5_SRGB_BLOCK}, {"VK_FORMAT_ASTC_8x5_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_8x5_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_8x6_SRGB_BLOCK}, {"VK_FORMAT_ASTC_8x6_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_8x6_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_8x8_SRGB_BLOCK}, {"VK_FORMAT_ASTC_8x8_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_8x8_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x5_SRGB_BLOCK}, {"VK_FORMAT_ASTC_10x5_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x5_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x6_SRGB_BLOCK}, {"VK_FORMAT_ASTC_10x6_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x6_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x8_SRGB_BLOCK}, {"VK_FORMAT_ASTC_10x8_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x8_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_10x10_SRGB_BLOCK}, {"VK_FORMAT_ASTC_10x10_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_10x10_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_12x10_SRGB_BLOCK}, {"VK_FORMAT_ASTC_12x10_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_12x10_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_ASTC_12x12_SRGB_BLOCK}, {"VK_FORMAT_ASTC_12x12_SRGB_BLOCK", false, true, true, true, VK_FORMAT_ASTC_12x12_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_BC1_RGBA_UNORM_BLOCK}, {"VK_FORMAT_BC1_RGBA_UNORM_BLOCK", false, true, true, true, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_BC1_RGBA_SRGB_BLOCK}, {"VK_FORMAT_BC1_RGBA_SRGB_BLOCK", false, true, true, true, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_BC2_UNORM_BLOCK}, {"VK_FORMAT_BC2_UNORM_BLOCK", false, true, true, true, VK_FORMAT_BC2_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_BC2_SRGB_BLOCK}, {"VK_FORMAT_BC2_SRGB_BLOCK", false, true, true, true, VK_FORMAT_BC2_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_BC3_UNORM_BLOCK}, {"VK_FORMAT_BC3_UNORM_BLOCK", false, true, true, true, VK_FORMAT_BC3_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_BC3_SRGB_BLOCK}, {"VK_FORMAT_BC3_SRGB_BLOCK", false, true, true, true, VK_FORMAT_BC3_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_BC6H_UFLOAT_BLOCK}, {"VK_FORMAT_BC6H_UFLOAT_BLOCK", false, true, true, true, VK_FORMAT_BC6H_UFLOAT_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_BC6H_SFLOAT_BLOCK}, {"VK_FORMAT_BC6H_SFLOAT_BLOCK", false, true, true, true, VK_FORMAT_BC6H_SFLOAT_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{VK_FORMAT_BC7_UNORM_BLOCK}, {"VK_FORMAT_BC7_UNORM_BLOCK", false, true, true, true, VK_FORMAT_BC7_UNORM_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{VK_FORMAT_BC7_SRGB_BLOCK}, {"VK_FORMAT_BC7_SRGB_BLOCK", false, true, true, true, VK_FORMAT_BC7_SRGB_BLOCK, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
    };
    // clang-format on

    std::string VulkanGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = vkSwapchainTestMap.find(imageFormat);

        if (it != vkSwapchainTestMap.end()) {
            return it->second.imageFormatName;
        }

        return std::string("unknown");
    }

    bool VulkanGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        SwapchainTestMap::const_iterator it = vkSwapchainTestMap.find(imageFormat);

        return (it != vkSwapchainTestMap.end());
    }

    bool VulkanGraphicsPlugin::GetSwapchainCreateTestParameters(XrInstance /*instance*/, XrSession /*session*/, XrSystemId /*systemId*/,
                                                                int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        // Swapchain image format support by the runtime is specified by the xrEnumerateSwapchainFormats function.
        // Runtimes should support R8G8B8A8 and R8G8B8A8 sRGB formats if possible.

        SwapchainTestMap::iterator it = vkSwapchainTestMap.find(imageFormat);

        // Verify that the image format is known. If it's not known then this test needs to be
        // updated to recognize new DXGI formats.
        CAPTURE(imageFormat);
        XRC_CHECK_THROW_MSG(it != vkSwapchainTestMap.end(), "Unknown Vulkan image format.");
        if (it == vkSwapchainTestMap.end()) {
            return false;
        }

        // Verify that imageFormat is not a typeless type. Only regular types are allowed to
        // be returned by the runtime for enumerated image formats. Note Vulkan doesn't really
        // have a "typeless" format to worry about so this should never be hit.
        CAPTURE(it->second.imageFormatName);
        XRC_CHECK_THROW_MSG(!it->second.mutableFormat, "Typeless Vulkan image formats must not be enumerated by runtimes.");
        if (it->second.mutableFormat) {
            return false;
        }

        // We may now proceed with creating swapchains with the format.
        SwapchainCreateTestParameters& tp = it->second;
        tp.arrayCountVector = {1, 2};
        if (tp.colorFormat && !tp.compressedFormat) {
            tp.mipCountVector = {1, 2};
        }
        else {
            tp.mipCountVector = {1};
        }

        *swapchainTestParameters = tp;
        return true;
    }

    bool VulkanGraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* /*tp*/,
                                                       XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
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
        // TODO: check VK_ACCESS_*? Mandate this in the spec?
        return true;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t VulkanGraphicsPlugin::SelectColorSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported color swapchain formats.
        const std::array<VkFormat, 4> f{VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_FORMAT_B8G8R8A8_UNORM};

        const int64_t* formatArrayEnd = formatArray + count;
        auto it = std::find_first_of(formatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
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

        const int64_t* formatArrayEnd = formatArray + count;
        auto it = std::find_first_of(formatArray, formatArrayEnd, f.begin(), f.end());

        if (it == formatArrayEnd) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    int64_t VulkanGraphicsPlugin::GetSRGBA8Format() const
    {
        return VK_FORMAT_R8G8B8A8_SRGB;
    }

    std::shared_ptr<IGraphicsPlugin::SwapchainImageStructs>
    VulkanGraphicsPlugin::AllocateSwapchainImageStructs(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        // What we are doing below is allocating a subclass of the SwapchainImageStructs struct and
        // using shared_ptr to manage it in a way that the caller doesn't need to know about the
        // graphics implementation behind it.

        auto derivedResult = std::make_shared<SwapchainImageContext>();

        // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
        // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
        // Keep the buffer alive by adding it into the list of buffers.

        std::vector<XrSwapchainImageBaseHeader*> bases = derivedResult->Create(
            m_vkDevice, &m_memAllocator, uint32_t(size), swapchainCreateInfo, m_pipelineLayout, m_shaderProgram, m_drawBuffer);

        for (auto& base : bases) {
            // Set the generic vector of base pointers
            derivedResult->imagePtrVector.push_back(base);
            // Map every swapchainImage base pointer to this context
            m_swapchainImageContextMap[base] = derivedResult;
        }

#if defined(USE_MIRROR_WINDOW)
        // Keep these around for mirror rendering
        m_swapchainImageContexts.push_back(derivedResult);
#endif

        // Cast our derived type to the caller-expected type.
        std::shared_ptr<SwapchainImageStructs> result =
            std::static_pointer_cast<SwapchainImageStructs, SwapchainImageContext>(derivedResult);

        return result;
    }

    void VulkanGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImageBase, int64_t imageFormat, uint32_t arraySlice,
                                             const RGBAImage& image)
    {
        const XrSwapchainImageVulkanKHR* swapchainImageVk = reinterpret_cast<const XrSwapchainImageVulkanKHR*>(swapchainImageBase);

        uint32_t w = image.width;
        uint32_t h = image.height;

        // Create a linear staging buffer
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
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
        const size_t rowSize = w * sizeof(RGBA8Color);
        for (size_t row = 0; row < h; ++row) {
            uint8_t* rowPtr = &data[layout.offset + row * layout.rowPitch];
            // Note pixels is a vector<RGBA8Color>
            memcpy(rowPtr, &image.pixels[row * w], rowSize);
        }
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

        // Switch the destination image from UNDEFINED -> TRANSFER_DST_OPTIMAL
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

        // Switch the destination image from TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgBarrier.srcQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.dstQueueFamilyIndex = m_queueFamilyIndex;
        imgBarrier.image = swapchainImageVk->image;
        imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, arraySlice, 1};
        vkCmdPipelineBarrier(m_cmdBuffer.buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &imgBarrier);

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

    void VulkanGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                               int64_t /*colorSwapchainFormat*/)
    {
        auto swapchainContext = m_swapchainImageContextMap[colorSwapchainImage];
        uint32_t imageIndex = swapchainContext->ImageIndex(colorSwapchainImage);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();

        VkRect2D renderArea = {{0, 0}, {swapchainContext->size.width, swapchainContext->size.height}};
        SetViewportAndScissor(renderArea);

        // Ensure depth is in the right layout
        DepthBuffer& depthBuffer = swapchainContext->slice[imageArrayIndex].depthBuffer;
        depthBuffer.TransitionLayout(&m_cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        // Bind eye render target
        VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        swapchainContext->BindRenderTarget(imageIndex, imageArrayIndex, renderArea, &renderPassBeginInfo);

        vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        swapchainContext->BindPipeline(m_cmdBuffer.buf, imageArrayIndex);

        // Clear the buffers
        static XrColor4f darkSlateGrey = {0.184313729f, 0.309803933f, 0.309803933f, 1.0f};
        static std::array<VkClearValue, 2> clearValues;
        clearValues[0].color.float32[0] = darkSlateGrey.r;
        clearValues[0].color.float32[1] = darkSlateGrey.g;
        clearValues[0].color.float32[2] = darkSlateGrey.b;
        clearValues[0].color.float32[3] = darkSlateGrey.a;
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        std::array<VkClearAttachment, 2> clearAttachments{{
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, clearValues[0]},
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0, clearValues[1]},
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

    void VulkanGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                          const XrSwapchainImageBaseHeader* colorSwapchainImage, int64_t /*colorSwapchainFormat*/,
                                          const std::vector<Cube>& cubes)
    {
        auto swapchainContext = m_swapchainImageContextMap[colorSwapchainImage];
        uint32_t imageIndex = swapchainContext->ImageIndex(colorSwapchainImage);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();

        CHECKPOINT();

        const XrRect2Di& r = layerView.subImage.imageRect;
        VkRect2D renderArea = {{r.offset.x, r.offset.y}, {uint32_t(r.extent.width), uint32_t(r.extent.height)}};
        SetViewportAndScissor(renderArea);

        // Just bind the eye render target, ClearImageSlice will have cleared it.
        VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

        swapchainContext->BindRenderTarget(imageIndex, layerView.subImage.imageArrayIndex, renderArea, &renderPassBeginInfo);

        vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        CHECKPOINT();

        swapchainContext->BindPipeline(m_cmdBuffer.buf, layerView.subImage.imageArrayIndex);

        CHECKPOINT();

        // Bind index and vertex buffers
        vkCmdBindIndexBuffer(m_cmdBuffer.buf, m_drawBuffer.idxBuf, 0, VK_INDEX_TYPE_UINT16);

        CHECKPOINT();

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(m_cmdBuffer.buf, 0, 1, &m_drawBuffer.vtxBuf, &offset);

        CHECKPOINT();

        // Compute the view-projection transform.
        // Note all matrixes (including OpenXR's) are column-major, right-handed.
        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrVector3f scale{1.f, 1.f, 1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute the model-view-projection transform and push it.
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            vkCmdPushConstants(m_cmdBuffer.buf, m_pipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp.m), &mvp.m[0]);

            CHECKPOINT();

            // Draw the cube.
            vkCmdDrawIndexed(m_cmdBuffer.buf, m_drawBuffer.count.idx, 1, 0, 0, 0);

            CHECKPOINT();
        }

        vkCmdEndRenderPass(m_cmdBuffer.buf);

        CHECKPOINT();

        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        // XXX Should double-buffer the command buffers, for now just flush
        m_cmdBuffer.Wait();

#if defined(USE_MIRROR_WINDOW)
        // Cycle the window's swapchain on the last view rendered
        if (swapchainContext == &m_swapchainImageContexts.back()) {
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

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<VulkanGraphicsPlugin>(std::move(platformPlugin));
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_VULKAN
