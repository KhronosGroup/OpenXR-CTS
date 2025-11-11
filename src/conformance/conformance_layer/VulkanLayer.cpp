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

#include <vulkan/vk_layer.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#if !defined(VK_LAYER_EXPORT)
#if defined(__GNUC__) && __GNUC__ >= 4
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#else
#define VK_LAYER_EXPORT
#endif
#endif

struct Device;

struct Instance
{
    VkInstance handle{};
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr{};
    PFN_vkDestroyInstance vkDestroyInstance{};

    std::vector<std::string> enabled_extensions{};

    Instance(VkInstance instance, PFN_vkGetInstanceProcAddr getInstanceProcAddr)
        : handle(instance), vkGetInstanceProcAddr(getInstanceProcAddr)
    {
#define PFN_LOAD(func) func = (PFN_##func)vkGetInstanceProcAddr(handle, #func)
        PFN_LOAD(vkDestroyInstance);
#undef PFN_LOAD
    }
};

struct Queue
{
    VkQueue handle{};
    uint32_t queueFamilyIndex{};
    uint32_t queueIndex{};

    Device *device = nullptr;

    std::atomic_bool accessed{false};

    Queue(VkQueue queue, uint32_t queueFamilyIndex, uint32_t queueIndex, Device *device)
        : handle(queue), queueFamilyIndex(queueFamilyIndex), queueIndex(queueIndex), device(device)
    {
    }

    Queue(const Queue &other)
        : handle(other.handle)
        , queueFamilyIndex(other.queueFamilyIndex)
        , queueIndex(other.queueIndex)
        , device(other.device)
        , accessed(other.accessed.load(std::memory_order_seq_cst))
    {
    }
};

struct Device
{
    VkDevice handle{};
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr{};
    PFN_vkDestroyDevice vkDestroyDevice{};
    PFN_vkGetDeviceQueue vkGetDeviceQueue{};
    PFN_vkQueueSubmit vkQueueSubmit{};
    PFN_vkQueueWaitIdle vkQueueWaitIdle{};

    std::vector<std::string> enabled_extensions{};
    std::list<Queue> queues;

    Device(VkDevice device, PFN_vkGetDeviceProcAddr getDeviceProcAddr) : handle(device), vkGetDeviceProcAddr(getDeviceProcAddr)
    {
#define PFN_LOAD(func) func = (PFN_##func)vkGetDeviceProcAddr(handle, #func)
        PFN_LOAD(vkDestroyDevice);
        PFN_LOAD(vkGetDeviceQueue);
        PFN_LOAD(vkQueueSubmit);
        PFN_LOAD(vkQueueWaitIdle);
#undef PFN_LOAD
    }
};

static std::mutex &GetLayerMutex()
{
    static std::mutex layer_mutex{};
    return layer_mutex;
}

/// Must only call and use return value while holding the lock from @ref GetLayerMutex()
static std::list<Instance> &GetInstanceListLocked()
{
    static std::list<Instance> instances{};
    return instances;
}

/// Must only call and use return value while holding the lock from @ref GetLayerMutex()
static std::list<Device> &GetDeviceListLocked()
{
    static std::list<Device> devices{};
    return devices;
}

static Instance *getInstance(VkInstance instance)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Instance> &instances = GetInstanceListLocked();
    const auto it = std::find_if(instances.begin(), instances.end(), [&instance](const auto &i) { return i.handle == instance; });
    if (it != instances.end()) {
        return &(*it);
    }
    return NULL;
}

static Device *getDevice(VkDevice device)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Device> &devices = GetDeviceListLocked();
    const auto it = std::find_if(devices.begin(), devices.end(), [&device](const auto &d) { return d.handle == device; });
    if (it != devices.end()) {
        return &(*it);
    }
    return NULL;
}

static Queue *getQueue(VkQueue queue)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Device> &devices = GetDeviceListLocked();
    for (Device &d : devices) {
        const auto it = std::find_if(d.queues.begin(), d.queues.end(), [&queue](const auto &q) { return q.handle == queue; });
        if (it != d.queues.end()) {
            return &(*it);
        }
    }

    return NULL;
}

bool InstanceVkExtensionEnabled(VkInstance instance, const char *extension)
{
    Instance *i = getInstance(instance);
    assert(i);

    return std::find(i->enabled_extensions.begin(), i->enabled_extensions.end(), std::string{extension}) != i->enabled_extensions.end();
}

bool DeviceVkExtensionEnabled(VkDevice device, const char *extension)
{
    Device *d = getDevice(device);
    assert(d);

    return std::find(d->enabled_extensions.begin(), d->enabled_extensions.end(), std::string{extension}) != d->enabled_extensions.end();
}

void ResetVkQueueAccess(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    Device *d = getDevice(device);
    assert(d);

    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    for (Queue &q : d->queues) {
        if (q.queueFamilyIndex == queueFamilyIndex && q.queueIndex == queueIndex) {
            q.accessed.store(false);
            return;
        }
    }
}

bool CheckVkQueueAccess(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    Device *d = getDevice(device);
    assert(d);

    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    for (Queue &q : d->queues) {
        if (q.queueFamilyIndex == queueFamilyIndex && q.queueIndex == queueIndex) {
            return q.accessed.load();
        }
    }

    return false;
}

static VKAPI_ATTR VkResult VKAPI_CALL createInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
                                                     VkInstance *pInstance)
{
    // Find the loader info with the layer link: VK_LAYER_LINK_INFO
    VkLayerInstanceCreateInfo *pLayerCreateInfo = (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;

    while (pLayerCreateInfo) {
        if (pLayerCreateInfo->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && pLayerCreateInfo->function == VK_LAYER_LINK_INFO) {
            break;
        }

        pLayerCreateInfo = (VkLayerInstanceCreateInfo *)pLayerCreateInfo->pNext;
    }

    if (!pLayerCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    assert(pLayerCreateInfo->u.pLayerInfo);

    // Next layer/runtime's GIPA
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = pLayerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkCreateInstance fpCreateInstance = (PFN_vkCreateInstance)getInstanceProcAddr(NULL, "vkCreateInstance");
    if (fpCreateInstance == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Pop one level of layer info
    pLayerCreateInfo->u.pLayerInfo = pLayerCreateInfo->u.pLayerInfo->pNext;

    // Call into next layer/runtime
    VkResult result = fpCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Initialize our per-instance object using the instance handle and the next level GIPA
    // (outside the lock) then move it in
    Instance inst{*pInstance, getInstanceProcAddr};
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        inst.enabled_extensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);
    }

    {
        const std::lock_guard<std::mutex> lock(GetLayerMutex());
        GetInstanceListLocked().emplace_back(std::move(inst));
    }

    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL destroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    auto it = std::find_if(GetInstanceListLocked().begin(), GetInstanceListLocked().end(),
                           [&instance](const auto &i) { return i.handle == instance; });

    it->vkDestroyInstance(instance, pAllocator);
    GetInstanceListLocked().erase(it);
}

static VKAPI_ATTR VkResult VKAPI_CALL createDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
    // Find the loader info with the layer link: VK_LAYER_LINK_INFO
    VkLayerDeviceCreateInfo *pLayerCreateInfo = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;

    while (pLayerCreateInfo) {
        if (pLayerCreateInfo->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && pLayerCreateInfo->function == VK_LAYER_LINK_INFO) {
            break;
        }

        pLayerCreateInfo = (VkLayerDeviceCreateInfo *)pLayerCreateInfo->pNext;
    }

    if (!pLayerCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    assert(pLayerCreateInfo->u.pLayerInfo);

    // Next layer/runtime's GIPA and GDPA
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = pLayerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = pLayerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;

    PFN_vkCreateDevice fpCreateDevice = (PFN_vkCreateDevice)getInstanceProcAddr(NULL, "vkCreateDevice");
    if (fpCreateDevice == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Pop one level of layer info
    pLayerCreateInfo->u.pLayerInfo = pLayerCreateInfo->u.pLayerInfo->pNext;

    // Call into next layer/runtime
    VkResult result = fpCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Initialize our per-device object using the device handle and the next level GDPA
    // (outside the lock) then move it in.
    Device dev{*pDevice, getDeviceProcAddr};
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        dev.enabled_extensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);
    }

    {
        const std::lock_guard<std::mutex> lock(GetLayerMutex());
        GetDeviceListLocked().emplace_back(std::move(dev));
    }

    return VK_SUCCESS;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getInstanceProcAddr(VkInstance instance, const char *pName)
{
    const std::string pfn(pName);

    if (pfn == "vkCreateInstance") {
        return (PFN_vkVoidFunction)createInstance;
    }
    if (pfn == "vkDestroyInstance") {
        return (PFN_vkVoidFunction)destroyInstance;
    }
    if (pfn == "vkCreateDevice") {
        return (PFN_vkVoidFunction)createDevice;
    }

    return getInstance(instance)->vkGetInstanceProcAddr(instance, pName);
}

static VKAPI_ATTR void VKAPI_CALL destroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    auto it =
        std::find_if(GetDeviceListLocked().begin(), GetDeviceListLocked().end(), [&device](const auto &d) { return d.handle == device; });

    it->vkDestroyDevice(device, pAllocator);
    GetDeviceListLocked().erase(it);
}

static VKAPI_ATTR void VKAPI_CALL getDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue)
{
    Device *d = getDevice(device);

    d->vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);

    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    d->queues.emplace_back(*pQueue, queueFamilyIndex, queueIndex, d);
}

static VKAPI_ATTR VkResult VKAPI_CALL queueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
    Queue *q = getQueue(queue);
    VkResult res = q->device->vkQueueSubmit(queue, submitCount, pSubmits, fence);
    q->accessed.store(true);
    return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL queueWaitIdle(VkQueue queue)
{
    Queue *q = getQueue(queue);
    VkResult res = q->device->vkQueueWaitIdle(queue);
    q->accessed.store(true);
    return res;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getDeviceProcAddr(VkDevice device, const char *pName)
{
    const std::string pfn(pName);

    if (pfn == "vkDestroyDevice") {
        return (PFN_vkVoidFunction)destroyDevice;
    }
    if (pfn == "vkGetDeviceQueue") {
        return (PFN_vkVoidFunction)getDeviceQueue;
    }
    if (pfn == "vkQueueSubmit") {
        return (PFN_vkVoidFunction)queueSubmit;
    }
    if (pfn == "vkQueueWaitIdle") {
        return (PFN_vkVoidFunction)queueWaitIdle;
    }

    return getDevice(device)->vkGetDeviceProcAddr(device, pName);
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *pVersionStruct)
{
    if (pVersionStruct == NULL || pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pVersionStruct->loaderLayerInterfaceVersion = CURRENT_LOADER_LAYER_INTERFACE_VERSION;
    pVersionStruct->pfnGetInstanceProcAddr = getInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = getDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
    return getInstanceProcAddr(instance, pName);
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
    return getDeviceProcAddr(device, pName);
}

// Keep in sync with conformance_vk_layer.json
static const VkLayerProperties layerProperty = {
    "VK_LAYER_OPENXR_xr_runtime_conformance",
    VK_API_VERSION_1_0,
    1,
    "API Layer to validate OpenXR runtime conformance",
};

static VkResult getLayerProperties(uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
    if (pPropertyCount == NULL) {
        return VK_INCOMPLETE;
    }
    if (pProperties == NULL) {
        *pPropertyCount = 1;
        return VK_SUCCESS;
    }

    *pPropertyCount = 1;
    memcpy(pProperties, &layerProperty, sizeof(layerProperty));

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t *pCount, VkLayerProperties *pProperties)
{
    return getLayerProperties(pCount, pProperties);
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pCount,
                                                                                VkLayerProperties *pProperties)
{
    (void)physicalDevice;
    return getLayerProperties(pCount, pProperties);
}

static VkResult getLayerExtensionProps(const char *pLayerName, uint32_t *pCount, VkExtensionProperties *pProperties)
{
    (void)pProperties;
    if (strcmp(pLayerName, layerProperty.layerName) == 0) {
        *pCount = 0;
        return VK_SUCCESS;
    }

    return VK_ERROR_LAYER_NOT_PRESENT;
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char *pLayerName, uint32_t *pCount,
                                                                                      VkExtensionProperties *pProperties)
{
    return getLayerExtensionProps(pLayerName, pCount, pProperties);
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName,
                                                                                    uint32_t *pCount, VkExtensionProperties *pProperties)
{
    (void)physicalDevice;
    return getLayerExtensionProps(pLayerName, pCount, pProperties);
}
