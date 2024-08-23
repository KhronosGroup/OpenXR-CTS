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

#include "Common.h"
#include "ConformanceHooks.h"
#include "HandleState.h"
#include "gen_dispatch.h"

#include <cstring>

namespace
{
    static const char* LAYER_NAME = "XR_APILAYER_KHRONOS_runtime_conformance";

    XRAPI_ATTR XrResult XRAPI_CALL ConformanceLayer_RegisterInstance(const XrInstanceCreateInfo* createInfo,
                                                                     const XrApiLayerCreateInfo* apiLayerInfo, XrInstance* instance)
    {
        try {
            // Call down to the next layer's xrCreateApiLayerInstance.
            {
                // Clone the XrApiLayerCreateInfo, but move to the next XrApiLayerNextInfo in the chain. nextInfo will be null
                // if the loader's terminator function is going to be called (between the layer and the runtime) but this is OK
                // because the loader's terminator function won't use it.
                XrApiLayerCreateInfo newApiLayerInfo = *apiLayerInfo;
                newApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

                const XrResult res = apiLayerInfo->nextInfo->nextCreateApiLayerInstance(createInfo, &newApiLayerInfo, instance);
                if (XR_FAILED(res)) {
                    return res;  // The next layer's xrCreateApiLayerInstance failed.
                }
            }

            // Generate the dispatch table using the next layer's xrGetInstanceProcAddr implementation.
            XrGeneratedDispatchTable dispatchTable{};
            GeneratedXrPopulateDispatchTable(&dispatchTable, *instance, apiLayerInfo->nextInfo->nextGetInstanceProcAddr);

            std::shared_ptr<ConformanceHooksBase> conformanceHooks =
                std::make_shared<ConformanceHooks>(*instance, dispatchTable, EnabledVersions(createInfo), EnabledExtensions(createInfo));

            // Register the instance handle in the lookup table.
            RegisterHandleState(std::unique_ptr<HandleState>(
                new HandleState((IntHandle)*instance, XR_OBJECT_TYPE_INSTANCE, nullptr /* no parent */, conformanceHooks)));

            return XR_SUCCESS;
        }
        catch (...) {
            return XR_ERROR_INITIALIZATION_FAILED;
        }
    }
}  // namespace

#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define LAYER_EXPORT __attribute__((visibility("default")))
#else
#define LAYER_EXPORT
#endif

// Function used to negotiate an interface betewen the loader and an API layer.  Each library exposing one or
// more API layers needs to expose at least this function.
extern "C" LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo* loaderInfo,
                                                                                          const char* apiLayerName,
                                                                                          XrNegotiateApiLayerRequest* apiLayerRequest)
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
    apiLayerRequest->getInstanceProcAddr = ConformanceLayer_xrGetInstanceProcAddr;
    apiLayerRequest->createApiLayerInstance = ConformanceLayer_RegisterInstance;

    return XR_SUCCESS;
}
