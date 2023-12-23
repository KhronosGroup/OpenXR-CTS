// Copyright (c) 2019-2023, The Khronos Group Inc.
// Copyright (c) 2019 Collabora, Ltd.
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

#include "two_call_struct.h"
#include "utilities/throw_helpers.h"
#include "common/hex_and_handles.h"

#include <openxr/openxr.h>

#include <vector>
#include <tuple>
#include <type_traits>
#include <functional>
#include <sstream>

#include <stdint.h>
#include <catch2/catch_test_macros.hpp>

namespace Conformance
{
    namespace detail
    {
        // Avoid having to repeat these types for every single function
        template <typename StructType, typename ArraySetsTypeList>
        struct TwoCallStructSubtests
        {
            using StructStorage = storage::TwoCallStructStorage<StructType, ArraySetsTypeList>;

            static constexpr size_t kNumberOfArraySets = std::tuple_size<typename StructStorage::tuple_type>();

            /// Allocate all arrays to the full requested size from @p structWithCounts and assign all pointer fields and capacities in the returned struct.
            ///
            /// @param twoCallStorage the @ref storage::TwoCallStructStorage instance containing the backing vectors (cleared by this call)
            /// @param twoCallStruct the structure you want modified and returned
            /// @param structWithCounts a structure whose *CountOutput fields are populated.
            ///
            /// @return @p twoCallStruct with the array fields populated with pointers to correctly-allocated vectors,
            /// and the capacities set to the provided counts
            static StructType FullyAllocateAndAssign(StructStorage& twoCallStorage, StructType twoCallStruct,
                                                     const StructType& structWithCounts)
            {
                twoCallStorage.Clear();
                ForEachTupleElement(twoCallStorage.arraySetStorages, [&](auto& arraySetStorage) {
                    uint32_t count = CountOutput(arraySetStorage.data.capacityCount, structWithCounts);
                    if (count > 0) {
                        CapacityInput(arraySetStorage.data.capacityCount, twoCallStruct) = count;
                        ForEachTupleElement(arraySetStorage.arrayStorages, [&](auto& arrayStorage) {
                            arrayStorage.Allocate(count);
                            arrayStorage.AssignArrayField(twoCallStruct);
                        });
                    }
                });
                return twoCallStruct;
            }

            /// Check what happens if we make a specified array set's capacity 0.
            /// Called in a loop by @ref CheckSingleZero to test each array set in turn
            template <typename F>
            static void CheckSpecificSingleZero(StructStorage& twoCallStorage, const StructType& emptyStruct,
                                                const StructType& structWithCounts, size_t setIndexOfZeroCapacity, const char* functionName,
                                                F&& doCall)
            {

                auto& capacityAndCountData = CapacityCountData(setIndexOfZeroCapacity, twoCallStorage.data);
                auto zeroedCapacityInputName = capacityAndCountData.capacityInputName;

                // Skip the rest of this test if this array set is not suitable for testing setting capacity to 0 rather than the requested count
                if (CountOutput(capacityAndCountData, structWithCounts) == 0) {
                    WARN("Cannot try a count-request call with 0 in " << zeroedCapacityInputName << " because we got 0 in "
                                                                      << capacityAndCountData.countOutputName);
                    return;
                }
                bool haveAnyOthersTwoPlus = false;
                for (size_t i = 0; i < kNumberOfArraySets; ++i) {
                    if (setIndexOfZeroCapacity != i && CountOutput(i, twoCallStorage.data, structWithCounts) > 1) {
                        haveAnyOthersTwoPlus = true;
                        break;
                    }
                }
                if (!haveAnyOthersTwoPlus) {
                    WARN("Cannot try a count-request call with 0 in " << zeroedCapacityInputName
                                                                      << " because no other counts got a value larger than 1");
                    return;
                }

                // The actual calling and verification of return result. We try several scenarios in this function.
                auto checkCall = [&](StructType twoCallStruct) {
                    INFO(Describe(twoCallStorage.data, twoCallStruct));
                    INFO("Expect XR_SUCCESS since " << zeroedCapacityInputName
                                                    << " = 0 means it should be treated like all capacities are 0.");
                    XrResult result = doCall(&twoCallStruct);
                    INFO("Result of " << functionName << " was " << result << " [" << ResultToString(result) << "]");

                    XRC_CHECK_THROW_XRRESULT(result, functionName);
                };

                INFO("Setting array set " << setIndexOfZeroCapacity << " " << zeroedCapacityInputName << " to 0");

                /// One capacity is zero, others are all sufficient
                {
                    INFO("Setting other array sets to sufficient capacity");

                    StructType s = FullyAllocateAndAssign(twoCallStorage, emptyStruct, structWithCounts);
                    CapacityInput(capacityAndCountData, s) = 0;
                    ClearArrayFields(setIndexOfZeroCapacity, twoCallStorage.data, s);
                    checkCall(s);
                }

                /// One capacity is zero, others are insufficient-but-nonzero if possible
                /// The zero-ness should be handled first, with the insufficient-ness of the others ignored.
                {
                    INFO("Setting other array sets to insufficient but non-zero capacity");

                    StructType s = FullyAllocateAndAssign(twoCallStorage, emptyStruct, structWithCounts);
                    CapacityInput(capacityAndCountData, s) = 0;
                    ClearArrayFields(setIndexOfZeroCapacity, twoCallStorage.data, s);
                    std::ostringstream os;
                    for (size_t i = 0; i < kNumberOfArraySets; ++i) {
                        if (i == setIndexOfZeroCapacity) {
                            // skip the one we already zeroed
                            continue;
                        }
                        uint32_t count = CountOutput(i, twoCallStorage.data, structWithCounts);
                        if (count > 1) {
                            uint32_t new_capacity = count - 1;
                            os << "Reducing " << CapacityCountData(i, twoCallStorage.data).capacityInputName << " to " << new_capacity
                               << " - should be ignored since " << zeroedCapacityInputName << " is 0\n";
                            CapacityInput(i, twoCallStorage.data, s) = new_capacity;
                        }
                    }
                    INFO(os.str());

                    checkCall(s);
                }
            }

            /// Check that having any one CapacityInput equal to 0 is treated as if they all were 0, by trying each capacity/array set in turn
            template <typename F>
            static void CheckSingleZero(StructStorage& twoCallStorage, const StructType& emptyStruct, const StructType& structWithCounts,
                                        const char* functionName, F&& doCall)
            {
                // Any 0 capacity is as if all were 0
                // set one capacity to zero, we should succeed.
                INFO("Check that setting any one CapacityInput to 0 is treated as if all were 0");
                for (size_t i = 0; i < kNumberOfArraySets; ++i) {
                    twoCallStorage.Clear();
                    CheckSpecificSingleZero(twoCallStorage, emptyStruct, structWithCounts, i, functionName, std::forward<F>(doCall));
                }
            }

            /// Check what happens if we make a specified array set's capacity insufficient but non-zero (with the others sufficient).
            /// Called in a loop by @ref CheckInsufficientCapacity to test each array set in turn
            template <typename F>
            static void CheckSpecificInsufficientCapacity(StructStorage& twoCallStorage, const StructType& emptyStruct,
                                                          const StructType& structWithCounts, size_t insufficientArraySetIndex, F&& doCall)
            {
                twoCallStorage.Clear();
                auto& capacityAndCountData = CapacityCountData(insufficientArraySetIndex, twoCallStorage.data);
                auto reducedCapacityInputName = capacityAndCountData.capacityInputName;
                uint32_t requestedSize = CountOutput(capacityAndCountData, structWithCounts);
                if (requestedSize <= 1) {
                    WARN("Cannot test XR_ERROR_SIZE_INSUFFICIENT for " << reducedCapacityInputName << " because we got <= 1 in "
                                                                       << capacityAndCountData.countOutputName);
                    return;
                }

                StructType s = FullyAllocateAndAssign(twoCallStorage, emptyStruct, structWithCounts);
                uint32_t newCapacity = requestedSize - 1;
                CapacityInput(capacityAndCountData, s) = newCapacity;

                INFO("Reduced " << reducedCapacityInputName << " to " << newCapacity << " to trigger XR_ERROR_SIZE_INSUFFICIENT");

                INFO(Describe(twoCallStorage.data, s));

                XrResult result = doCall(&s);
                XRC_CHECK_THROW_MSG(XR_ERROR_SIZE_INSUFFICIENT == result,
                                    std::string("Expected XR_ERROR_SIZE_INSUFFICIENT but got ") + ResultToString(result));
            }

            /// Check that having any one array set's capacity insufficient but non-zero triggers SIZE_INSUFFICIENT,
            /// by making each array set insufficient in turn.
            template <typename F>
            static void CheckInsufficientCapacity(StructStorage& twoCallStorage, const StructType& emptyStruct,
                                                  const StructType& structWithCounts, F&& doCall)
            {
                // Any 0 count is as if all were 0
                // set one capacity to zero, set others to too-small.
                // countOutputs should be full and we should succeed.
                INFO("Check that reducing any one CapacityInput (>1) to a non-zero value is XR_ERROR_SIZE_INSUFFICIENT");

                for (size_t i = 0; i < kNumberOfArraySets; ++i) {
                    CheckSpecificInsufficientCapacity(twoCallStorage, emptyStruct, structWithCounts, i, std::forward<F>(doCall));
                }
            }
        };
    }  // namespace detail

    /**
     * Automatically check for conformant behavior of a function that uses the two-call idiom with a struct
     *
     * @param twoCallData The data describing the two-call-struct: typically placed in an overload of `getTwoCallStructData()` for modularity and reusability.
     * @param emptyStruct The empty struct you want to start with when creating copies of the struct for tests. Must be at least zeroed with the type and next set appropriately.
     * @param functionName A string literal for the function name you call in @p doCall
     * @param emptyIsError If we should error out in case we receive an empty enumeration
     * @param doCall A functor that takes a pointer to your two-call-struct type and returns XrResult.
     *
     * @tparam StructType Automatically deduced
     * @tparam ArraySetsTypeList Automatically deduced from your metadata::TwoCallStructData
     * @tparam F Automatically deduced from your functor you provide
     */
    template <typename StructType, typename ArraySetsTypeList, typename F>
    static inline void CheckTwoCallStructConformance(const metadata::TwoCallStructData<StructType, ArraySetsTypeList>& twoCallData,
                                                     const StructType& emptyStruct, const char* functionName, bool emptyIsError, F&& doCall)
    {
        INFO("Two-call idiom checking, structure-style");

        using Subtests = detail::TwoCallStructSubtests<StructType, ArraySetsTypeList>;

        StructType structWithCounts = emptyStruct;
        auto twoCallStorage = MakeTwoCallStructStorage(twoCallData);

        // Condition 1 - normal first call
        {
            INFO("Check normal count-getting behavior, empty struct");
            StructType s = emptyStruct;
            XrResult result = doCall(&s);
            XRC_CHECK_THROW_XRRESULT(result, functionName);
            if (XR_SUCCESS != result) {
                return;
            }
            structWithCounts = s;
        }

        // Make sure we're enumerating things at all
        bool allZero = true;
        for (size_t i = 0; i < Subtests::kNumberOfArraySets; ++i) {
            if (CountOutput(i, twoCallData, structWithCounts) != 0) {
                allZero = false;
                break;
            }
        }

        if (allZero) {
            if (emptyIsError) {
                FAIL("Could not fully test two-call structure conformance, all xCountOutput fields were 0 after a call:\n"
                     << Describe(twoCallData, structWithCounts));
            }
            else {
                WARN("Could not fully test two-call structure conformance, all xCountOutput fields were 0 after a call:\n"
                     << Describe(twoCallData, structWithCounts));
            }
            return;
        }

        // Condition 2 - normal second call (full, sufficient allocations)
        {
            INFO("Allocate exactly what was asked for");

            StructType s = Subtests::FullyAllocateAndAssign(twoCallStorage, emptyStruct, structWithCounts);
            INFO(Describe(twoCallStorage.data, s));
            XRC_CHECK_THROW_XRRESULT(doCall(&s), functionName);
        }

        // Condition 3 - at least one capacity is non-zero but insufficient, while other capacities (if any) are sufficient
        Subtests::CheckInsufficientCapacity(twoCallStorage, emptyStruct, structWithCounts, std::forward<F>(doCall));

        // Condition 4 - one capacity is 0, so the runtime should act as if all capacities were 0
        Subtests::CheckSingleZero(twoCallStorage, emptyStruct, structWithCounts, functionName, std::forward<F>(doCall));
    }

}  // namespace Conformance
