// Copyright (c) 2019-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "conformance_framework.h"
#include "controller_animation_handler.h"
#include "graphics_plugin.h"
#include "utilities/types_and_constants.h"
#include <nonstd/span.hpp>
#include <openxr/openxr.h>

namespace Conformance
{
    using nonstd::span;
    struct DispatchTable_EXT_render_model
    {
        XrInstance instance;

        PFN_xrCreateRenderModelEXT xrCreateRenderModelEXT_;
        PFN_xrDestroyRenderModelEXT xrDestroyRenderModelEXT_;
        PFN_xrGetRenderModelPropertiesEXT xrGetRenderModelPropertiesEXT_;
        PFN_xrCreateRenderModelSpaceEXT xrCreateRenderModelSpaceEXT_;
        PFN_xrCreateRenderModelAssetEXT xrCreateRenderModelAssetEXT_;
        PFN_xrDestroyRenderModelAssetEXT xrDestroyRenderModelAssetEXT_;
        PFN_xrGetRenderModelAssetDataEXT xrGetRenderModelAssetDataEXT_;
        PFN_xrGetRenderModelAssetPropertiesEXT xrGetRenderModelAssetPropertiesEXT_;
        PFN_xrGetRenderModelStateEXT xrGetRenderModelStateEXT_;

        explicit DispatchTable_EXT_render_model(XrInstance instance_)
            : instance(instance_)
            , xrCreateRenderModelEXT_(GetInstanceExtensionFunction<PFN_xrCreateRenderModelEXT, true>(instance, "xrCreateRenderModelEXT"))
            , xrDestroyRenderModelEXT_(GetInstanceExtensionFunction<PFN_xrDestroyRenderModelEXT, true>(instance, "xrDestroyRenderModelEXT"))
            , xrGetRenderModelPropertiesEXT_(
                  GetInstanceExtensionFunction<PFN_xrGetRenderModelPropertiesEXT, true>(instance, "xrGetRenderModelPropertiesEXT"))
            , xrCreateRenderModelSpaceEXT_(
                  GetInstanceExtensionFunction<PFN_xrCreateRenderModelSpaceEXT, true>(instance, "xrCreateRenderModelSpaceEXT"))
            , xrCreateRenderModelAssetEXT_(
                  GetInstanceExtensionFunction<PFN_xrCreateRenderModelAssetEXT, true>(instance, "xrCreateRenderModelAssetEXT"))
            , xrDestroyRenderModelAssetEXT_(
                  GetInstanceExtensionFunction<PFN_xrDestroyRenderModelAssetEXT, true>(instance, "xrDestroyRenderModelAssetEXT"))
            , xrGetRenderModelAssetDataEXT_(
                  GetInstanceExtensionFunction<PFN_xrGetRenderModelAssetDataEXT, true>(instance, "xrGetRenderModelAssetDataEXT"))
            , xrGetRenderModelAssetPropertiesEXT_(GetInstanceExtensionFunction<PFN_xrGetRenderModelAssetPropertiesEXT, true>(
                  instance, "xrGetRenderModelAssetPropertiesEXT"))
            , xrGetRenderModelStateEXT_(
                  GetInstanceExtensionFunction<PFN_xrGetRenderModelStateEXT, true>(instance, "xrGetRenderModelStateEXT"))
        {
        }
    };

    namespace deleters
    {

        class RenderModelEXTDelete
        {
        public:
            typedef XrRenderModelEXT pointer;
            RenderModelEXTDelete(PFN_xrDestroyRenderModelEXT destroyRenderModel) : xrDestroyRenderModelEXT_(destroyRenderModel)
            {
            }

            void operator()(XrRenderModelEXT s) const;

        private:
            PFN_xrDestroyRenderModelEXT xrDestroyRenderModelEXT_;
        };

        class RenderModelAssetEXTDelete
        {
        public:
            typedef XrRenderModelAssetEXT pointer;
            RenderModelAssetEXTDelete(PFN_xrDestroyRenderModelAssetEXT destroyRenderModelAsset)
                : xrDestroyRenderModelAssetEXT_(destroyRenderModelAsset)
            {
            }

            void operator()(XrRenderModelAssetEXT s) const;

        private:
            PFN_xrDestroyRenderModelAssetEXT xrDestroyRenderModelAssetEXT_;
        };
    }  // namespace deleters

    using RenderModelEXTScoped = ScopedHandle<XrRenderModelEXT, deleters::RenderModelEXTDelete>;
    using RenderModelAssetEXTScoped = ScopedHandle<XrRenderModelAssetEXT, deleters::RenderModelAssetEXTDelete>;

    struct RenderModelData
    {
        RenderModelEXTScoped rm;
        GLTFModelHandle glTFModelHandle;
        GLTFModelInstanceHandle glTFModelInstanceHandle;
        RenderModelAnimationHandler animationHandler;
    };

    RenderModelData testRenderModel(const DispatchTable_EXT_render_model& ext, XrSession session, XrRenderModelIdEXT rmid,
                                    bool testNoExtensions, span<const char*> extensions);
}  // namespace Conformance
