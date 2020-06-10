// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#include <catch2/catch.hpp>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string>
#include <set>
#include <vector>

namespace Conformance
{

    /// Custom matcher for use with ???_THAT() assertions, which takes a user-provided predicate and checks for at least one
    /// element in the collection for which this is true.
    template <typename T, typename ValType = typename T::value_type>
    class ContainsPredicate : public Catch::MatcherBase<T>
    {
    public:
        using ValueType = ValType;
        using PredicateType = std::function<bool(ValueType const&)>;
        ContainsPredicate(PredicateType&& predicate, const char* desc) : predicate_(std::move(predicate)), desc_(desc)
        {
        }

        virtual bool match(T const& container) const override
        {
            using std::begin;
            using std::end;
            return end(container) != std::find_if(begin(container), end(container), predicate_);
        }

        virtual std::string describe() const override
        {
            return std::string("contains an element such that ") + desc_;
        }

    private:
        PredicateType predicate_;
        const char* desc_;
    };

    template <typename T>
    using VectorContainsPredicate = ContainsPredicate<std::vector<T>>;

    /// Custom matcher for use with ???_THAT() assertions, which takes an initializer_list of permitted values and ensures
    /// the checked value is one of those.
    template <typename T>
    class In : public Catch::MatcherBase<T>
    {
    public:
        In(std::initializer_list<T> permittedValues) : permittedValues_(permittedValues)
        {
        }

        virtual bool match(T const& val) const override
        {
            using std::begin;
            using std::end;
            return end(permittedValues_) != std::find(begin(permittedValues_), end(permittedValues_), val);
        }

        virtual std::string describe() const override
        {
            std::ostringstream os;
            os << "is one of {";
            bool needComma = false;
            for (auto& val : permittedValues_) {
                if (needComma) {
                    os << ", ";
                }
                os << val;
                needComma = true;
            }
            os << "}";
            return os.str();
        }

    private:
        std::initializer_list<T> permittedValues_;
    };

    /// Custom matcher for use with ???_THAT() assertions, which ensures that the checked value (a fixed-length C string) is
    /// null terminated.
    template <size_t StringLength>
    class NullTerminated : public Catch::MatcherBase<char const (&)[StringLength]>
    {
    public:
        NullTerminated() = default;

        virtual bool match(char const (&str)[StringLength]) const override
        {
            using std::begin;
            using std::end;
            return end(str) != std::find(begin(str), end(str), '\0');
        }

        virtual std::string describe() const override
        {
            std::ostringstream os;
            os << "has a null-terminator within its fixed max length of " << StringLength;
            return os.str();
        }
    };

    template <size_t StringLength>
    static inline auto NullTerminatedInLength(char const (&)[StringLength]) -> NullTerminated<StringLength>
    {
        return {};
    }

    /// Custom matcher for vectors of values, to identify if there are any duplicates.
    template <typename ValueType>
    class VectorHasOnlyUniqueElements : public Catch::MatcherBase<std::vector<ValueType>>
    {
    public:
        VectorHasOnlyUniqueElements() = default;
        bool match(std::vector<ValueType> const& vec) const override
        {
            std::set<ValueType> s(vec.begin(), vec.end());
            return s.size() == vec.size();
        }
        std::string describe() const override
        {
            return "has only unique values.";
        }
    };

}  // namespace Conformance
