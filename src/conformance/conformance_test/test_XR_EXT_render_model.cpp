// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "action_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "controller_animation_handler.h"
#include "ext_render_model.h"
#include "report.h"
#include "two_call.h"
#include "two_call_struct_tests.h"

#include "utilities/types_and_constants.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace Conformance
{
    TEST_CASE("XR_EXT_render_model-simple", "[XR_EXT_render_model]")
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsInstanceExtensionSupported(XR_EXT_RENDER_MODEL_EXTENSION_NAME)) {
            SKIP(XR_EXT_RENDER_MODEL_EXTENSION_NAME " not supported");
        }

        AutoBasicInstance instance({XR_EXT_UUID_EXTENSION_NAME, XR_EXT_RENDER_MODEL_EXTENSION_NAME});
        AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);
        DispatchTable_EXT_render_model ext(instance);

        SECTION("InvalidRenderModelID")
        {
            XrRenderModelCreateInfoEXT createInfo{XR_TYPE_RENDER_MODEL_CREATE_INFO_EXT};
            createInfo.renderModelId = XR_NULL_RENDER_MODEL_ID_EXT;
            {
                INFO("Null render model ID");
                CAPTURE(createInfo.renderModelId);
                XrRenderModelEXT rm = XR_NULL_HANDLE;
                REQUIRE(XR_ERROR_RENDER_MODEL_ID_INVALID_EXT == ext.xrCreateRenderModelEXT_(session, &createInfo, &rm));
            }

            createInfo.renderModelId = XRC_INVALID_RENDER_MODEL_ID_EXT_VALUE;
            {
                INFO("Invalid render model ID");
                CAPTURE(createInfo.renderModelId);
                XrRenderModelEXT rm = XR_NULL_HANDLE;
                REQUIRE(XR_ERROR_RENDER_MODEL_ID_INVALID_EXT == ext.xrCreateRenderModelEXT_(session, &createInfo, &rm));
            }
        }

        // TODO: If the application passes a UUID not retrieved in this way (for example, passing
        // a UUID received from a previous session), the runtime must: return
        // ename:XR_ERROR_VALIDATION_FAILURE.
        // SECTION("Invalid UUID")
        // {
        // }
    }
}  // namespace Conformance
