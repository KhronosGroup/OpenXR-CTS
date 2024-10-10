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

#ifdef XR_USE_GRAPHICS_API_METAL

#include "conformance_framework.h"
#include "graphics_plugin.h"
#include "graphics_plugin_impl_helpers.h"
#include "graphics_plugin_metal_gltf.h"
#include "swapchain_image_data.h"

#include "common/xr_dependencies.h"
#include "common/xr_linear.h"
#include "pbr/Metal/MetalResources.h"
#include "pbr/Metal/MetalTexture.h"
#include "utilities/Geometry.h"
#include "utilities/metal_utils.h"
#include "utilities/swapchain_format_data.h"
#include "utilities/swapchain_parameters.h"
#include "utilities/throw_helpers.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr_platform.h>
#include <nonstd/span.hpp>

#include <algorithm>
#include <array>

namespace Conformance
{

    struct MetalMesh
    {
        NS::SharedPtr<MTL::Device> device;
        NS::SharedPtr<MTL::Buffer> vertexBuffer;
        NS::SharedPtr<MTL::Buffer> indexBuffer;
        uint32_t numIndices;

        MetalMesh(NS::SharedPtr<MTL::Device> metalDevice, span<const uint16_t> indices, span<const Geometry::Vertex> vertices)
            : device(metalDevice), numIndices((uint32_t)indices.size())
        {
            struct VertexData
            {
                simd::float4 position;
                simd::float4 color;
            };

            std::vector<VertexData> newVertices;

            for (size_t i = 0; i < vertices.size(); ++i) {
                const auto& d = vertices[i];
                VertexData v{simd_make_float4(d.Position.x, d.Position.y, d.Position.z, 1.0f),
                             simd_make_float4(d.Color.x, d.Color.y, d.Color.z, 1.0f)};
                newVertices.push_back(v);
            }

            vertexBuffer = NS::TransferPtr(device->newBuffer(sizeof(VertexData) * vertices.size(), MTL::ResourceStorageModeManaged));
            indexBuffer = NS::TransferPtr(device->newBuffer(sizeof(uint16_t) * indices.size(), MTL::ResourceStorageModeManaged));

            memcpy(vertexBuffer->contents(), newVertices.data(), vertexBuffer->length());
            memcpy(indexBuffer->contents(), indices.data(), indexBuffer->length());
            vertexBuffer->didModifyRange(NS::Range::Make(0, vertexBuffer->length()));
            indexBuffer->didModifyRange(NS::Range::Make(0, indexBuffer->length()));
        }
    };

    struct MetalFallbackDepthTexture
    {
    public:
        MetalFallbackDepthTexture() = default;

        void Reset()
        {
            m_texture.reset();
            m_xrImage.texture = nullptr;
        }
        bool Allocated() const
        {
            return m_texture.operator bool();
        }

        void Allocate(MTL::Device* metalDevice, uint32_t width, uint32_t height, uint32_t arraySize, uint32_t sampleCount)
        {
            Reset();

            MTL::TextureDescriptor* desc = MTL::TextureDescriptor::texture2DDescriptor(GetDefaultDepthFormat(), width, height, false);
            if (sampleCount > 1) {
                if (arraySize > 1) {
                    desc->setTextureType(MTL::TextureType2DMultisampleArray);
                    desc->setArrayLength(arraySize);
                }
                else {
                    desc->setTextureType(MTL::TextureType2DMultisample);
                }
                desc->setSampleCount(sampleCount);
            }
            else {
                if (arraySize > 1) {
                    desc->setTextureType(MTL::TextureType2DArray);
                    desc->setArrayLength(arraySize);
                }
                else {
                    desc->setTextureType(MTL::TextureType2D);
                }
            }
            desc->setUsage(MTL::TextureUsageRenderTarget);
            desc->setStorageMode(MTL::StorageModePrivate);  // to be compatible with Intel-based Mac
            m_texture = NS::TransferPtr(metalDevice->newTexture(desc));
            XRC_CHECK_THROW(m_texture);
            m_xrImage.texture = m_texture.get();
        }

        const XrSwapchainImageMetalKHR& GetTexture() const
        {
            return m_xrImage;
        }

        static MTL::PixelFormat GetDefaultDepthFormat()
        {
            return MTL::PixelFormatDepth32Float;
        }

    private:
        NS::SharedPtr<MTL::Texture> m_texture{};
        XrSwapchainImageMetalKHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR, NULL, nullptr};
    };

    class MetalSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageMetalKHR>
    {
    public:
        MetalSwapchainImageData(NS::SharedPtr<MTL::Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo,
                                XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo)
            , m_device(device)
        {
        }
        MetalSwapchainImageData(NS::SharedPtr<MTL::Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR, capacity, createInfo)
            , m_device(device)
            , m_internalDepthTextures(capacity)
        {
        }

        void Reset() override
        {
            m_pipelineStateObject.reset();
            m_internalDepthTextures.clear();
            m_device.reset();
            SwapchainImageDataBase::Reset();
        }

        const XrSwapchainImageMetalKHR& GetFallbackDepthSwapchainImage(uint32_t i) override
        {
            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(m_device.get(), this->Width(), this->Height(), this->ArraySize(),
                                                    this->DepthSampleCount());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

        NS::SharedPtr<MTL::RenderPipelineState> GetPipelineStateObject(NS::SharedPtr<MTL::Function> vertexFunction,
                                                                       NS::SharedPtr<MTL::Function> fragmentFunction)
        {
            if (!m_pipelineStateObject || vertexFunction != m_cachedVertexFunction || fragmentFunction != m_cachedFragmentFunction) {
                auto pDesc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
                pDesc->setVertexFunction(vertexFunction.get());
                pDesc->setFragmentFunction(fragmentFunction.get());
                pDesc->colorAttachments()->object(0)->setPixelFormat((MTL::PixelFormat)GetCreateInfo().format);
                pDesc->setDepthAttachmentPixelFormat(GetDepthCreateInfo() ? (MTL::PixelFormat)GetDepthCreateInfo()->format
                                                                          : MetalFallbackDepthTexture::GetDefaultDepthFormat());

                NS::Error* pError = nullptr;
                m_pipelineStateObject = NS::TransferPtr(m_device->newRenderPipelineState(pDesc.get(), &pError));
                XRC_CHECK_THROW(m_pipelineStateObject);
                m_cachedVertexFunction = vertexFunction;
                m_cachedFragmentFunction = fragmentFunction;
            }
            return m_pipelineStateObject;
        }

    private:
        NS::SharedPtr<MTL::Device> m_device;
        std::vector<MetalFallbackDepthTexture> m_internalDepthTextures;
        NS::SharedPtr<MTL::Function> m_cachedVertexFunction;
        NS::SharedPtr<MTL::Function> m_cachedFragmentFunction;
        NS::SharedPtr<MTL::RenderPipelineState> m_pipelineStateObject;
    };

    struct MetalGraphicsPlugin : public IGraphicsPlugin
    {
        MetalGraphicsPlugin(std::shared_ptr<IPlatformPlugin>);

        ~MetalGraphicsPlugin() override;

        bool Initialize() override;

        bool IsInitialized() const override;

        void Shutdown() override;

        std::string DescribeGraphics() const override;

        std::vector<std::string> GetInstanceExtensions() const override;

        bool InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                              uint32_t deviceCreationFlags) override;

        void ClearSwapchainCache() override;

        void ShutdownDevice() override;

        const XrBaseInStructure* GetGraphicsBinding() const override;

        void CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image) override;

        std::string GetImageFormatName(int64_t imageFormat) const override;

        bool IsImageFormatKnown(int64_t imageFormat) const override;

        bool GetSwapchainCreateTestParameters(int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters) override;

        bool ValidateSwapchainImages(int64_t imageFormat, const SwapchainCreateTestParameters* tp, XrSwapchain swapchain,
                                     uint32_t* imageCount) const override;

        bool ValidateSwapchainImageState(XrSwapchain swapchain, uint32_t index, int64_t imageFormat) const override;

        int64_t SelectColorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectDepthSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        int64_t SelectMotionVectorSwapchainFormat(const int64_t* imageFormatArray, size_t count) const override;

        // Format required by RGBAImage type.
        int64_t GetSRGBA8Format() const override;

        ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override;

        ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(size_t size,
                                                                          const XrSwapchainCreateInfo& colorSwapchainCreateInfo,
                                                                          XrSwapchain depthSwapchain,
                                                                          const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override;

        void ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex, XrColor4f color) override;

        MeshHandle MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx) override;

        GLTFModelHandle LoadGLTF(Gltf::ModelBuilder&& modelBuilder) override;
        std::shared_ptr<Pbr::Model> GetPbrModel(GLTFModelHandle handle) const override;
        GLTFModelInstanceHandle CreateGLTFModelInstance(GLTFModelHandle handle) override;
        Pbr::ModelInstance& GetModelInstance(GLTFModelInstanceHandle handle) override;

        void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* colorSwapchainImage,
                        const RenderParams& params) override;

    private:
        bool m_initialized{false};
        XrGraphicsBindingMetalKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_METAL_KHR};
        NS::SharedPtr<MTL::Device> m_device;
        NS::SharedPtr<MTL::CommandQueue> m_commandQueue;

        NS::SharedPtr<MTL::Library> m_library;
        NS::SharedPtr<MTL::Function> m_vertexFunction;
        NS::SharedPtr<MTL::Function> m_fragmentFunction;
        NS::SharedPtr<MTL::DepthStencilState> m_depthStencilState;

        SwapchainImageDataMap<MetalSwapchainImageData> m_swapchainImageDataMap;

        bool InitializeResources();
        void DestroyResources();

        typedef std::map<int64_t, SwapchainCreateTestParameters> SwapchainTestMap;
        const SwapchainTestMap& GetMetalSwapchainTestMap() const;

        MeshHandle m_cubeMesh;
        VectorWithGenerationCountedHandles<MetalMesh, MeshHandle> m_meshes;
        // This is fine to be a shared_ptr because Model doesn't directly hold any graphics state.
        VectorWithGenerationCountedHandles<std::shared_ptr<Pbr::Model>, GLTFModelHandle> m_gltfModels;
        VectorWithGenerationCountedHandles<MetalGLTF, GLTFModelInstanceHandle> m_gltfInstances;

        std::unique_ptr<Pbr::MetalResources> pbrResources;

        NS::SharedPtr<MTL::Texture> GetColorSliceTexture(MetalSwapchainImageData& swapchainData, uint32_t imageIndex,
                                                         uint32_t imageArrayIndex) const;
        NS::SharedPtr<MTL::Texture> GetDepthSliceTexture(MetalSwapchainImageData& swapchainData, uint32_t imageIndex,
                                                         uint32_t imageArrayIndex) const;
    };

    MetalGraphicsPlugin::MetalGraphicsPlugin(std::shared_ptr<IPlatformPlugin>)
    {
    }

    MetalGraphicsPlugin::~MetalGraphicsPlugin()
    {
        ShutdownDevice();
        Shutdown();
    }

    bool MetalGraphicsPlugin::Initialize()
    {
        if (m_initialized) {
            return false;
        }
        m_initialized = true;
        return true;
    }

    bool MetalGraphicsPlugin::IsInitialized() const
    {
        return m_initialized;
    }

    void MetalGraphicsPlugin::Shutdown()
    {
        if (m_initialized) {
            m_initialized = false;
        }
    }

    std::string MetalGraphicsPlugin::DescribeGraphics() const
    {
        return "Metal";
    }

    std::vector<std::string> MetalGraphicsPlugin::GetInstanceExtensions() const
    {
        return {XR_KHR_METAL_ENABLE_EXTENSION_NAME};
    }

    bool MetalGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId systemId, bool checkGraphicsRequirements,
                                               uint32_t /*deviceCreationFlags*/)
    {
        try {
            XrGraphicsRequirementsMetalKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_METAL_KHR};

            if (checkGraphicsRequirements) {

                auto xrGetMetalGraphicsRequirementsKHR =
                    GetInstanceExtensionFunction<PFN_xrGetMetalGraphicsRequirementsKHR>(instance, "xrGetMetalGraphicsRequirementsKHR");

                XrResult result = xrGetMetalGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
                CHECK(ValidateResultAllowed("xrGetMetalGraphicsRequirementsKHR", result));
                if (XR_FAILED(result)) {
                    return false;
                }
            }

            m_device.reset();
            m_commandQueue.reset();

            if (checkGraphicsRequirements) {
                m_device = NS::TransferPtr((MTL::Device*)graphicsRequirements.metalDevice);
                if (!m_device) {
                    return false;
                }
            }

            if (m_device) {
                m_commandQueue = NS::TransferPtr(m_device->newCommandQueue());
            }

            m_graphicsBinding.commandQueue = m_commandQueue.get();

            InitializeResources();
        }
        catch (...) {
            // Log it?
        }
        return true;
    }

    void MetalGraphicsPlugin::ClearSwapchainCache()
    {
        m_swapchainImageDataMap.Reset();
    }

    void MetalGraphicsPlugin::ShutdownDevice()
    {
        m_graphicsBinding = XrGraphicsBindingMetalKHR{XR_TYPE_GRAPHICS_BINDING_METAL_KHR};

        DestroyResources();

        m_swapchainImageDataMap.Reset();

        m_commandQueue.reset();
        m_device.reset();
    }

    const XrBaseInStructure* MetalGraphicsPlugin::GetGraphicsBinding() const
    {
        if (m_graphicsBinding.commandQueue) {
            return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
        }
        return nullptr;
    }

    void MetalGraphicsPlugin::CopyRGBAImage(const XrSwapchainImageBaseHeader* swapchainImage, uint32_t arraySlice, const RGBAImage& image)
    {
        MTL::Texture* texture = (MTL::Texture*)(reinterpret_cast<const XrSwapchainImageMetalKHR*>(swapchainImage)->texture);
        MTL::Region region(0, 0, image.width, image.height);
        NS::SharedPtr<MTL::Buffer> buffer = NS::TransferPtr(m_device->newBuffer(
            image.pixels.data(), image.width * image.height * sizeof(uint32_t), MTL::ResourceOptionCPUCacheModeDefault));
        MTL::CommandBuffer* pCmd = m_commandQueue->commandBuffer();
        MTL::BlitCommandEncoder* pBlitEncoder = pCmd->blitCommandEncoder();
        pBlitEncoder->setLabel(MTLSTR("BlitCommandEncoder_CopyRGBAImage"));
        pBlitEncoder->copyFromBuffer(buffer.get(), 0, image.width * sizeof(uint32_t), 0, region.size, texture, arraySlice, 0,
                                     region.origin);
        pBlitEncoder->endEncoding();
        pCmd->commit();
        pCmd->waitUntilCompleted();
    }

    static const SwapchainFormatDataMap& GetSwapchainFormatData()
    {

        // Add SwapchainCreateTestParameters for other Vulkan formats if they are supported by a runtime
        static SwapchainFormatDataMap map{{

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA8Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA8Unorm_sRGB).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBGRA8Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBGRA8Unorm_sRGB).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG8Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG8Unorm_sRGB).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR8Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR8Unorm_sRGB).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR8Snorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG8Snorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA8Snorm).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR8Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG8Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA8Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR8Sint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG8Sint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA8Sint).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR16Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG16Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA16Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR16Snorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG16Snorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA16Snorm).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR16Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG16Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA16Uint).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR16Sint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG16Sint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA16Sint).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR16Float).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG16Float).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA16Float).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR32Sint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG32Sint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA32Sint).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR32Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG32Uint).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA32Uint).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatR32Float).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG32Float).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA32Float).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatB5G6R5Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatA1BGR5Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBGR5A1Unorm).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatABGR4Unorm).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGB10A2Unorm).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBGR10A2Unorm).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGB10A2Uint).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRG11B10Float).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGB9E5Float).ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatRGBA16Float).ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatDepth16Unorm).Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatDepth24Unorm_Stencil8).DepthStencil().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatDepth32Float).Depth().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatDepth32Float_Stencil8).DepthStencil().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatETC2_RGB8).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatETC2_RGB8A1).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatETC2_RGB8_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatETC2_RGB8A1_sRGB).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatEAC_R11Unorm).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatEAC_RG11Unorm).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatEAC_R11Snorm).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatEAC_RG11Snorm).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatEAC_RGBA8).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatEAC_RGBA8_sRGB).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_4x4_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_5x4_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_5x5_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_6x5_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_6x6_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x5_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x6_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x8_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x5_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x6_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x10_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x10_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_12x10_sRGB).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_12x12_sRGB).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_4x4_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_5x4_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_5x5_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_6x5_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_6x6_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x5_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x6_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x8_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x5_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x6_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x10_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x10_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_12x10_LDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_12x12_LDR).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_4x4_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_5x4_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_5x5_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_6x5_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_6x6_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x5_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x6_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_8x8_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x5_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x6_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x10_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_10x10_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_12x10_HDR).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatASTC_12x12_HDR).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC1_RGBA).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC1_RGBA_sRGB).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC2_RGBA).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC2_RGBA_sRGB).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC3_RGBA).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC3_RGBA_sRGB).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC6H_RGBFloat).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC6H_RGBUfloat).Compressed().ToPair(),

            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC7_RGBAUnorm).Compressed().ToPair(),
            XRC_SWAPCHAIN_FORMAT(MTL::PixelFormatBC7_RGBAUnorm_sRGB).Compressed().ToPair(),
        }};
        return map;
    }

    std::string MetalGraphicsPlugin::GetImageFormatName(int64_t imageFormat) const
    {
        return ::Conformance::GetImageFormatName(GetSwapchainFormatData(), imageFormat);
    }

    bool MetalGraphicsPlugin::IsImageFormatKnown(int64_t imageFormat) const
    {
        return ::Conformance::IsImageFormatKnown(GetSwapchainFormatData(), imageFormat);
    }

    bool MetalGraphicsPlugin::GetSwapchainCreateTestParameters(int64_t imageFormat, SwapchainCreateTestParameters* swapchainTestParameters)
    {
        *swapchainTestParameters = ::Conformance::GetSwapchainCreateTestParameters(GetSwapchainFormatData(), imageFormat);
        return true;
    }

    bool MetalGraphicsPlugin::ValidateSwapchainImages(int64_t /*imageFormat*/, const SwapchainCreateTestParameters* /*tp*/,
                                                      XrSwapchain swapchain, uint32_t* imageCount) const noexcept(false)
    {
        *imageCount = 0;  // Zero until set below upon success.

        std::vector<XrSwapchainImageMetalKHR> swapchainImageVector;
        uint32_t countOutput;

        XrResult result = xrEnumerateSwapchainImages(swapchain, 0, &countOutput, nullptr);
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput > 0);

        swapchainImageVector.resize(countOutput, XrSwapchainImageMetalKHR{XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR});

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
        swapchainImageVector.resize(countOutput, XrSwapchainImageMetalKHR{XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR});
        result = xrEnumerateSwapchainImages(swapchain, countOutput, &countOutput,
                                            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImageVector.data()));
        CHECK(ValidateResultAllowed("xrEnumerateSwapchainImages", result));
        REQUIRE(result == XR_SUCCESS);
        REQUIRE(countOutput == swapchainImageVector.size());
        REQUIRE(ValidateStructVectorType(swapchainImageVector, XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR));

        for (const XrSwapchainImageMetalKHR& image : swapchainImageVector) {
            // Verify that the image is a valid handle
            CHECK(image.texture != nullptr);
        }

        *imageCount = countOutput;
        return true;
    }

    bool MetalGraphicsPlugin::ValidateSwapchainImageState(XrSwapchain /*swapchain*/, uint32_t /*index*/, int64_t /*imageFormat*/) const
    {
        return true;
    }

    int64_t MetalGraphicsPlugin::SelectColorSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported color swapchain formats.
        const std::array<MTL::PixelFormat, 4> f{MTL::PixelFormatRGBA8Unorm_sRGB, MTL::PixelFormatBGRA8Unorm_sRGB,
                                                MTL::PixelFormatRGBA8Unorm, MTL::PixelFormatBGRA8Unorm};

        span<const int64_t> formatArraySpan{formatArray, count};
        auto it = std::find_first_of(formatArraySpan.begin(), formatArraySpan.end(), f.begin(), f.end());

        if (it == formatArraySpan.end()) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    int64_t MetalGraphicsPlugin::SelectDepthSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of supported depth swapchain formats.
        const std::array<MTL::PixelFormat, 4> f{MTL::PixelFormatDepth32Float, MTL::PixelFormatDepth24Unorm_Stencil8,
                                                MTL::PixelFormatDepth16Unorm, MTL::PixelFormatDepth32Float_Stencil8};

        span<const int64_t> formatArraySpan{formatArray, count};
        auto it = std::find_first_of(formatArraySpan.begin(), formatArraySpan.end(), f.begin(), f.end());

        if (it == formatArraySpan.end()) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t MetalGraphicsPlugin::SelectMotionVectorSwapchainFormat(const int64_t* formatArray, size_t count) const
    {
        // List of swapchain formats suitable for motion vectors.
        const std::array<MTL::PixelFormat, 2> f{MTL::PixelFormatRGBA16Float, MTL::PixelFormatRGBA32Float};

        span<const int64_t> formatArraySpan{formatArray, count};
        auto it = std::find_first_of(formatArraySpan.begin(), formatArraySpan.end(), f.begin(), f.end());

        if (it == formatArraySpan.end()) {
            assert(false);  // Assert instead of throw as we need to switch to the big table which can't fail.
            return formatArray[0];
        }

        return *it;
    }

    int64_t MetalGraphicsPlugin::GetSRGBA8Format() const
    {
        return MTL::PixelFormatRGBA8Unorm_sRGB;
    }

    ISwapchainImageData* MetalGraphicsPlugin::AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo)
    {
        auto typedResult = std::make_unique<MetalSwapchainImageData>(m_device, uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    ISwapchainImageData* MetalGraphicsPlugin::AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo)
    {

        auto typedResult = std::make_unique<MetalSwapchainImageData>(m_device, uint32_t(size), colorSwapchainCreateInfo, depthSwapchain,
                                                                     depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    NS::SharedPtr<MTL::Texture> MetalGraphicsPlugin::GetColorSliceTexture(MetalSwapchainImageData& swapchainData, uint32_t imageIndex,
                                                                          uint32_t imageArrayIndex) const
    {
        MTL::Texture* texture = (MTL::Texture*)swapchainData.GetTypedImage(imageIndex).texture;
        MTL::PixelFormat textureViewFormat = (MTL::PixelFormat)swapchainData.GetCreateInfo().format;
        MTL::TextureType textureViewType = (swapchainData.SampleCount() > 1) ? MTL::TextureType2DMultisample : MTL::TextureType2D;
        NS::Range textureViewMipRange = NS::Range::Make(0, 1);
        NS::Range textureViewArrayRange = NS::Range::Make(imageArrayIndex, 1);
        MTL::Texture* textureView = texture->newTextureView(textureViewFormat, textureViewType, textureViewMipRange, textureViewArrayRange);
        textureView->setLabel(MTLSTR("ColorSliceTexture"));
        return NS::TransferPtr(textureView);
    }

    NS::SharedPtr<MTL::Texture> MetalGraphicsPlugin::GetDepthSliceTexture(MetalSwapchainImageData& swapchainData, uint32_t imageIndex,
                                                                          uint32_t imageArrayIndex) const
    {
        // Clear depth buffer.
        MTL::Texture* texture = (MTL::Texture*)swapchainData.GetDepthImageForColorIndex(imageIndex).texture;
        const XrSwapchainCreateInfo* depthCreateInfo = swapchainData.GetDepthCreateInfo();
        MTL::PixelFormat depthSwapchainFormat =
            depthCreateInfo != nullptr ? (MTL::PixelFormat)depthCreateInfo->format : MetalFallbackDepthTexture::GetDefaultDepthFormat();
        MTL::TextureType textureViewType = (swapchainData.DepthSampleCount() > 1) ? MTL::TextureType2DMultisample : MTL::TextureType2D;
        NS::Range textureViewMipRange = NS::Range::Make(0, 1);
        NS::Range textureViewArrayRange = NS::Range::Make(imageArrayIndex, 1);
        MTL::Texture* textureView =
            texture->newTextureView(depthSwapchainFormat, textureViewType, textureViewMipRange, textureViewArrayRange);
        textureView->setLabel(MTLSTR("DepthSliceTexture"));
        return NS::TransferPtr(textureView);
    }

    void MetalGraphicsPlugin::ClearImageSlice(const XrSwapchainImageBaseHeader* colorSwapchainImage, uint32_t imageArrayIndex,
                                              XrColor4f color)
    {
        auto pAutoReleasePool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

        MetalSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        NS::SharedPtr<MTL::Texture> colorTexture = GetColorSliceTexture(*swapchainData, imageIndex, imageArrayIndex);
        NS::SharedPtr<MTL::Texture> depthTexture = GetDepthSliceTexture(*swapchainData, imageIndex, imageArrayIndex);

        MTL::CommandBuffer* pCmd = m_commandQueue->commandBuffer();

        auto renderPassDesc = NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
        renderPassDesc->colorAttachments()->object(0)->setTexture(colorTexture.get());
        renderPassDesc->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(color.r, color.g, color.b, color.a));
        renderPassDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
        renderPassDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        renderPassDesc->depthAttachment()->setTexture(depthTexture.get());
        renderPassDesc->depthAttachment()->setClearDepth(
            1.0f);  // depthDirection is not considered (same as the other graphics plugins), which could be a glitch.
        renderPassDesc->depthAttachment()->setLoadAction(MTL::LoadActionClear);
        renderPassDesc->depthAttachment()->setStoreAction(MTL::StoreActionStore);

        MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(renderPassDesc.get());
        pEnc->setLabel(MTLSTR("ClearImageSlice"));
        pEnc->endEncoding();
        pCmd->commit();
    }

    MeshHandle MetalGraphicsPlugin::MakeSimpleMesh(span<const uint16_t> idx, span<const Geometry::Vertex> vtx)
    {
        auto handle = m_meshes.emplace_back(m_device, idx, vtx);
        return handle;
    }

    GLTFModelHandle MetalGraphicsPlugin::LoadGLTF(Gltf::ModelBuilder&& modelBuilder)
    {
        auto handle = m_gltfModels.emplace_back(modelBuilder.Build(*pbrResources));
        return handle;
    }

    std::shared_ptr<Pbr::Model> MetalGraphicsPlugin::GetPbrModel(GLTFModelHandle handle) const
    {
        return m_gltfModels[handle];
    }

    GLTFModelInstanceHandle MetalGraphicsPlugin::CreateGLTFModelInstance(GLTFModelHandle handle)
    {
        auto pbrModelInstance = Pbr::MetalModelInstance(*pbrResources, GetPbrModel(handle));
        auto instanceHandle = m_gltfInstances.emplace_back(std::move(pbrModelInstance));
        return instanceHandle;
    }

    Pbr::ModelInstance& MetalGraphicsPlugin::GetModelInstance(GLTFModelInstanceHandle handle)
    {
        return m_gltfInstances[handle].GetModelInstance();
    }

    void MetalGraphicsPlugin::RenderView(const XrCompositionLayerProjectionView& layerView,
                                         const XrSwapchainImageBaseHeader* colorSwapchainImage, const RenderParams& params)
    {
        auto pAutoReleasePool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

        MetalSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(colorSwapchainImage);

        NS::SharedPtr<MTL::Texture> colorTexture = GetColorSliceTexture(*swapchainData, imageIndex, layerView.subImage.imageArrayIndex);
        NS::SharedPtr<MTL::Texture> depthTexture = GetDepthSliceTexture(*swapchainData, imageIndex, layerView.subImage.imageArrayIndex);

        MTL::CommandBuffer* pCmd = m_commandQueue->commandBuffer();

        auto renderPassDesc = NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
        renderPassDesc->colorAttachments()->object(0)->setTexture(colorTexture.get());
        renderPassDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionLoad);  // no clear
        renderPassDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        renderPassDesc->depthAttachment()->setTexture(depthTexture.get());
        renderPassDesc->depthAttachment()->setLoadAction(MTL::LoadActionLoad);  // no clear
        renderPassDesc->depthAttachment()->setStoreAction(MTL::StoreActionStore);

        MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(renderPassDesc.get());
        pEnc->setLabel(MTLSTR("RenderView"));

        MTL::Viewport viewport{(double)layerView.subImage.imageRect.offset.x,
                               (double)layerView.subImage.imageRect.offset.y,
                               (double)layerView.subImage.imageRect.extent.width,
                               (double)layerView.subImage.imageRect.extent.height,
                               0.0,
                               1.0};
        pEnc->setViewport(viewport);
        pEnc->setDepthStencilState(m_depthStencilState.get());
        pEnc->setCullMode(MTL::CullModeBack);

        pEnc->setRenderPipelineState(swapchainData->GetPipelineStateObject(m_vertexFunction, m_fragmentFunction).get());

        // Compute the view-projection transform.
        // Note all matrixes are column-major, right-handed.
        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_METAL, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrVector3f scale{1.f, 1.f, 1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        static_assert(sizeof(XrMatrix4x4f) == sizeof(simd::float4x4), "Unexpected matrix size");

        if (params.cubes.size() > 0 || params.meshes.size() > 0) {
            pEnc->pushDebugGroup(MTLSTR("CubesAndMeshes"));

            MeshHandle lastMeshHandle;

            const auto drawMesh = [this, &pEnc, &vp, &lastMeshHandle](const MeshDrawable mesh) {
                MetalMesh& metalMesh = m_meshes[mesh.handle];

                if (mesh.handle != lastMeshHandle) {
                    pEnc->setVertexBuffer(metalMesh.vertexBuffer.get(), 0, 0);
                    lastMeshHandle = mesh.handle;
                }

                XrMatrix4x4f model;
                XrMatrix4x4f_CreateTranslationRotationScale(&model, &mesh.params.pose.position, &mesh.params.pose.orientation,
                                                            &mesh.params.scale);
                XrMatrix4x4f mvp;
                XrMatrix4x4f_Multiply(&mvp, &vp, &model);

                pEnc->setVertexBytes(&mvp, sizeof(simd::float4x4), 1);
                pEnc->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, metalMesh.numIndices, MTL::IndexTypeUInt16,
                                            metalMesh.indexBuffer.get(), 0);
            };

            // Render each cube
            for (const Cube& cube : params.cubes) {
                drawMesh(MeshDrawable{m_cubeMesh, cube.params.pose, cube.params.scale});
            }

            // Render each mesh
            for (const auto& mesh : params.meshes) {
                drawMesh(mesh);
            }

            pEnc->popDebugGroup();
        }

        // Render each gltf
        pEnc->pushDebugGroup(MTLSTR("glTFs"));
        for (const auto& gltfDrawable : params.glTFs) {
            MetalGLTF& gltf = m_gltfInstances[gltfDrawable.handle];
            // Compute and update the model transform.

            XrMatrix4x4f modelToWorld;
            XrMatrix4x4f_CreateTranslationRotationScale(&modelToWorld, &gltfDrawable.params.pose.position,
                                                        &gltfDrawable.params.pose.orientation, &gltfDrawable.params.scale);

            pbrResources->SetViewProjection(view, proj);

            MTL::PixelFormat colorFormat = (MTL::PixelFormat)swapchainData->GetCreateInfo().format;
            MTL::PixelFormat depthFormat = swapchainData->GetDepthCreateInfo()
                                               ? (MTL::PixelFormat)swapchainData->GetDepthCreateInfo()->format
                                               : MetalFallbackDepthTexture::GetDefaultDepthFormat();

            gltf.Render(pEnc, *pbrResources, modelToWorld, colorFormat, depthFormat);
        }
        pEnc->popDebugGroup();

        pEnc->endEncoding();
        pCmd->commit();
    }

    // Private methods

    bool MetalGraphicsPlugin::InitializeResources()
    {
        using NS::StringEncoding::UTF8StringEncoding;

        const char* shaderSrc = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct VertexBuffer {
                float4 position;
                float4 color;
            };

            struct v2f
            {
                float4 position [[position]];
                half4 color;
            };

            v2f vertex vertexMain( uint vertexId [[vertex_id]],
                                   uint instanceId [[instance_id]],
                                   device const VertexBuffer* vertexBuffer [[buffer(0)]],
                                   device const float4x4* matricesBuffer [[buffer(1)]] )
            {
                v2f o;
                float4 pos = vertexBuffer[vertexId].position;
                o.position = matricesBuffer[instanceId] * pos;
                o.color = half4(vertexBuffer[vertexId].color);
                return o;
            }

            half4 fragment fragmentMain( v2f in [[stage_in]] )
            {
                return in.color;
            }
        )";

        NS::Error* pError = nullptr;
        m_library = NS::TransferPtr(m_device->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError));
        if (!m_library) {
            return false;
        }

        m_vertexFunction = NS::TransferPtr(m_library->newFunction(NS::String::string("vertexMain", UTF8StringEncoding)));
        m_fragmentFunction = NS::TransferPtr(m_library->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding)));

        auto depthDescriptor = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
        depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLessEqual);
        depthDescriptor->setDepthWriteEnabled(true);
        m_depthStencilState = NS::TransferPtr(m_device->newDepthStencilState(depthDescriptor.get()));

        m_cubeMesh = MakeCubeMesh();

        pbrResources = std::make_unique<Pbr::MetalResources>(m_device.get());
        pbrResources->SetLight({0.0f, 0.7071067811865475f, 0.7071067811865475f}, Pbr::RGB::White);

        auto blackCubeMap =
            Pbr::MetalTexture::CreateFlatCubeTexture(*pbrResources, Pbr::RGBA::Black, MTL::PixelFormatRGBA8Unorm, MTLSTR("blackCubeMap"));
        pbrResources->SetEnvironmentMap(blackCubeMap.get(), blackCubeMap.get());

        std::vector<uint8_t> brdfLutFileData = ReadFileBytes("brdf_lut.png");
        NS::SharedPtr<MTL::Texture> brdfLutTexture = Pbr::MetalTexture::LoadTextureImage(
            *pbrResources, false, brdfLutFileData.data(), (uint32_t)brdfLutFileData.size(), MTLSTR("brdf_lut.png"));
        pbrResources->SetBrdfLut(brdfLutTexture.get());

        return true;
    }

    void MetalGraphicsPlugin::DestroyResources()
    {
        m_cubeMesh = {};
        m_meshes.clear();
        m_gltfInstances.clear();
        m_gltfModels.clear();
        pbrResources.reset();

        m_depthStencilState.reset();
        m_vertexFunction.reset();
        m_fragmentFunction.reset();
        m_library.reset();
    }

    std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Metal(std::shared_ptr<IPlatformPlugin> platformPlugin)
    {
        return std::make_shared<MetalGraphicsPlugin>(platformPlugin);
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_METAL
