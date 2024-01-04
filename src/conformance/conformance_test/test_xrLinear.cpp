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

#include "common/xr_linear.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <openxr/openxr.h>

namespace Conformance
{
    static bool Vector3fEqual(const XrVector3f& a, const XrVector3f& b)
    {
        constexpr float e = 0.001f;
        return (a.x == Catch::Approx(b.x).epsilon(e)) && (a.y == Catch::Approx(b.y).epsilon(e)) && (a.z == Catch::Approx(b.z).epsilon(e));
    }

    static bool QuatfEqual(const XrQuaternionf& a, const XrQuaternionf& b)
    {
        constexpr float e = 0.001f;
        return (a.x == Catch::Approx(b.x).epsilon(e)) && (a.y == Catch::Approx(b.y).epsilon(e)) && (a.z == Catch::Approx(b.z).epsilon(e)) &&
               (a.w == Catch::Approx(b.w).epsilon(e));
    }

    static bool PosefEqual(const XrPosef& a, const XrPosef& b)
    {
        return Vector3fEqual(a.position, b.position) && QuatfEqual(a.orientation, b.orientation);
    };

    TEST_CASE("xrLinear", "")
    {
        SECTION("XrPosef")
        {
            SECTION("Identity")
            {
                static constexpr XrPosef knownIdentity{{0, 0, 0, 1}, {0, 0, 0}};

                XrPosef identity{};
                XrPosef_CreateIdentity(&identity);
                REQUIRE(PosefEqual(identity, knownIdentity));
            }

            SECTION("Transforms match")
            {
                static constexpr XrVector3f kVectorUp{0, 1, 0};
                static constexpr XrVector3f kVectorForward{0, 0, -1};

                auto validateTransformsMatch = [](const XrQuaternionf q, const XrVector3f& v) -> XrVector3f {
                    XrMatrix4x4f m{};
                    XrMatrix4x4f_CreateFromQuaternion(&m, &q);

                    XrVector3f resultMat{};
                    XrMatrix4x4f_TransformVector3f(&resultMat, &m, &v);

                    XrVector3f resultQuat{};
                    XrQuaternionf_RotateVector3f(&resultQuat, &q, &v);

                    REQUIRE(Vector3fEqual(resultMat, resultQuat));

                    return resultMat;
                };

                // Validate that identity quaternion doesn't rotate
                {
                    XrQuaternionf q{};
                    XrQuaternionf_CreateIdentity(&q);
                    auto vec = validateTransformsMatch(q, kVectorForward);

                    REQUIRE(Vector3fEqual(kVectorForward, vec));
                }
                // Validate that unit length XrVector3f can be rotated.
                {
                    XrQuaternionf q{};
                    XrQuaternionf_CreateFromAxisAngle(&q, &kVectorUp, 30 * (MATH_PI / 180));
                    validateTransformsMatch(q, kVectorForward);
                }
                // Validate that non-unit length XrVector3f can be rotated
                {
                    XrQuaternionf q{};
                    XrQuaternionf_CreateFromAxisAngle(&q, &kVectorForward, 30 * (MATH_PI / 180));

                    auto vec1 = validateTransformsMatch(q, kVectorUp);

                    XrVector3f nonUnitLength;
                    XrVector3f_Scale(&nonUnitLength, &kVectorUp, 2.0f);

                    auto vec2 = validateTransformsMatch(q, nonUnitLength);

                    XrVector3f vec2rescale;
                    XrVector3f_Scale(&vec2rescale, &vec2, 0.5f);

                    REQUIRE(Vector3fEqual(vec2rescale, vec1));
                }
            }
        }
    }

}  // namespace Conformance
