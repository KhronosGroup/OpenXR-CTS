// Copyright (c) 2019-2020 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "Common.h"
#include "ConformanceHooks.h"
#include "gen_dispatch.h"

// Android's security model differ significantly from other platforms.
// As such, only layers delivered as part of the application will be enabled.
// No manifest file will be used, instead introspection functions will be used to
// query API Layer properties. These Introspection functions are outlined in the
// OpenXR Loader specification doc: 'API Layer Manifest JSON Fields'.
#ifdef XR_USE_PLATFORM_ANDROID

#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define LAYER_EXPORT __attribute__((visibility("default")))
#else
#define LAYER_EXPORT
#endif

static const char* conformance_layer_name = "XR_APILAYER_KHRONOS_runtime_conformance";
static const char* conformance_layer_description = "API Layer to validate OpenXR runtime conformance";
extern "C" LAYER_EXPORT XrResult XRAPI_CALL XRAPI_CALL xrEnumerateApiLayerProperties(uint32_t propertyCapacityInput,
                                                                                     uint32_t* propertyCountOutput,
                                                                                     XrApiLayerProperties* properties)
{

    if (propertyCountOutput == NULL) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    // If 'propertyCapacityInput' is 0, return the required capacity in 'propertyCountOutput'.
    if (propertyCapacityInput == 0) {
        *propertyCountOutput = 1;
        return XR_SUCCESS;
    }
    else if (propertyCapacityInput < 1) {
        *propertyCountOutput = 1;
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    else if (properties == NULL) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    *propertyCountOutput = 1;

    XrApiLayerProperties* prop = &properties[0];
    strncpy(prop->layerName, conformance_layer_name, XR_MAX_API_LAYER_NAME_SIZE - 1);
    prop->layerName[XR_MAX_API_LAYER_NAME_SIZE - 1] = '\0';
    prop->specVersion = XR_CURRENT_API_VERSION;
    prop->layerVersion = 1;
    strncpy(prop->description, conformance_layer_description, XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1);
    prop->description[XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1] = '\0';

    return XR_SUCCESS;
}

extern "C" LAYER_EXPORT XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput,
                                                                                   uint32_t* propertyCountOutput,
                                                                                   XrExtensionProperties* properties)
{

    if (layerName && !strcmp(layerName, conformance_layer_name)) {
        if (propertyCountOutput == NULL) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        // If 'propertyCapacityInput' is 0, return the required capacity in 'propertyCountOutput'.
        if (propertyCapacityInput == 0) {
            *propertyCountOutput = 0;
            return XR_SUCCESS;
        }
        else if (propertyCapacityInput < 0) {
            *propertyCountOutput = 0;
            return XR_ERROR_SIZE_INSUFFICIENT;
        }
        else if (properties == NULL) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        return XR_SUCCESS;
    }

    return XR_ERROR_API_LAYER_NOT_PRESENT;
}

#endif  // XR_USE_PLATFORM_ANDROID
