// Copyright (c) 2025 The Khronos Group Inc.
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

#ifndef __ANDROID__
#error "Android only file"
#endif

/**
 * On Android, there is no manifest file for the Vulkan layers. The Vulkan Loader will load any
 * library in the .apk file that starts with the `libVkLayer` prefix and ends with the `.so`
 * suffix as a layer (see [1]).
 *
 * This file acts as a passthrough on Android, it will be loaded as a layer by the Vulkan Loader
 * and will forward the Vulkan calls to the Vulkan layer part of XrApiLayer_runtime_conformance.
 *
 *  [1]: https://github.com/android-source/platform_frameworks_native/blob/master/vulkan/libvulkan/layers_extensions.cpp#L399
 */

#include <vulkan/vk_layer.h>

#include <dlfcn.h>

#define PFN_LOAD(func) (PFN_##func) dlsym(getHandle(), #func)

static void *getHandle(void)
{
    static void *handle = NULL;
    if (handle == NULL) {
        handle = dlopen("libXrApiLayer_runtime_conformance.so", RTLD_NOW | RTLD_LOCAL);
    }

    return handle;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *funcName)
{
    static PFN_vkGetInstanceProcAddr getInstanceProcAddr = NULL;
    if (getInstanceProcAddr == NULL) {
        getInstanceProcAddr = PFN_LOAD(vkGetInstanceProcAddr);
    }

    return getInstanceProcAddr(instance, funcName);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *funcName)
{
    static PFN_vkGetDeviceProcAddr getDeviceProcAddr = NULL;
    if (getDeviceProcAddr == NULL) {
        getDeviceProcAddr = PFN_LOAD(vkGetDeviceProcAddr);
    }

    return getDeviceProcAddr(device, funcName);
}

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *pVersionStruct)
{
    static PFN_vkNegotiateLoaderLayerInterfaceVersion negotiateLoaderLayerInterfaceVersion = NULL;
    if (negotiateLoaderLayerInterfaceVersion == NULL) {
        negotiateLoaderLayerInterfaceVersion = PFN_LOAD(vkNegotiateLoaderLayerInterfaceVersion);
    }

    return negotiateLoaderLayerInterfaceVersion(pVersionStruct);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t *pCount, VkLayerProperties *pProperties)
{
    static PFN_vkEnumerateInstanceLayerProperties enumerateInstanceLayerProperties = NULL;
    if (enumerateInstanceLayerProperties == NULL) {
        enumerateInstanceLayerProperties = PFN_LOAD(vkEnumerateInstanceLayerProperties);
    }

    return enumerateInstanceLayerProperties(pCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pCount,
                                                                VkLayerProperties *pProperties)
{
    static PFN_vkEnumerateDeviceLayerProperties enumerateDeviceLayerProperties = NULL;
    if (enumerateDeviceLayerProperties == NULL) {
        enumerateDeviceLayerProperties = PFN_LOAD(vkEnumerateDeviceLayerProperties);
    }

    return enumerateDeviceLayerProperties(physicalDevice, pCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char *pLayerName, uint32_t *pCount,
                                                                      VkExtensionProperties *pProperties)
{
    static PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties = NULL;
    if (enumerateInstanceExtensionProperties == NULL) {
        enumerateInstanceExtensionProperties = PFN_LOAD(vkEnumerateInstanceExtensionProperties);
    }

    return enumerateInstanceExtensionProperties(pLayerName, pCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName,
                                                                    uint32_t *pCount, VkExtensionProperties *pProperties)
{
    static PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties = NULL;
    if (enumerateDeviceExtensionProperties == NULL) {
        enumerateDeviceExtensionProperties = PFN_LOAD(vkEnumerateDeviceExtensionProperties);
    }

    return enumerateDeviceExtensionProperties(physicalDevice, pLayerName, pCount, pProperties);
}
