// Copyright (c) 2019-2022, The Khronos Group Inc.
// Copyright (c) 2018-2019 Collabora, Ltd.
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
 * @brief  Header providing wrappers for returning a variable-length collection
 * by repeatedly calling a "two-call idiom" OpenXR function for you. Lets you
 * pretend it's only a single call, possibly returning a std::vector<> (for some
 * variants).
 *
 * Based in part on xrtraits/TwoCall.h from https://gitlab.freedesktop.org/monado/utilities/xrtraits
 * (More complete functionality is available there under BSL-1.0)
 *
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 */

#pragma once

#include <openxr/openxr.h>
#include <vector>
#include <utility>

namespace Conformance
{

    namespace detail
    {

        static constexpr uint32_t MAX_CALLS_FOR_TWO_CALL_IDIOM = 5;
        struct TwoCallResult
        {
            bool doneCalling = false;
            XrResult returnCode = XR_SUCCESS;
            //! Only valid if returnCode is XR_SUCCESS or XR_ERROR_
            uint32_t count = 0;
        };
        template <typename F, typename... Args>
        static inline TwoCallResult getCount(F&& wrappedCall, Args&&... a)
        {
            TwoCallResult ret;
            ret.returnCode = wrappedCall(std::forward<Args>(a)..., 0, &ret.count, nullptr);

            if (ret.returnCode != XR_SUCCESS || ret.count == 0) {
                // Zero should always give success, whether there are 0
                // items or more.
                // If we were told the count was zero, we're also done.
                ret.doneCalling = true;
            }
            return ret;
        }

        template <typename T, typename F, typename... Args>
        static inline TwoCallResult callOnce(std::vector<T>& container, T const& emptyElement, F&& wrappedCall, Args&&... a)
        {
            TwoCallResult ret;
            if (container.empty()) {
                // No capacity, just treat as a count retrieval.
                ret = getCount(std::forward<F>(wrappedCall), std::forward<Args>(a)...);
            }
            else {
                // We have at least some capacity already.
                ret.returnCode = wrappedCall(std::forward<Args>(a)..., uint32_t(container.size()), &ret.count, container.data());

                // If we get a non-size related error, or a success,
                // we're done.
                ret.doneCalling = ret.returnCode != XR_ERROR_SIZE_INSUFFICIENT;
            }
            // Resize accordingly
            if (ret.returnCode == XR_SUCCESS || ret.returnCode == XR_ERROR_SIZE_INSUFFICIENT) {
                container.resize(ret.count, emptyElement);
            }

            return ret;
        }

        template <typename T, typename F, typename... Args>
        static inline XrResult twoCallLoop(uint32_t max_calls, std::vector<T>& container, T const& emptyElement, F&& wrappedCall,
                                           Args&&... a)
        {
            TwoCallResult result;
            // Repeatedly call until we succeed, fail, or get bored of
            // resizing.
            for (uint32_t i = 0; !result.doneCalling && i < max_calls; ++i) {
                result = callOnce(container, emptyElement, std::forward<F>(wrappedCall), std::forward<Args>(a)...);
            }
            return result.returnCode;
        }
    }  // namespace detail

    /*!
     * Perform the two call idiom, returning XrResult, to populate an existing
     * container, whose size may hint at expected count.
     *
     * In this variant, the default value of your element type will be used when
     * enlarging the vector. For things like OpenXR structs with type and next,
     * use @ref doTwoCallInPlaceWithEmptyElement
     *
     * @param container The container to fill. If it is not empty, the buffer size
     * will be used as a size hint: if sufficient, only one call to the wrappedCall
     * will be made.
     * @param wrappedCall A function or lambda that takes the `capacityInput`,
     * `countOutput`, and `array` parameters as its only or last parameters.
     * @param a Any additional arguments passed to this call will be forwarded to
     * the call **before** the `capacityInput`, `countOutput`, and `array`
     * parameters.
     *
     * Note that this does not include any Catch2 testing assertions - see @ref CHECK_TWO_CALL
     * and @ref REQUIRE_TWO_CALL for those.
     */
    template <typename T, typename F, typename... Args>
    inline XrResult doTwoCallInPlace(std::vector<T>& container, F&& wrappedCall, Args&&... a)
    {

        return detail::twoCallLoop(detail::MAX_CALLS_FOR_TWO_CALL_IDIOM, container, {}, std::forward<F>(wrappedCall),
                                   std::forward<Args>(a)...);
    }

    /*!
     * Perform the two call idiom, returning XrResult, to populate an existing
     * container, whose size may hint at expected count.
     *
     * @param container The container to fill. If it is not empty, the buffer size
     * will be used as a size hint: if sufficient, only one call to the wrappedCall
     * will be made.
     * @param emptyElement The empty/default element to copy when enlarging the vector.
     * @param wrappedCall A function or lambda that takes the `capacityInput`,
     * `countOutput`, and `array` parameters as its only or last parameters.
     * @param a Any additional arguments passed to this call will be forwarded to
     * the call **before** the `capacityInput`, `countOutput`, and `array`
     * parameters.
     *
     * Note that this does not include any Catch2 testing assertions - see CHECK_TWO_CALL
     * and REQUIRE_TWO_CALL for those.
     */
    template <typename T, typename F, typename... Args>
    inline XrResult doTwoCallInPlaceWithEmptyElement(std::vector<T>& container, T const& emptyElement, F&& wrappedCall, Args&&... a)
    {

        return detail::twoCallLoop(detail::MAX_CALLS_FOR_TWO_CALL_IDIOM, container, emptyElement, std::forward<F>(wrappedCall),
                                   std::forward<Args>(a)...);
    }

}  // namespace Conformance
