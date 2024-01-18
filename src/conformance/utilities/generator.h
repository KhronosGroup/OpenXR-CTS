// Copyright (c) 2019-2024, The Khronos Group Inc.
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
/*!
 * @file
 * @brief  Header for functionality similar to the Catch2 generators, but customized for our needs.
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 */

#pragma once

#include <memory>
#include <utility>

/*!
 * @defgroup cts_generators Generators
 * @ingroup cts_framework
 *
 * @brief A generator is a way to "produce" a collection of values or objects one at a time.
 *
 * This type of generator is conceptually very similar to the Catch2 generators,
 * but simpler to understand with less magic - no variable magically takes multiple values,
 * the generator simply produces one value per loop which it moves to your ownership at your request.
 * It also allows production of objects (typically, creator functions for objects), and not just values.
 *
 * A value of GeneratorWrapper type is returned by a factory function for a generator:
 * the specific GeneratorBase subclass implementing it is hidden as implementation details.
 *
 * To use a generator, you create it (as a GeneratorWrapper value) using the factory function.
 * Then, you start a while loop. The condition for your loop is `generator.next()`.
 * In the body of the loop, you call `generator.get()` **a single time** to retrieve the generated value/object.
 *
 * Example usage:
 *
 * ```
 * // Generate every combination of these flags, including none of the flags.
 * auto&& generator = bitmaskGeneratorIncluding0({
 *      XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT,
 *      XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
 *      XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
 *      XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
 *      XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
 * });
 * while (generator.next()) {
 *      auto val = generator.get();
 *      // Do something with val here
 * }
 * ```
 *
 * In a test, you may use a `DYNAMIC_SECTION` to split iterations,
 * if only a single generated value should be used per test case execution.
 * Alternately, if you don't need to re-start the test case for each generated value,
 * you can record the appropriate data about this iteration with an `INFO` or a `CAPTURE`.
 */

/*!
 * @defgroup cts_generators_details Implementation details of generators
 * @ingroup cts_generators
 */
/// @{
/*!
 * Base class for a generator.
 *
 * Only used by generator implementors: users of generators need not worry about this class,
 * though the interface matches the GeneratorWrapper which is directly used by generator users.
 */
template <typename T>
class GeneratorBase
{
public:
    virtual ~GeneratorBase() = default;

    /// Advance to the next element, if any, returning false if we have run out.
    ///
    /// Call at the top of your loop, not the bottom.
    virtual bool next() = 0;

    /// Retrieve the current element - only call once per loop iteration!
    virtual T get() = 0;
};

/*!
 * Value-wrapper for a unique_ptr holding a generator.
 *
 * Permits hiding the generators completely.
 *
 * Shares the same interface as GeneratorBase, however.
 */
template <typename T>
class GeneratorWrapper
{
    std::unique_ptr<GeneratorBase<T>> inner_;

public:
    explicit GeneratorWrapper(std::unique_ptr<GeneratorBase<T>>&& inner) : inner_(std::move(inner))
    {
    }

    /// Advance to the next element, if any, returning false if we have run out.
    ///
    /// Call at the top of your loop, not the bottom.
    bool next()
    {
        return inner_->next();
    }

    /// Retrieve the current element - only call once per loop iteration!
    T get()
    {
        return inner_->get();
    }
};
/// @}
