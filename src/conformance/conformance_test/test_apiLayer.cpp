// Copyright (c) 2019-2025 The Khronos Group Inc.
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

#include "conformance_framework.h"
#include "conformance_options.h"
#include "conformance_utils.h"
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define LAYER_EXPORT __attribute__((visibility("default")))
#else
#define LAYER_EXPORT
#endif

namespace
{
    std::mutex g_instance_dispatch_mutex;
    std::unordered_map<XrInstance, PFN_xrGetInstanceProcAddr> g_instance_dispatch_map;

    static const char* LAYER_NAME = "XR_APILAYER_KHRONOS_conformance_test_layer";

    XRAPI_ATTR XrResult XRAPI_CALL TestLayer_CreateApiLayerInstance(const XrInstanceCreateInfo* createInfo,
                                                                    const XrApiLayerCreateInfo* apiLayerInfo, XrInstance* instance)
    {
        //
        // 1. Validate input data from Loader / previous layer
        //
        {
            if (apiLayerInfo == nullptr || apiLayerInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO ||
                apiLayerInfo->structVersion != XR_API_LAYER_CREATE_INFO_STRUCT_VERSION ||
                apiLayerInfo->structSize != sizeof(XrApiLayerCreateInfo)) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }

            // apiLayerInfo->loaderInstance is deprecated and must be ignored
            // apiLayerInfo->settings_file_location is currently unused.

            if (apiLayerInfo->nextInfo == nullptr || apiLayerInfo->nextInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO ||
                apiLayerInfo->nextInfo->structVersion != XR_API_LAYER_NEXT_INFO_STRUCT_VERSION ||
                apiLayerInfo->nextInfo->structSize != sizeof(XrApiLayerNextInfo)) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }

            if (strcmp(apiLayerInfo->nextInfo->layerName, LAYER_NAME) != 0) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }

            if (apiLayerInfo->nextInfo->nextGetInstanceProcAddr == nullptr ||
                apiLayerInfo->nextInfo->nextCreateApiLayerInstance == nullptr) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }
        }

        //
        // 2.A. Checks associated with https://gitlab.khronos.org/openxr/openxr/-/issues/2333
        //      API Layers may need to know information about the other layers or the runtime,
        //      and to do so has to query information from the next chain.
        //
        {
            PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties{nullptr};
            XrResult res = apiLayerInfo->nextInfo->nextGetInstanceProcAddr(
                XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
                reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateInstanceExtensionProperties));
            if (res != XR_SUCCESS) {
                return res;
            }

            uint32_t extensionsCount = 0;
            res = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionsCount, nullptr);
            if (res != XR_SUCCESS) {
                return res;
            }

            std::vector<XrExtensionProperties> extensions(extensionsCount, {XR_TYPE_EXTENSION_PROPERTIES});
            res = xrEnumerateInstanceExtensionProperties(nullptr, extensionsCount, &extensionsCount, extensions.data());
            if (res != XR_SUCCESS) {
                return res;
            }

            // The API layer would now cache extensions vector or use it for validation.
        }

        //
        // 2.B. Checks for feature availability in instances.
        //
        {
            XrInstance temporaryInstance{XR_NULL_HANDLE};

            XrApiLayerCreateInfo temporaryNextApiLayerInfo = *apiLayerInfo;
            temporaryNextApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

            XrResult res = apiLayerInfo->nextInfo->nextCreateApiLayerInstance(createInfo, &temporaryNextApiLayerInfo, &temporaryInstance);

            if (res != XR_SUCCESS) {
                return res;
            }

            // The API layer would now query the instance using other functions to validate feature availability.

            PFN_xrDestroyInstance xrDestroyInstance{nullptr};
            res = apiLayerInfo->nextInfo->nextGetInstanceProcAddr(temporaryInstance, "xrDestroyInstance",
                                                                  reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyInstance));
            res = xrDestroyInstance(temporaryInstance);
            if (res != XR_SUCCESS) {
                return res;
            }
        }

        //
        // 3. Call down to the next layer's xrCreateApiLayerInstance and record next gipa.
        //
        {

            // Clone the XrApiLayerCreateInfo, but move to the next XrApiLayerNextInfo in the chain. nextInfo will be null
            // if the loader's terminator function is going to be called (between the layer and the runtime) but this is OK
            // because the loader's terminator function won't use it.
            XrApiLayerCreateInfo newApiLayerInfo = *apiLayerInfo;
            newApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

            XrResult nextLayerCreateRes = apiLayerInfo->nextInfo->nextCreateApiLayerInstance(createInfo, &newApiLayerInfo, instance);
            if (XR_FAILED(nextLayerCreateRes)) {
                // Some layer higher the chain failed - we return the error.
                return nextLayerCreateRes;
            }

            {
                // Record the get instance proc addr for the next layer in the chain.
                std::unique_lock<std::mutex> lock(g_instance_dispatch_mutex);
                g_instance_dispatch_map[*instance] = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
            }

            return XR_SUCCESS;
        }
    }

    XRAPI_ATTR XrResult XRAPI_CALL TestLayer_GetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function)
    {
        if (instance == XR_NULL_HANDLE) {
            // assert(false);
            return XR_SUCCESS;
        }

        PFN_xrGetInstanceProcAddr nextGetProcAddr = nullptr;
        {
            std::unique_lock<std::mutex> lock(g_instance_dispatch_mutex);
            auto it = g_instance_dispatch_map.find(instance);
            if (it == g_instance_dispatch_map.end()) {
                return XR_ERROR_HANDLE_INVALID;
            }
            nextGetProcAddr = it->second;
        }
        return nextGetProcAddr(instance, name, function);
    }
}  // namespace

extern "C" LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL testLayer_xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo, const char* apiLayerName, XrNegotiateApiLayerRequest* apiLayerRequest)
{
    if (loaderInfo == nullptr || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo)) {
        // TODO: Log reason somehow.
        // LogPlatformUtilsError("loaderInfo struct is not valid");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION) {
        // TODO: Log reason somehow.
        // LogPlatformUtilsError("loader interface version is not in the range [minInterfaceVersion, maxInterfaceVersion]");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (loaderInfo->minApiVersion > XR_CURRENT_API_VERSION || loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION) {
        // TODO: Log reason somehow.
        // LogPlatformUtilsError("loader api version is not in the range [minApiVersion, maxApiVersion]");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (strcmp(apiLayerName, LAYER_NAME) != 0) {
        // TODO: Log reason somehow.
        // LogPlatformUtilsError("loader layer name does not match expected name");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (apiLayerRequest == nullptr || apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest)) {
        // TODO: Log reason somehow.
        // LogPlatformUtilsError("apiLayerRequest is not valid");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
    apiLayerRequest->getInstanceProcAddr = TestLayer_GetInstanceProcAddr;
    apiLayerRequest->createApiLayerInstance = TestLayer_CreateApiLayerInstance;

    return XR_SUCCESS;
}

namespace Conformance
{
    TEST_CASE("validApiLayer", "")
    {
        GlobalData& globalData = GetGlobalData();

        // Layer for run-time conformance (and anything else global)
        StringVec enabledApiLayers = globalData.enabledAPILayerNames;
        // plus our test layer
        enabledApiLayers.push_back(LAYER_NAME);

        // Enable only the required platform extensions by default
        auto enabledExtensions = StringVec(globalData.requiredPlatformInstanceExtensions);

        XrInstance instance = XR_NULL_HANDLE_CPP;
        CleanupInstanceOnScopeExit cleanup(instance);

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};

        strcpy(createInfo.applicationInfo.applicationName, "conformance test");
        createInfo.applicationInfo.applicationVersion = 1;
        // Leave engineName and engineVersion empty, which is valid usage.
        createInfo.applicationInfo.apiVersion = Options::Get().minApiVersionValue;

        if (globalData.requiredPlatformInstanceCreateStruct) {
            createInfo.next = globalData.requiredPlatformInstanceCreateStruct;
        }

        createInfo.enabledApiLayerCount = (uint32_t)enabledApiLayers.size();
        createInfo.enabledApiLayerNames = enabledApiLayers.data();
        createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
        createInfo.enabledExtensionNames = enabledExtensions.data();

        SECTION("XR_SUCCESS, only platform-required extensions enabled")
        {
            REQUIRE(XR_SUCCESS == xrCreateInstance(&createInfo, &instance));
        }
    }

}  // namespace Conformance
