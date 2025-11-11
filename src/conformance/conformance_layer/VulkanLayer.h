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

#pragma once

#include <vulkan/vulkan.h>

bool InstanceVkExtensionEnabled(VkInstance instance, const char *extension);

bool DeviceVkExtensionEnabled(VkDevice device, const char *extension);

// Resets the VkQueue access flag
void ResetVkQueueAccess(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex);

// Checks if a VkQueue has been accessed
bool CheckVkQueueAccess(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex);
