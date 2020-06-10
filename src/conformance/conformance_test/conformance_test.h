// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#include <openxr/openxr.h>
#include <openxr/openxr_platform_defines.h>

enum MessageType
{
    MessageType_Stdout,
    MessageType_Stderr,
    MessageType_TestSectionStarting,
    MessageType_AssertionFailed,
};

typedef void(XRAPI_PTR* PFN_Message)(MessageType type, const char* message);

struct ConformanceLaunchSettings
{
    int argc;
    const char** argv;

    PFN_Message message;
};

#if defined(__GNUC__) && __GNUC__ >= 4
#define CONFORMANCE_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define CONFORMANCE_EXPORT __attribute__((visibility("default")))
#else
#define CONFORMANCE_EXPORT __declspec(dllexport)
#endif

struct ConformanceTestCase
{
    char testName[256];
    char tags[256];
};

enum XrcResult
{
    XRC_SUCCESS = 0,
    XRC_ERROR_SIZE_INSUFFICIENT = -1,
    XRC_ERROR_COMMAND_LINE_INVALID = -2,
    XRC_ERROR_INITIALIZATION_FAILED = -3,
    XRC_ERROR_INTERNAL_ERROR = -4,
};

extern "C" CONFORMANCE_EXPORT XrcResult XRAPI_CALL xrcEnumerateTestCases(uint32_t capacityInput, uint32_t* countOutput,
                                                                         ConformanceTestCase* testCases);

// Returns failure count, including tests and initialization failure.
extern "C" CONFORMANCE_EXPORT XrcResult XRAPI_CALL xrcRunConformanceTests(const ConformanceLaunchSettings* conformanceLaunchSettings,
                                                                          uint32_t* failureCount);
