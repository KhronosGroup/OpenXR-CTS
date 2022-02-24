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

#include <sstream>
#include <iostream>
#include <cmath>
#include <stdarg.h>
#include "Common.h"
#include "ConformanceHooks.h"
#include "RuntimeFailure.h"

namespace
{
    void RuntimeFailure(const XrGeneratedDispatchTable* dispatchTable, XrInstance instance, XrDebugUtilsMessageSeverityFlagsEXT severity,
                        const char* xrFuncName, const char* detailsFmt, va_list vl)
    {
        std::string detailsStr;
        {
            va_list vl2;
            va_copy(vl2, vl);
            int size = std::vsnprintf(nullptr, 0, detailsFmt, vl2);
            va_end(vl2);

            if (size != -1) {
                std::unique_ptr<char[]> buffer(new char[size + 1]);

                va_copy(vl2, vl);
                size = std::vsnprintf(buffer.get(), size + 1, detailsFmt, vl2);
                va_end(vl2);
                if (size != -1) {
                    detailsStr = std::string(buffer.get(), size);
                }
            }
        }

        std::stringstream ss;
        ss << "[" << xrFuncName << "]:" << detailsStr << std::endl;
        const std::string directMsg = ss.str();

#ifdef XR_USE_PLATFORM_WIN32
        OutputDebugStringA(directMsg.c_str());
#endif

        std::cerr << directMsg << std::endl;

        XrDebugUtilsMessengerCallbackDataEXT callbackData{XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
        callbackData.functionName = xrFuncName;
        callbackData.message = detailsStr.c_str();
        callbackData.messageId = "CONF";  // TODO: Technically should have a distinct ID per message.

        dispatchTable->SubmitDebugUtilsMessageEXT(instance, severity, XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT, &callbackData);

        if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
#if !defined(NDEBUG)
#ifdef _MSC_VER
            if (::IsDebuggerPresent()) {
                __debugbreak();
            }
#endif
#endif
        }
    }
}  // namespace

// Callback from the auto-generated conformance layer.
void ConformanceHooks::ConformanceFailure(XrDebugUtilsMessageSeverityFlagsEXT severity, const char* functionName, const char* fmtMessage,
                                          ...)
{
    va_list vl;
    va_start(vl, fmtMessage);
    RuntimeFailure(&this->dispatchTable, this->instance, severity, functionName, fmtMessage, vl);
    va_end(vl);
}

XrBaseStructChainValidator::XrBaseStructChainValidator(ConformanceHooksBase* conformanceHook, const void* arg, std::string parameterName,
                                                       std::string functionName)
    : m_conformanceHook(conformanceHook)
    , m_parameterName(std::move(parameterName))
    , m_functionName(std::move(functionName))
    , m_head(reinterpret_cast<const XrBaseInStructure*>(arg))
{
    for (const XrBaseInStructure* baseArg = m_head; baseArg != nullptr; baseArg = baseArg->next) {
        m_chainCache.push_back(*baseArg);
    }
}

XrBaseStructChainValidator::~XrBaseStructChainValidator()
{
    for (const XrBaseInStructure* baseArg = m_head; baseArg != nullptr; baseArg = baseArg->next) {
        if (m_chainCache.empty()) {
            m_conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, m_functionName.c_str(),
                                                  "Parameter %s next chain was lengthened", m_parameterName.c_str());
            continue;
        }
        const XrBaseInStructure expected = m_chainCache.front();
        m_chainCache.pop_front();
        if (expected.type != baseArg->type) {
            m_conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, m_functionName.c_str(),
                                                  "Struct 'type' modified for parameter %s or chained structure", m_parameterName.c_str());
        }
        if (expected.next != baseArg->next) {
            m_conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, m_functionName.c_str(),
                                                  "Struct 'next' chain modified for parameter %s or chained structure",
                                                  m_parameterName.c_str());
        }
    }
}

void ValidateXrBool32(ConformanceHooksBase* conformanceHook, XrBool32 value, const char* valueName, const char* xrFunctionName)
{
    if (!IsValidXrBool32(value)) {
        conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrFunctionName,
                                            "%s is not a valid XrBool32 value: %d", valueName, value);
    }
}

void ValidateFloat(ConformanceHooksBase* conformanceHook, float value, float min, float max, const char* valueName,
                   const char* xrFunctionName)
{
    if (value < min || value > max) {
        conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrFunctionName,
                                            "%s float value is out of range [%f, %f]: %f", valueName, min, max, value);
    }
}

void ValidateXrTime(ConformanceHooksBase* conformanceHook, XrTime time, const char* valueName, const char* xrFunctionName)
{
    // TODO: The spec does not disallow this.
    if (time < 0) {
        conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrFunctionName,
                                            "%s is not a valid XrTime value: %lld", valueName, time);
    }
}

void ValidateXrQuaternion(ConformanceHooksBase* conformanceHook, const XrQuaternionf& q, const char* valueName, const char* xrFunctionName)
{
    float length;
    if (!IsUnitQuaternion(q, &length)) {
        conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrFunctionName,
                                            "%s is not a unit quaternion value: (%f, %f, %f, %f) has length %f", valueName, q.x, q.y, q.z,
                                            q.w, length);
    }
}

void ValidateXrVector3f(ConformanceHooksBase* conformanceHook, const XrVector3f& v, const char* valueName, const char* xrFunctionName)
{
    auto isValidFloat = [](float v) { return std::isfinite(v); };

    if (!isValidFloat(v.x) || !isValidFloat(v.y) || !isValidFloat(v.z)) {
        conformanceHook->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrFunctionName,
                                            "%s is not a valid XrVector3d value: (%f, %f, %f)", valueName, v.x, v.y, v.z);
    }
}
