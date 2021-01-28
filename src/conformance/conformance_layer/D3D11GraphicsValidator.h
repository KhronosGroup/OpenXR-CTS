// Copyright (c) 2019-2021, The Khronos Group Inc.
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

struct IGraphicsValidator
{
    virtual ~IGraphicsValidator() = default;

    virtual void ValidateSwapchainFormats(uint32_t count, uint64_t* formats) = 0;
    virtual void ValidateSwapchainImageStructs(uint64_t swapchainFormat, uint32_t count, XrSwapchainImageBaseHeader* images) = 0;
};
