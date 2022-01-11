// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#pragma once

#include "Common.h"
#include "gen_dispatch.h"
#include <openxr/openxr_reflection.h>

// Backs up the chain of type and next pointers. On destruction, validates there have been no changes.
// This should be used on all non-const pointer arguments (out parameters).
struct XrBaseStructChainValidator
{
    XrBaseStructChainValidator(ConformanceHooksBase* conformanceHook, const void* arg, std::string parameterName, std::string functionName);
    ~XrBaseStructChainValidator();

private:
    ConformanceHooksBase* const m_conformanceHook;
    const std::string m_parameterName;
    const std::string m_functionName;
    const XrBaseInStructure* const m_head;
    std::deque<XrBaseInStructure> m_chainCache;
};

void ValidateXrBool32(ConformanceHooksBase* conformanceHook, XrBool32 value, const char* valueName, const char* xrFunctionName);
void ValidateFloat(ConformanceHooksBase* conformanceHook, float value, float min, float max, const char* valueName,
                   const char* xrFunctionName);
void ValidateXrTime(ConformanceHooksBase* conformanceHook, XrTime time, const char* valueName, const char* xrFunctionName);
void ValidateXrQuaternion(ConformanceHooksBase* conformanceHook, const XrQuaternionf& q, const char* valueName, const char* xrFunctionName);
void ValidateXrVector3f(ConformanceHooksBase* conformanceHook, const XrVector3f& v, const char* valueName, const char* xrFunctionName);

// clang-format off
#define ENUM_CASE_BOOL(name, val) case name: return true;
#define MAKE_IS_VALID_ENUM_VALUE(enumType, zeroIsValid) \
    inline bool is_valid_enum_val(enumType e) {         \
        if (!zeroIsValid && e == 0) return false;       \
        else if (e == 0x7FFFFFFF) return false;         \
                                                        \
        switch (e) {                                    \
            XR_LIST_ENUM_##enumType(ENUM_CASE_BOOL)     \
            default: return false;                      \
        }                                               \
    }
// clang-format on

MAKE_IS_VALID_ENUM_VALUE(XrSessionState, false);
MAKE_IS_VALID_ENUM_VALUE(XrReferenceSpaceType, false);
MAKE_IS_VALID_ENUM_VALUE(XrPerfSettingsDomainEXT, false);
MAKE_IS_VALID_ENUM_VALUE(XrPerfSettingsSubDomainEXT, false);
MAKE_IS_VALID_ENUM_VALUE(XrPerfSettingsNotificationLevelEXT, true);

template <typename TEnum>
void ValidateXrEnum(ConformanceHooksBase* conformanceHook, TEnum value, const char* valueName, const char* xrFunctionName)
{
    if (!is_valid_enum_val(value)) {
        conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrFunctionName,
                                            "%s is not a valid enum value: %d", valueName, value);
    }
}

//
// WARNING: The convenience macros below will log for __func__ and should only be used directly in the XR function directly.
//          These convenience macros use "this" (ConformanceHooksBase*).
//          The goal is to avoid attributing failure to "SomeInternalHelperFunc".
//
#define RUNTIME_FAILURE(severity, detailFmt, ...) this->ConformanceFailure(severity, __func__, detailFmt, ##__VA_ARGS__);

#define NONCONFORMANT(detailFmt, ...) RUNTIME_FAILURE(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, detailFmt, ##__VA_ARGS__);
#define POSSIBLE_NONCONFORMANT(detailFmt, ...) RUNTIME_FAILURE(XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, detailFmt, ##__VA_ARGS__);

#define NONCONFORMANT_IF(condition, detailFmt, ...)  \
    do {                                             \
        if (condition) {                             \
            NONCONFORMANT(detailFmt, ##__VA_ARGS__); \
        }                                            \
    } while (false)
#define POSSIBLE_NONCONFORMANT_IF(condition, detailFmt, ...)  \
    do {                                                      \
        if (condition) {                                      \
            POSSIBLE_NONCONFORMANT(detailFmt, ##__VA_ARGS__); \
        }                                                     \
    } while (false)

#define CREATE_STRUCT_CHAIN_VALIDATOR(parameter) XrBaseStructChainValidator(this, parameter, #parameter, __func__)
#define VALIDATE_STRUCT_CHAIN(parameter) const XrBaseStructChainValidator __chainValidator##parameter(this, parameter, #parameter, __func__)
#define VALIDATE_XRBOOL32(value) ValidateXrBool32(this, value, #value, __func__)
#define VALIDATE_FLOAT(value, min, max) ValidateFloat(this, value, min, max, #value, __func__)
#define VALIDATE_XRTIME(value) ValidateXrTime(this, value, #value, __func__)
#define VALIDATE_QUATERNION(value) ValidateXrQuaternion(this, value, #value, __func__)
#define VALIDATE_VECTOR3F(value) ValidateXrVector3f(this, value, #value, __func__)
#define VALIDATE_XRENUM(value) ValidateXrEnum(this, value, #value, __func__)
