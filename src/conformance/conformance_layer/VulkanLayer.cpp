// Copyright (c) 2025-2026 The Khronos Group Inc.
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
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Cannot use platform_exports.h as vk_layer.h forward declarations conflict
// https://github.com/KhronosGroup/Vulkan-Loader/blob/main/tests/framework/layer/test_layer.cpp#L1055-L1056
#if !defined(VK_LAYER_EXPORT)
#if defined(__GNUC__) && __GNUC__ >= 4
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#else
#define VK_LAYER_EXPORT
#endif
#endif

#include "VulkanLayer.h"

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

    std::map<std::thread::id, bool> accessed;

    Queue(VkQueue queue, uint32_t queueFamilyIndex, uint32_t queueIndex, Device *device)
        : handle(queue), queueFamilyIndex(queueFamilyIndex), queueIndex(queueIndex), device(device)
    {
    }
};

struct Swapchain
{
    VkSwapchainKHR handle{};
    VkSwapchainCreateInfoKHR createInfo{};

    Swapchain(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR *pCreateInfo) : handle(swapchain), createInfo(*pCreateInfo)
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
    PFN_vkCreateImage vkCreateImage{};
    PFN_vkDestroyImage vkDestroyImage{};
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR{};
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR{};
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR{};

    std::vector<std::string> enabled_extensions{};
    std::list<Queue> queues;
    std::list<Swapchain> swapchains;

    Device(VkDevice device, PFN_vkGetDeviceProcAddr getDeviceProcAddr) : handle(device), vkGetDeviceProcAddr(getDeviceProcAddr)
    {
#define PFN_LOAD(func) func = (PFN_##func)vkGetDeviceProcAddr(handle, #func)
        PFN_LOAD(vkDestroyDevice);
        PFN_LOAD(vkGetDeviceQueue);
        PFN_LOAD(vkQueueSubmit);
        PFN_LOAD(vkQueueWaitIdle);
        PFN_LOAD(vkCreateImage);
        PFN_LOAD(vkDestroyImage);
        PFN_LOAD(vkCreateSwapchainKHR);
        PFN_LOAD(vkDestroySwapchainKHR);
        PFN_LOAD(vkGetSwapchainImagesKHR);
#undef PFN_LOAD
    }

    Swapchain *getSwapchain(VkSwapchainKHR swapchain)
    {
        auto it = std::find_if(swapchains.begin(), swapchains.end(), [&swapchain](const auto &s) { return s.handle == swapchain; });
        if (it != swapchains.end()) {
            return &(*it);
        }

        return nullptr;
    }
};

struct Image
{
    VkImage handle{};
    VkFormat format{};

    Image(VkImage image, const VkImageCreateInfo *pCreateInfo) : handle(image), format(pCreateInfo->format)
    {
    }

    Image(VkImage image, const VkSwapchainCreateInfoKHR *pCreateInfo) : handle(image), format(pCreateInfo->imageFormat)
    {
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

/// Must only call and use return value while holding the lock from @ref GetLayerMutex()
static std::list<Image> &GetImageListLocked()
{
    static std::list<Image> images{};
    return images;
}

static Instance *getInstance(VkInstance instance)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Instance> &instances = GetInstanceListLocked();
    const auto it = std::find_if(instances.begin(), instances.end(), [&instance](const auto &i) { return i.handle == instance; });
    if (it != instances.end()) {
        return &(*it);
    }
    return nullptr;
}

static Device *getDevice(VkDevice device)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Device> &devices = GetDeviceListLocked();
    const auto it = std::find_if(devices.begin(), devices.end(), [&device](const auto &d) { return d.handle == device; });
    if (it != devices.end()) {
        return &(*it);
    }
    return nullptr;
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

    return nullptr;
}

static Image *getImage(VkImage image)
{
    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Image> &images = GetImageListLocked();
    auto it = std::find_if(images.begin(), images.end(), [&image](const auto &i) { return i.handle == image; });
    if (it != images.end()) {
        return &(*it);
    }
    return nullptr;
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
            q.accessed[std::this_thread::get_id()] = false;
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
            return q.accessed.at(std::this_thread::get_id());
        }
    }

    return false;
}

VkFormat GetVkImageFormat(VkImage image)
{
    Image *i = getImage(image);
    assert(i);

    return i->format;
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
    PFN_vkCreateInstance fpCreateInstance = (PFN_vkCreateInstance)getInstanceProcAddr(nullptr, "vkCreateInstance");
    if (fpCreateInstance == nullptr) {
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

    PFN_vkCreateDevice fpCreateDevice = (PFN_vkCreateDevice)getInstanceProcAddr(nullptr, "vkCreateDevice");
    if (fpCreateDevice == nullptr) {
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

    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    q->accessed[std::this_thread::get_id()] = true;
    return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL queueWaitIdle(VkQueue queue)
{
    Queue *q = getQueue(queue);
    VkResult res = q->device->vkQueueWaitIdle(queue);

    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    q->accessed[std::this_thread::get_id()] = true;
    return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
    Device *d = getDevice(device);
    assert(d);

    VkResult res = d->vkCreateImage(device, pCreateInfo, pAllocator, pImage);
    {
        const std::lock_guard<std::mutex> lock(GetLayerMutex());
        std::list<Image> &images = GetImageListLocked();
        images.emplace_back(*pImage, pCreateInfo);
    }

    return res;
}

static VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
    Device *d = getDevice(device);
    assert(d);

    const std::lock_guard<std::mutex> lock(GetLayerMutex());
    std::list<Image> &images = GetImageListLocked();
    auto it = std::find_if(images.begin(), images.end(), [&image](const auto &i) { return i.handle == image; });
    images.erase(it);

    d->vkDestroyImage(device, image, pAllocator);
}

static VKAPI_ATTR VkResult VKAPI_CALL createSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                         const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
    Device *d = getDevice(device);
    assert(d);

    VkResult res = d->vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    {
        const std::lock_guard<std::mutex> lock(GetLayerMutex());
        d->swapchains.emplace_back(*pSwapchain, pCreateInfo);
    }

    return res;
}

static VKAPI_ATTR void VKAPI_CALL destroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
    Device *d = getDevice(device);
    assert(d);

    {
        const std::lock_guard<std::mutex> lock(GetLayerMutex());
        auto it = std::find_if(d->swapchains.begin(), d->swapchains.end(), [&swapchain](const auto &s) { return s.handle == swapchain; });
        d->swapchains.erase(it);
    }

    d->vkDestroySwapchainKHR(device, swapchain, pAllocator);
}

static VKAPI_ATTR VkResult VKAPI_CALL getSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *pSwapchainImageCount,
                                                            VkImage *pSwapchainImages)
{
    Device *d = getDevice(device);
    assert(d);

    Swapchain *s = d->getSwapchain(swapchain);
    assert(s);

    VkResult res = d->vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);

    if (*pSwapchainImageCount > 0) {
        const std::lock_guard<std::mutex> lock(GetLayerMutex());
        std::list<Image> &images = GetImageListLocked();

        for (uint32_t i = 0; i < *pSwapchainImageCount; ++i) {
            images.emplace_back(pSwapchainImages[i], &s->createInfo);
        }
    }

    return res;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getDeviceProcAddr(VkDevice device, const char *pName)
{
    const std::string pfn(pName);

    if (pfn == "vkGetDeviceProcAddr") {
        return (PFN_vkVoidFunction)getDeviceProcAddr;
    }
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
    if (pfn == "vkCreateImage") {
        return (PFN_vkVoidFunction)createImage;
    }
    if (pfn == "vkDestroyImage") {
        return (PFN_vkVoidFunction)destroyImage;
    }
    if (pfn == "vkCreateSwapchainKHR") {
        return (PFN_vkVoidFunction)createSwapchainKHR;
    }
    if (pfn == "vkDestroySwapchainKHR") {
        return (PFN_vkVoidFunction)destroySwapchainKHR;
    }
    if (pfn == "vkGetSwapchainImagesKHR") {
        return (PFN_vkVoidFunction)getSwapchainImagesKHR;
    }

    return getDevice(device)->vkGetDeviceProcAddr(device, pName);
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getInstanceProcAddr(VkInstance instance, const char *pName)
{
    const std::string pfn(pName);

    if (pfn == "vkGetInstanceProcAddr") {
        return (PFN_vkVoidFunction)getInstanceProcAddr;
    }
    if (pfn == "vkGetDeviceProcAddr") {
        return (PFN_vkVoidFunction)getDeviceProcAddr;
    }
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

extern "C" VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *pVersionStruct)
{
    if (pVersionStruct == nullptr || pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pVersionStruct->loaderLayerInterfaceVersion = CURRENT_LOADER_LAYER_INTERFACE_VERSION;
    pVersionStruct->pfnGetInstanceProcAddr = getInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = getDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

    return VK_SUCCESS;
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
    return getInstanceProcAddr(instance, pName);
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
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
    if (pPropertyCount == nullptr) {
        return VK_INCOMPLETE;
    }
    if (pProperties == nullptr) {
        *pPropertyCount = 1;
        return VK_SUCCESS;
    }

    *pPropertyCount = 1;
    memcpy(pProperties, &layerProperty, sizeof(layerProperty));

    return VK_SUCCESS;
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t *pCount,
                                                                                             VkLayerProperties *pProperties)
{
    return getLayerProperties(pCount, pProperties);
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                                                                           uint32_t *pCount, VkLayerProperties *pProperties)
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

extern "C" VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char *pLayerName, uint32_t *pCount,
                                                                                                 VkExtensionProperties *pProperties)
{
    return getLayerExtensionProps(pLayerName, pCount, pProperties);
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                                                               const char *pLayerName, uint32_t *pCount,
                                                                                               VkExtensionProperties *pProperties)
{
    (void)physicalDevice;
    return getLayerExtensionProps(pLayerName, pCount, pProperties);
}
