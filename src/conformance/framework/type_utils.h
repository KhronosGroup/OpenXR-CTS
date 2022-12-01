// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#include <tuple>
#include <utility>
#include <type_traits>

#include <stddef.h>

namespace Conformance
{
    namespace types
    {
        /// A simple type list template, used for very simple metaprogramming with parameter packs.
        template <typename... Types>
        struct List;

        namespace detail
        {
            template <typename T>
            struct is_tuple : std::false_type
            {
            };

            template <typename... Elements>
            struct is_tuple<std::tuple<Elements...>> : std::true_type
            {
            };

            template <typename T>
            struct is_tuple<const T> : is_tuple<T>
            {
            };

            template <typename T>
            struct is_tuple<T&> : is_tuple<T>
            {
            };
        }  // namespace detail

        /// returns true if the parameter is a std::tuple<...> type (after removing const and reference)
        template <typename T>
        static constexpr bool IsTuple()
        {
            return detail::is_tuple<T>{};
        }

    }  // namespace types

    namespace detail
    {
        template <typename Functor, typename Tuple, size_t... Indices>
        static inline void ForEachTupleElement_impl(Tuple&& t, Functor&& f, std::index_sequence<Indices...>)
        {
            // workaround for not having fold expressions, that nevertheless enforces evaluation order
            // https://codereview.stackexchange.com/questions/51407/stdtuple-foreach-implementation

            auto throwaway = {
                1,
                // this parenthesized expression is parameter-pack-expanded (repeated for each value in Indices)
                (f(std::get<Indices>(std::forward<Tuple>(t))) /* call f on an element */, void() /* defang return value of f */,
                 int{} /* default construct an int so this whole parens evaluates to an int */)...};
            (void)throwaway;
        }

    }  // namespace detail

    /// Calls a functor (class with templated operator(), or a generic lambda) on each element of a tuple.
    template <typename Functor, typename Tuple>
    static inline void ForEachTupleElement(Tuple&& t, Functor&& f)
    {
        static_assert(types::IsTuple<std::decay_t<Tuple>>(), "Can only call on a tuple");
        constexpr size_t size = std::tuple_size<std::decay_t<Tuple>>::value;
        detail::ForEachTupleElement_impl(std::forward<Tuple>(t), std::forward<Functor>(f), std::make_index_sequence<size>{});
    }

    namespace detail
    {
        template <typename Functor, typename Tuple, size_t... Indices>
        static inline void ForEachTupleElementAndIndex_impl(Tuple&& t, Functor&& f, std::index_sequence<Indices...>)
        {
            // Just a workaround for not having fold expressions, that nevertheless enforces evaluation order
            // https://codereview.stackexchange.com/questions/51407/stdtuple-foreach-implementation

            auto throwaway = {
                1,
                // this parenthesized expression is parameter-pack-expanded (repeated for each value in Indices)
                (f(std::get<Indices>(std::forward<Tuple>(t)), Indices) /* call f on an element and index */,
                 void() /* defang return value of f */, int{} /* default construct an int so this whole parens evaluates to an int */)...};
            (void)throwaway;
        }
    }  // namespace detail

    /// Calls a functor @p f (class with templated operator(), or a generic lambda) on each element of a tuple and its index as std::integral_constant<size_t, I>
    template <typename Functor, typename Tuple>
    static inline void ForEachTupleElementAndIndex(Tuple&& t, Functor&& f)
    {
        static_assert(types::IsTuple<std::decay_t<Tuple>>(), "Can only call on a tuple");
        constexpr size_t size = std::tuple_size<std::decay_t<Tuple>>::value;
        detail::ForEachTupleElementAndIndex_impl(std::forward<Tuple>(t), std::forward<Functor>(f), std::make_index_sequence<size>{});
    }

    namespace detail
    {
        template <typename F, typename Tuple, size_t... I>
        decltype(auto) TransformTuple_impl(F&& f, Tuple&& tuple, std::index_sequence<I...>)
        {
            return std::make_tuple(f(std::get<I>(std::forward<Tuple>(tuple)))...);
        }
    }  // namespace detail

    /// Make one tuple from another one, by transforming each element by applying functor @p f (class with templated operator(), or a generic lambda) to each element.
    template <typename F, typename Tuple>
    decltype(auto) TransformTuple(F&& f, Tuple&& t)
    {
        static_assert(types::IsTuple<std::decay_t<Tuple>>(), "Can only call on a tuple");
        constexpr size_t size = std::tuple_size<std::decay_t<Tuple>>::value;
        return detail::TransformTuple_impl(std::forward<F>(f), std::forward<Tuple>(t), std::make_index_sequence<size>{});
    }

    namespace detail
    {

        // Inspired by https://www.foonathan.net/2017/03/tuple-iterator/ and
        // https://accu.org/journals/overload/25/139/williams_2382/
        template <typename Tup, typename Ret, typename Conv, typename Indices = std::make_index_sequence<std::tuple_size<Tup>::value>>
        struct tuple_runtime_access_table;

        template <typename Tup, typename Ret, typename Conv, size_t... Indices>
        struct tuple_runtime_access_table<Tup, Ret, Conv, std::index_sequence<Indices...>>
        {
            using function_type = Ret (*)(Tup&, Conv&);

            template <size_t Idx>
            static Ret Access(Tup& tup, Conv& converter)
            {
                // we need this little trampoline function so that the converter functor can be passed as a parameter at runtime
                // which allows lambda capture, etc.
                return converter(std::get<Idx>(tup));
            }
            static constexpr std::array<function_type, std::tuple_size<Tup>::value> access_func_ptrs = {&Access<Indices>...};
        };

        // need this separate redeclare for pre-C++17 compilers
        template <typename Tup, typename Ret, typename Conv, size_t... Indices>
        constexpr std::array<Ret (*)(Tup&, Conv&), std::tuple_size<Tup>::value>
            tuple_runtime_access_table<Tup, Ret, Conv, std::index_sequence<Indices...>>::access_func_ptrs;

    }  // namespace detail

    /// Run-time tuple indexing, supporting a single return type
    ///
    /// @tparam Ret return type of @p converter
    ///
    /// Other type params are deduced automatically
    ///
    /// @param i runtime tuple element index
    /// @param tup a tuple
    /// @param converter A generic functor/lambda that returns @p Ret no matter which tuple element it is given.
    template <typename Ret, typename Tup, typename Conv>
    static inline Ret AccessTupleElement(size_t i, Tup& tup, Conv&& converter)
    {
        auto* func_ptr = detail::tuple_runtime_access_table<Tup, Ret, Conv>::access_func_ptrs[i];
        return (*func_ptr)(tup, converter);
    }
}  // namespace Conformance
