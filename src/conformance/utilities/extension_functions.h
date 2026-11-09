// Copyright (c) 2019-2026 The Khronos Group Inc.
// Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
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

#define CTS_DECLARE_FUNCTION_POINTER(name, ext) PFN_xr##name xr##name##_;
#define CTS_LOAD_FUNCTION_POINTER(name, ext) xr##name##_ = GetInstanceExtensionFunction<PFN_xr##name>(instance, "xr" #name);

// clang-format off

#define CTS_EXTENSION_DATA_FOR(ext)                             \
    struct ExtensionDataFor##ext                                \
    {                                                           \
        XR_LIST_FUNCTIONS_##ext(CTS_DECLARE_FUNCTION_POINTER)   \
                                                                \
        ExtensionDataFor##ext(XrInstance instance)              \
        {                                                       \
            XR_LIST_FUNCTIONS_##ext(CTS_LOAD_FUNCTION_POINTER)  \
        }                                                       \
    }

// clang-format on
