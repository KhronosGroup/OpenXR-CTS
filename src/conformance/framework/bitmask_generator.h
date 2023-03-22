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
/*!
 * @file
 * @brief  Header providing a way to name bitmask bits,
 * combine names and descriptions,
 * and generate all combinations of bitmask bits.
 *
 * See the xrCreateSwapchain test for examples of usage.
 *
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 */

#pragma once

#include "generator.h"

#include <initializer_list>
#include <string>

namespace Conformance
{
    /*!
     * Generate all combinations of the supplied list of bitmasks,
     * including the 0 combination with none of the element (and thus bits).
     *
     * @see bitmaskGenerator
     *
     * @ingroup cts_generators
     */
    GeneratorWrapper<uint64_t const&> bitmaskGeneratorIncluding0(std::initializer_list<uint64_t> const& bits);

    /*!
     * Generate all combinations of the supplied list of bitmasks that include at least one set element.
     *
     * This excludes the 0 combination.
     *
     * @see bitmaskGeneratorIncluding0
     *
     * @ingroup cts_generators
     */
    GeneratorWrapper<uint64_t const&> bitmaskGenerator(std::initializer_list<uint64_t> const& bits);

}  // namespace Conformance
