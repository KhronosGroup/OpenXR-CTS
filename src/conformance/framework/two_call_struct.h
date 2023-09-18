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

#include "type_utils.h"
#include "utilities/throw_helpers.h"
#include "hex_and_handles.h"

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
    namespace metadata
    {
        /// Data for an array (empty element, member object pointer to the array)
        /// @tparam StructType the type of the OpenXR structure containing the array
        /// @tparam Element the array value type
        template <typename StructType, typename Element>
        struct ArrayData;

        /// Capacity and count for a single array set
        /// @tparam StructType the type of the OpenXR structure containing the arrays and capacity
        template <typename StructType>
        struct CapacityInputCountOutput;

        /// Data for one or more arrays that share a single capacity/count.
        /// @tparam StructType the type of the OpenXR structure containing the arrays and capacity
        /// @tparam ArrayElementTypeList a types::List<> of element types.
        template <typename StructType, typename ArrayElementTypeList>
        struct ArraySetData;

        /// Data for a whole structure, with zero or more array sets.
        /// @tparam StructType the type of the OpenXR structure containing the arrays and capacities
        /// @tparam ArraySetsTypeList types::List<> with one entry for each set, where each entry is a types::List<> of element types.
        template <typename StructType, typename ArraySetsTypeList>
        struct TwoCallStructData;

        template <typename StructType, typename Element>
        struct ArrayData
        {
            using element_type = Element;
            using struct_type = StructType;

            const char* name;
            element_type* struct_type::*ptr;
            element_type empty;
        };

        /// Get the name of the array field
        template <typename StructType, typename Element>
        static inline const char* ArrayName(const ArrayData<StructType, Element>& data) noexcept
        {
            return data.name;
        }

        /// Get the array field
        template <typename StructType, typename Element>
        static inline Element* GetArray(const ArrayData<StructType, Element>& data, const StructType& s) noexcept
        {
            return s.*(data.ptr);
        }

        template <typename StructType>
        struct CapacityInputCountOutput
        {
            using struct_type = StructType;
            const char* capacityInputName;
            uint32_t struct_type::*capacityInputPtr;
            const char* countOutputName;
            uint32_t struct_type::*countOutputPtr;
        };

        template <typename StructType, typename... ArrayElementTypes>
        struct ArraySetData<StructType, types::List<ArrayElementTypes...>>
        {
            using struct_type = StructType;
            using tuple_type = std::tuple<ArrayData<struct_type, ArrayElementTypes>...>;
            using type_list = types::List<ArrayElementTypes...>;

            CapacityInputCountOutput<struct_type> capacityCount;

            tuple_type arrayDatas;

            template <typename Element>
            using append_array_t = ArraySetData<struct_type, types::List<ArrayElementTypes..., Element>>;

            /// Add another array's metadata, sharing the same capacity
            ///
            /// @param name The name of the field (string literal)
            /// @param ptr A pointer-to-member to the field
            /// @param empty The value that should be used to initialize array elements as "empty"
            template <typename Element>
            auto Array(const char* name, Element* struct_type::*ptr, const Element& empty) const -> append_array_t<Element>
            {
                return {capacityCount, std::tuple_cat(arrayDatas, std::tuple<ArrayData<StructType, Element>>{
                                                                      ArrayData<StructType, Element>{name, ptr, empty}})};
            }

            /// Add another array's metadata, sharing the same capacity, using default initialization
            ///
            /// @param name The name of the field (string literal)
            /// @param ptr A pointer-to-member to the field
            template <typename Element>
            auto Array(const char* name, Element* struct_type::*ptr) const -> append_array_t<Element>
            {
                return Array(name, ptr, Element{});
            }
        };

        template <typename StructType, typename... ArraySetLists>
        struct TwoCallStructData<StructType, types::List<ArraySetLists...>>
        {
            using struct_type = StructType;
            using tuple_type = std::tuple<ArraySetData<struct_type, ArraySetLists>...>;
            using type_list = types::List<ArraySetLists...>;

            struct_type empty;
            tuple_type arraySets;
        };

        template <typename StructType>
        static inline uint32_t CapacityInput(const CapacityInputCountOutput<StructType>& capacityCount,
                                             const StructType& twoCallStruct) noexcept
        {
            return twoCallStruct.*(capacityCount.capacityInputPtr);
        }

        template <typename StructType>
        static inline uint32_t& CapacityInput(const CapacityInputCountOutput<StructType>& capacityCount, StructType& twoCallStruct) noexcept
        {
            return twoCallStruct.*(capacityCount.capacityInputPtr);
        }

        template <typename StructType>
        static inline uint32_t CountOutput(const CapacityInputCountOutput<StructType>& capacityCount,
                                           const StructType& twoCallStruct) noexcept
        {
            return twoCallStruct.*(capacityCount.countOutputPtr);
        }

        template <typename StructType, typename ArrayElementTypeList>
        static inline uint32_t CountOutput(const ArraySetData<StructType, ArrayElementTypeList>& data,
                                           const StructType& twoCallStruct) noexcept
        {
            return CountOutput(data.capacityCount, twoCallStruct);
        }

        /// Get the name of the xCountOutput field
        template <typename StructType, typename ArrayElementTypeList>
        static inline const char* CountOutputName(const ArraySetData<StructType, ArrayElementTypeList>& data) noexcept
        {
            return data.capacityCount.countOutputName;
        }

        /// Get the name of the xCapacityInput field
        template <typename StructType, typename ArrayElementTypeList>
        static inline const char* CapacityInputName(const ArraySetData<StructType, ArrayElementTypeList>& data) noexcept
        {
            return data.capacityCount.capacityInputName;
        }

        /// Get the capacity input and count output data for array set index @p arraySetIndex in the two-call data @p data
        template <typename StructType, typename ArraySetsTypeList>
        const CapacityInputCountOutput<StructType>& CapacityCountData(size_t arraySetIndex,
                                                                      const TwoCallStructData<StructType, ArraySetsTypeList>& data)
        {
            using return_type = const CapacityInputCountOutput<StructType>&;
            return AccessTupleElement<return_type>(arraySetIndex, data.arraySets,
                                                   [](const auto& arraySetData) -> return_type { return arraySetData.capacityCount; });
        }

        /// Get the capacity input for array set index @p arraySetIndex in the two-call structure @p s described by @p data
        template <typename StructType, typename ArraySetsTypeList>
        uint32_t CapacityInput(size_t arraySetIndex, const TwoCallStructData<StructType, ArraySetsTypeList>& data, const StructType& s)
        {
            return CapacityInput(CapacityCountData(arraySetIndex, data), s);
        }
        /// Get a reference to the capacity input for array set index @p arraySetIndex in the two-call structure @p s described by @p data
        template <typename StructType, typename ArraySetsTypeList>
        uint32_t& CapacityInput(size_t arraySetIndex, const TwoCallStructData<StructType, ArraySetsTypeList>& data, StructType& s)
        {
            return CapacityInput(CapacityCountData(arraySetIndex, data), s);
        }

        /// Get the count output for array set index @p arraySetIndex in the two-call structure @p s described by @p data
        template <typename StructType, typename ArraySetsTypeList>
        uint32_t CountOutput(size_t arraySetIndex, const TwoCallStructData<StructType, ArraySetsTypeList>& data, const StructType& s)
        {
            return CountOutput(CapacityCountData(arraySetIndex, data), s);
        }

        /// Get the name of array index @p arrayIndex in the array set @p data
        template <typename StructType, typename ArrayElementTypeList>
        const char* ArrayName(size_t arrayIndex, const ArraySetData<StructType, ArrayElementTypeList>& data)
        {
            return AccessTupleElement<const char*>(arrayIndex, data.arrayDatas, [](const auto& arrayData) { return arrayData.name; });
        }

        /// Clear the array pointer in @p s for all fields in this array set @p data
        template <typename StructType, typename ArrayElementTypeList>
        void ClearArrayFields(const ArraySetData<StructType, ArrayElementTypeList>& data, StructType& s)
        {
            ForEachTupleElement(data.arrayDatas, [&](auto& arrayData) { s.*(arrayData.ptr) = nullptr; });
        }

        /// Clear the array pointer in @p s for all fields in array set index @p arraySetIdx in struct described by @p data
        template <typename StructType, typename ArraySetsTypeList>
        void ClearArrayFields(size_t arraySetIdx, const TwoCallStructData<StructType, ArraySetsTypeList>& data, StructType& s)
        {
            ForEachTupleElementAndIndex(data.arraySets, [&](auto& arraySetData, size_t i) {
                if (i == arraySetIdx) {
                    ClearArrayFields(arraySetData, s);
                }
            });
        }

        template <typename StructType, typename Element>
        static inline void Describe(std::ostream& os, const ArrayData<StructType, Element>& data, const StructType& twoCallStruct)
        {
            os << data.name;
            auto arrayField = GetArray(data, twoCallStruct);
            if (arrayField == nullptr) {
                os << " = nullptr\n";
            }
            else {
                os << " = " << to_hex(arrayField) << "\n";
            }
        }

        template <typename StructType, typename ArrayElementTypeList>
        static inline void Describe(std::ostream& os, const ArraySetData<StructType, ArrayElementTypeList>& data,
                                    const StructType& twoCallStruct)
        {
            os << CapacityInputName(data) << " = " << CapacityInput(data.capacityCount, twoCallStruct) << "\n";
            os << CountOutputName(data) << " = " << CountOutput(data.capacityCount, twoCallStruct) << "\n";
            ForEachTupleElement(data.arrayDatas, [&](auto& arrayData) { Describe(os, arrayData, twoCallStruct); });
        }

        /// Stream a string describing the contents of a struct.
        template <typename StructType, typename ArraySetsTypeList>
        static inline void Describe(std::ostream& os, const TwoCallStructData<StructType, ArraySetsTypeList>& data,
                                    const StructType& twoCallStruct)
        {
            ForEachTupleElement(data.arraySets, [&](auto& arraySetData) { Describe(os, arraySetData, twoCallStruct); });
        }

        /// Get a string describing the contents of a struct.
        template <typename StructType, typename ArraySetsTypeList>
        static inline std::string Describe(const TwoCallStructData<StructType, ArraySetsTypeList>& data, const StructType& twoCallStruct)
        {
            std::ostringstream os;
            Describe(os, data, twoCallStruct);
            return os.str();
        }
    }  // namespace metadata

    /**
     * Begin constructing metadata for one or more arrays sharing a single capacity/count (an "array set") in a two-call-idiom structure.
     *
     * For the paired "name, member object pointer" parameters, it's recommended to use the `NAME_AND_MEMPTR()` macro to automatically stringify the member name.
     *
     * This returns data for an "array set" with no arrays: build up metadata for the arrays by repeatedly calling the member function `Array()` in a chained or "builder" pattern.
     *
     * @tparam StructType Deduced from the provided member object pointers.
     *
     * @param capacityInputName The name of the `*CapacityInput` field
     * @param capacityInputFieldPointer A member object pointer to the `*CapacityInput` field
     * @param countOutputName The name of the `*CountOutput` field
     * @param countOutputFieldPointer A member object pointer to the `*CountOutput` field
     *
     * @return An instance of some metadata::ArraySetData<> type
     *
     * @ingroup TwoCallStructMetadata
     */
    template <typename StructType>
    static constexpr inline metadata::ArraySetData<StructType, types::List<>>
    CapacityInputCountOutput(const char* capacityInputName, uint32_t StructType::*capacityInputFieldPointer, const char* countOutputName,
                             uint32_t StructType::*countOutputFieldPointer)
    {
        return {metadata::CapacityInputCountOutput<StructType>{capacityInputName, capacityInputFieldPointer, countOutputName,
                                                               countOutputFieldPointer},
                {}};
    }

    /**
     * Create the metadata for a two-call-struct.
     *
     * @param empty An empty instance of your two-call-struct type
     * @param a one or more @ref metadata::ArraySetData objects.
     * Each may be created by calling @ref CapacityInputCountOutput() then (repeatedly) calling the member function `Array()` in a chained or "builder" pattern.
     *
     * @tparam StructType The structure type, deduced based on @p empty
     * @tparam ArraySets Deduced based on parameters
     *
     * @return An object of some metadata::ArraySetData<> type containing (in its type and runtime data) all constant information about two-call-idiom usage with this structure type.
     *
     * @ingroup TwoCallStructMetadata
     */
    template <typename StructType, typename... ArraySets>
    static constexpr inline auto TwoCallStruct(const StructType& empty, ArraySets... a)
    {
        using array_sets_type_list = types::List<typename ArraySets::type_list...>;
        return metadata::TwoCallStructData<StructType, array_sets_type_list>{empty, std::make_tuple(std::forward<ArraySets>(a)...)};
    }

    namespace storage
    {
        template <typename StructType, typename ArrayElementTypeList>
        struct ArraySetStorage;

        template <typename StructType, typename ArraySetsTypeList>
        struct TwoCallStructStorage;

        /// Storage for an array
        /// @tparam StructType the type of the OpenXR structure containing the array
        /// @tparam Element the array value type
        template <typename StructType, typename Element>
        struct ArrayStorage
        {
            using metadata_type = metadata::ArrayData<StructType, Element>;
            using element_type = Element;
            using struct_type = StructType;

            /// Reference to our metadata
            const metadata_type& data;

            /// The array we'll use as backing storage for calls
            std::vector<element_type> array;

            /// Allocate space for @p count elements in our array
            void Allocate(uint32_t count)
            {
                array.clear();
                array.resize(count, data.empty);
            }

            /// Populate the corresponding pointer in @p twoCallStruct with our array pointer
            void AssignArrayField(struct_type& twoCallStruct)
            {
                twoCallStruct.*(data.ptr) = array.data();
            }
        };

        /// Get the name of the array field
        template <typename StructType, typename Element>
        static inline const char* ArrayName(const ArrayStorage<StructType, Element>& storage) noexcept
        {
            return ArrayName(storage.data);
        }

        namespace detail
        {
            // we need to pack-expand the list, wrapping each element, to make a tuple type
            template <typename StructType, typename L>
            struct MakeArraySetStorageTupleType;
            template <typename StructType, typename... ArrayElementType>
            struct MakeArraySetStorageTupleType<StructType, types::List<ArrayElementType...>>
            {
                using type = std::tuple<ArrayStorage<StructType, ArrayElementType>...>;
            };
        }  // namespace detail

        /// A collection of ArrayStorage.
        /// @tparam StructType the type of the OpenXR structure containing the arrays and capacity
        /// @tparam ArrayElementTypeList a types::List<> of element types.
        template <typename StructType, typename ArrayElementTypeList>
        struct ArraySetStorage
        {
            using struct_type = StructType;
            using metadata_type = metadata::ArraySetData<StructType, ArrayElementTypeList>;
            using tuple_type = typename detail::MakeArraySetStorageTupleType<StructType, ArrayElementTypeList>::type;

            const metadata_type& data;
            tuple_type arrayStorages;

            explicit constexpr ArraySetStorage(const metadata_type& data_) noexcept;
        };

        /// Get the name of the xCountOutput field
        template <typename StructType, typename ArrayElementTypeList>
        static inline const char* CountOutputName(const ArraySetStorage<StructType, ArrayElementTypeList>& storage) noexcept
        {
            return CountOutputName(storage.data);
        }

        /// Get the name of the xCapacityInput field
        template <typename StructType, typename ArrayElementTypeList>
        static inline const char* CapacityInputName(const ArraySetStorage<StructType, ArrayElementTypeList>& storage) noexcept
        {
            return CapacityInputName(storage.data);
        }

        namespace detail
        {
            // we need to pack-expand the list, wrapping each element, to make a tuple type
            template <typename StructType, typename L>
            struct MakeTwoCallStructStorageTupleType;
            template <typename StructType, typename... ArrayElementTypeList>
            struct MakeTwoCallStructStorageTupleType<StructType, types::List<ArrayElementTypeList...>>
            {
                using type = std::tuple<ArraySetStorage<StructType, ArrayElementTypeList>...>;
            };
        }  // namespace detail

        /// A collection of ArraySetStorage for a whole structure, with zero or more array sets.
        /// @tparam StructType the type of the OpenXR structure containing the arrays and capacities
        /// @tparam ArraySetsTypeList types::List<> with one entry for each set, where each entry is a types::List<> of element types.
        template <typename StructType, typename ArraySetsTypeList>
        struct TwoCallStructStorage
        {
            using metadata_type = metadata::TwoCallStructData<StructType, ArraySetsTypeList>;
            using struct_type = StructType;
            using tuple_type = typename detail::MakeTwoCallStructStorageTupleType<StructType, ArraySetsTypeList>::type;
            static_assert(types::IsTuple<tuple_type>(), "if this fails there's an internal error in MakeTwoCallStructStorageTupleType");
            const metadata_type& data;
            tuple_type arraySetStorages;

            explicit constexpr TwoCallStructStorage(const metadata_type& metadata) noexcept;

            /// Clear all arrays
            void Clear();
        };

        /// Make an ArraySetStorage object from a corresponding ArraySetData object.
        /// Function exists for type deduction.
        template <typename StructType, typename ArrayElementTypeList>
        static constexpr inline storage::ArraySetStorage<StructType, ArrayElementTypeList>
        MakeArraySetStorage(const metadata::ArraySetData<StructType, ArrayElementTypeList>& data) noexcept
        {
            return storage::ArraySetStorage<StructType, ArrayElementTypeList>{data};
        }

        /// Make an ArrayStorage object from a corresponding ArrayData object.
        /// Function exists for type deduction.
        template <typename StructType, typename Element>
        static constexpr inline storage::ArrayStorage<StructType, Element>
        MakeArrayStorage(const metadata::ArrayData<StructType, Element>& data) noexcept
        {
            return storage::ArrayStorage<StructType, Element>{data};
        }

        template <typename StructType, typename ArraySetsTypeList>
        inline constexpr TwoCallStructStorage<StructType, ArraySetsTypeList>::TwoCallStructStorage(const metadata_type& metadata) noexcept
            : data(metadata), arraySetStorages(TransformTuple([](const auto& d) { return MakeArraySetStorage(d); }, metadata.arraySets))
        {
        }

        template <typename StructType, typename ArraySetsTypeList>
        inline void TwoCallStructStorage<StructType, ArraySetsTypeList>::Clear()
        {
            ForEachTupleElement(arraySetStorages, [&](auto& arraySetStorage) {
                ForEachTupleElement(arraySetStorage.arrayStorages, [](auto& arrayStorage) { arrayStorage.array.clear(); });
            });
        }

        template <typename StructType, typename ArrayElementTypeList>
        inline constexpr ArraySetStorage<StructType, ArrayElementTypeList>::ArraySetStorage(const metadata_type& data_) noexcept
            : data(data_), arrayStorages(TransformTuple([](const auto& d) { return MakeArrayStorage(d); }, data_.arrayDatas))
        {
        }

    }  // namespace storage

    /// Make a TwoCallStructStorage object from a TwoCallStructData object.
    /// Function exists for type deduction.
    template <typename StructType, typename ArraySetsTypeList>
    static constexpr inline storage::TwoCallStructStorage<StructType, ArraySetsTypeList>
    MakeTwoCallStructStorage(const metadata::TwoCallStructData<StructType, ArraySetsTypeList>& metadata)
    {
        return storage::TwoCallStructStorage<StructType, ArraySetsTypeList>{metadata};
    }

}  // namespace Conformance
