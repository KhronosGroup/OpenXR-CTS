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

#include <openxr/openxr.h>

#include <catch2/catch.hpp>

#include "conformance_framework.h"

#include <string>
#include <vector>

// Unix and Windows versions of clang-format act differently for this header, so disable it.
// clang-format off

namespace Conformance
{

    /// Namespace of implementation details
    namespace twocallimpl
    {

        /// Class turning a variadic list of string literals (each argument to a CHECK_TWO_CALL macro) into the basic pieces
        /// used in the two call checker assertion messages.
        struct Strings
        {
            template <typename... Args>
            Strings(const char* typeName, const char* emptyInitializer, const char* callName, Args... a)
            {
                Catch::ReusableStringStream callStartStream;
                callStartStream << callName << "( ";

                Catch::ReusableStringStream exprStream;
                exprStream << typeName << ", " << emptyInitializer << ", " << callName;

                std::initializer_list<const char*> extraArgs{std::forward<Args>(a)...};
                if (extraArgs.size() != 0) {
                    for (const auto& arg : extraArgs) {
                        callStartStream << arg << ", ";
                        exprStream << ", " << arg;
                    }
                }
                expressionString = exprStream.str();
                callStart = callStartStream.str();
            }
            std::string expressionString;
            std::string callStart;
        };

        /// Main workings of the two-call checker.
        template <typename T, typename F, typename... Args>
        inline std::vector<T> test(Catch::StringRef const& macroName, Strings const& strings, const Catch::SourceLineInfo& lineinfo,
                                   Catch::ResultDisposition::Flags resultDisposition, T const& empty, F&& wrappedCall, Args&&... a)
        {
            std::vector<T> ret;
            uint32_t count = 0;
            {
                std::string name = (Catch::ReusableStringStream() << strings.expressionString << " ) // count request call: "
                                                                  << strings.callStart << "0, &count, nullptr")
                                       .str();

                Catch::AssertionHandler catchAssertionHandler(macroName, lineinfo, name, resultDisposition);
                INTERNAL_CATCH_TRY
                {
                    auto result = wrappedCall(std::forward<Args>(a)..., 0, &count, nullptr);
                    catchAssertionHandler.handleExpr(Catch::ExprLhs<XrResult>(XR_SUCCESS) == result);
                }
                INTERNAL_CATCH_CATCH(catchAssertionHandler)
                INTERNAL_CATCH_REACT(catchAssertionHandler)
            }

            if (Catch::getResultCapture().lastAssertionPassed() && count > 0) {
                std::string name = (Catch::ReusableStringStream() << strings.expressionString << " ) // buffer fill call: "
                                                                  << strings.callStart << count << " /*capacity*/, &count, array")
                                       .str();

                Catch::AssertionHandler catchAssertionHandler(macroName, lineinfo, name, resultDisposition);
                INTERNAL_CATCH_TRY
                {
                    // Allocate
                    ret.resize(count, empty);

                    // Perform call and handle assertion
                    auto result = wrappedCall(std::forward<Args>(a)..., uint32_t(ret.size()), &count, ret.data());
                    catchAssertionHandler.handleExpr(Catch::ExprLhs<XrResult>(XR_SUCCESS) == result);
                    if (Catch::getResultCapture().lastAssertionPassed()) {
                        // If success, resize to exact length.
                        ret.resize(count);
                    }
                    else {
                        // In case of error, clear return value
                        ret.clear();
                    }
                }
                INTERNAL_CATCH_CATCH(catchAssertionHandler)
                INTERNAL_CATCH_REACT(catchAssertionHandler)
            }
            return ret;
        }
    }  // namespace twocallimpl
}  // namespace Conformance

// Internal macro
#define INTERNAL_TWO_CALL_STRINGIFY(...)                      \
    ::Conformance::twocallimpl::Strings                       \
    {                                                         \
        CATCH_REC_LIST(CATCH_INTERNAL_STRINGIFY, __VA_ARGS__) \
    }

// Internal macro providing shared implementation between CHECK_TWO_CALL and REQUIRE_TWO_CALL
#define INTERNAL_TEST_TWO_CALL(macroName, resultDisposition, STRINGS, TYPE, ...)                                                 \
    [&] {                                                                                                                        \
        return ::Conformance::twocallimpl::test<TYPE>(macroName##_catch_sr, STRINGS, CATCH_INTERNAL_LINEINFO, resultDisposition, \
                                                      __VA_ARGS__);                                                              \
    }()

    /*!
 * @defgroup TwoCallCheckers Checkers for Two Call Idiom
 *
 * All of these perform the two call idiom and return a std::vector<T>. Arguments are:
 *
 * - The type of a single buffer element
 * - An initializer for an empty single buffer element
 * - The name of the call
 * - Any additional arguments that should be passed **before**  the capacityInput, countOutput, and array parameters
 */
    /// @{

#ifndef CATCH_CONFIG_TRADITIONAL_MSVC_PREPROCESSOR
/// Try a two-call idiom in "check" mode: failures are recorded but return an empty container.
#define CHECK_TWO_CALL(TYPE, ...)                                                         \
    INTERNAL_TEST_TWO_CALL("CHECK_TWO_CALL", Catch::ResultDisposition::ContinueOnFailure, \
                           (INTERNAL_TWO_CALL_STRINGIFY(TYPE, __VA_ARGS__)), TYPE, __VA_ARGS__)
/// Try a two-call idiom in "require" mode: failures are recorded and terminate the execution.
#define REQUIRE_TWO_CALL(TYPE, ...)                                                                                                      \
    INTERNAL_TEST_TWO_CALL("REQUIRE_TWO_CALL", Catch::ResultDisposition::Normal, (INTERNAL_TWO_CALL_STRINGIFY(TYPE, __VA_ARGS__)), TYPE, \
                           __VA_ARGS__)
#else
/// Try a two-call idiom in "check" mode: failures are recorded but return an empty container.
#define CHECK_TWO_CALL(TYPE, ...)                                                         \
    INTERNAL_TEST_TWO_CALL("CHECK_TWO_CALL", Catch::ResultDisposition::ContinueOnFailure, \
                           INTERNAL_CATCH_EXPAND_VARGS(INTERNAL_TWO_CALL_STRINGIFY(TYPE, __VA_ARGS__)), TYPE, __VA_ARGS__)
/// Try a two-call idiom in "require" mode: failures are recorded and terminate the execution.
#define REQUIRE_TWO_CALL(TYPE, ...)                                              \
    INTERNAL_TEST_TWO_CALL("REQUIRE_TWO_CALL", Catch::ResultDisposition::Normal, \
                           INTERNAL_CATCH_EXPAND_VARGS(INTERNAL_TWO_CALL_STRINGIFY(TYPE, __VA_ARGS__)), TYPE, __VA_ARGS__)
#endif
/// @}
