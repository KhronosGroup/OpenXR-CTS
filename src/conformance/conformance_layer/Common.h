// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <queue>
#include <deque>
#include <algorithm>
#include <chrono>
#include <iterator>
#include <type_traits>
#include <cstring>
#include <cmath>
#include <initializer_list>

#include "common/xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>

#include <xr_generated_dispatch_table.h>

// Macro to generate stringify functions for OpenXR enumerations based data provided in openxr_reflection.h
// clang-format off
#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType)                  \
    inline const char* to_string(enumType e) {         \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return "Unknown " #enumType;      \
        }                                              \
    }
// clang-format on

MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrStructureType);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrObjectType);

template <typename T, typename TSuper>
T* FindChainedXrStruct(TSuper* super, XrStructureType matchType)
{
    auto extension = reinterpret_cast<T*>(super);
    while (extension && extension->type != matchType) {
        extension = reinterpret_cast<T*>(extension->next);
    }
    return extension;
}

template <typename TCallback>
void ForEachExtension(const void* next, TCallback callback)
{
    for (const XrBaseInStructure* ext = reinterpret_cast<const XrBaseInStructure*>(next); ext != nullptr; ext = ext->next) {
        callback(ext);
    }
}

template <typename T>
bool ContainsDuplicates(const std::vector<T>& collection)
{
    std::set<T> unique(collection.begin(), collection.end());
    return unique.size() != collection.size();
}

inline bool IsValidXrBool32(XrBool32 value)
{
    return value == XR_TRUE || value == XR_FALSE;
}

inline bool IsUnitQuaternion(const XrQuaternionf& q, float* length)
{
    *length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return std::abs(1 - *length) < 0.000001f;
}

template <typename T>
class VectorInspection
{
public:
    using VectorType = std::vector<T>;
    VectorInspection(VectorType const& currentVector) : currentVector_(currentVector)
    {
        std::copy(currentVector_.begin(), currentVector_.end(), std::inserter(currentElements_, currentElements_.begin()));
    }

    bool ContainsDuplicates() const
    {
        return currentVector_.size() != currentElements_.size();
    }

    bool ContainsValue(T const& elt) const
    {
        auto it = currentElements_.find(elt);
        return it != currentElements_.end();
    }

    /// Compares the contents of vectors, ignoring order of elements.
    bool SameElementsAs(VectorType const& prevVector) const
    {
        if (currentVector_.size() != prevVector.size()) {
            return false;
        }
        for (const auto& elt : prevVector) {
            if (!ContainsValue(elt)) {
                return false;
            }
        }
        return true;
    }

    bool ContainsAnyNotIn(std::initializer_list<T> const& known)
    {
        auto b = known.begin();
        auto e = known.end();
        for (const auto& elt : currentElements_) {
            auto it = std::find(b, e, elt);
            if (it == e) {
                // current vec contains an element not found in the provided list
                return true;
            }
        }
        return false;
    }

private:
    using SetType = std::set<T>;
    VectorType const& currentVector_;
    SetType currentElements_;
};
