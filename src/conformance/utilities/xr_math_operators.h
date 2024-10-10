// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>

#include <openxr/openxr.h>
#include "common/xr_linear.h"

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard)
#define xr_math_operators_nodiscard [[nodiscard]]
#endif
#endif

#if !defined xr_math_operators_nodiscard
#define xr_math_operators_nodiscard
#endif

xr_math_operators_nodiscard inline XrVector3f operator*(const XrVector3f& a, const float& scale)
{
    XrVector3f result;
    XrVector3f_Scale(&result, &a, scale);
    return result;
}

xr_math_operators_nodiscard inline XrQuaternionf operator*(const XrQuaternionf& a, const XrQuaternionf& b)
{
    XrQuaternionf result;
    XrQuaternionf_Multiply(&result, &a, &b);
    return result;
}

xr_math_operators_nodiscard inline XrPosef operator*(const XrPosef& a, const XrPosef& b)
{
    XrPosef result;
    XrPosef_Multiply(&result, &a, &b);
    return result;
}

xr_math_operators_nodiscard inline XrMatrix4x4f operator*(const XrMatrix4x4f& a, const XrMatrix4x4f& b)
{
    XrMatrix4x4f result;
    XrMatrix4x4f_Multiply(&result, &a, &b);
    return result;
}

xr_math_operators_nodiscard inline constexpr bool operator==(const XrVector3f& lhs, const XrVector3f& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

// Not using XrVector3f_Add here so that this can be constexpr
xr_math_operators_nodiscard inline constexpr XrVector3f operator+(const XrVector3f& a, const XrVector3f& b)
{
    return XrVector3f{a.x + b.x, a.y + b.y, a.z + b.z};
}
inline constexpr void operator+=(XrVector3f& a, const XrVector3f& b)
{
    a = a + b;
}
static_assert(XrVector3f{1, 2, 3} + XrVector3f{1, 2, 3} == XrVector3f{1 + 1, 2 + 2, 3 + 3}, "XrVector3f addition");

xr_math_operators_nodiscard inline constexpr XrVector3f operator-(const XrVector3f& a, const XrVector3f& b)
{
    return XrVector3f{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline constexpr void operator-=(XrVector3f& a, const XrVector3f& b)
{
    a = a - b;
}
static_assert(XrVector3f{1, 2, 3} - XrVector3f{1, 2, 3} == XrVector3f{1 - 1, 2 - 2, 3 - 3}, "XrVector3f subtraction");

namespace openxr
{
    namespace math_operators
    {
        xr_math_operators_nodiscard inline constexpr float DegToRad(float degree)
        {
            return degree / 180 * MATH_PI;
        }

        namespace Quat
        {
            constexpr XrQuaternionf Identity{0, 0, 0, 1};
            xr_math_operators_nodiscard inline constexpr float DotProduct(const XrQuaternionf& a, const XrQuaternionf& b)
            {
                return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            }
            xr_math_operators_nodiscard inline XrQuaternionf FromAxisAngle(const XrVector3f& axis, float radians)
            {
                XrQuaternionf result;
                XrQuaternionf_CreateFromAxisAngle(&result, &axis, radians);
                return result;
            }
            xr_math_operators_nodiscard inline XrVector3f RotateVector(const XrQuaternionf& quat, const XrVector3f& v)
            {
                XrVector3f result;
                XrQuaternionf_RotateVector3f(&result, &quat, &v);
                return result;
            }
            xr_math_operators_nodiscard inline bool ApproxEqual(const XrQuaternionf& a, const XrQuaternionf& b,
                                                                float tolerance = DegToRad(0.5f))
            {
                float dotProduct = fabs(DotProduct(a, b));
                // we want to check `acosf(dotProduct) < tolerance` here but the range
                // of acosf is [-1, 1], if the value is outside of this range it will return NaN.
                float cosTolerance = cos(tolerance);
                return dotProduct > cosTolerance;
            }
        }  // namespace Quat

        namespace Vector
        {
            xr_math_operators_nodiscard inline float DotProduct(const XrVector3f& a, const XrVector3f& b)
            {
                return XrVector3f_Dot(&a, &b);
            }
            xr_math_operators_nodiscard inline XrVector3f CrossProduct(const XrVector3f& a, const XrVector3f& b)
            {
                XrVector3f result;
                XrVector3f_Cross(&result, &a, &b);
                return result;
            }
            xr_math_operators_nodiscard inline XrVector3f Lerp(const XrVector3f& a, const XrVector3f& b, const float f)
            {
                XrVector3f result;
                XrVector3f_Lerp(&result, &a, &b, f);
                return result;
            }
            xr_math_operators_nodiscard inline float Length(const XrVector3f& a)
            {
                return XrVector3f_Length(&a);
            }
            inline void Normalize(XrVector3f& v)
            {
                XrVector3f_Normalize(&v);
            }
            xr_math_operators_nodiscard inline bool ApproxEqual(const XrVector3f& a, const XrVector3f& b, float tolerance = 0.001f)
            {
                return Length(a - b) < tolerance;
            }
        }  // namespace Vector

        namespace Pose
        {
            constexpr XrPosef Identity{Quat::Identity, {0, 0, 0}};

            xr_math_operators_nodiscard inline bool ApproxEqual(const XrPosef& a, const XrPosef& b, float positionTolerance = 0.001f,
                                                                float angularTolerance = DegToRad(0.5f))
            {
                return Vector::ApproxEqual(a.position, b.position, positionTolerance) &&
                       Quat::ApproxEqual(a.orientation, b.orientation, angularTolerance);
            }
        }  // namespace Pose

        namespace Matrix
        {
            // Not using XrMatrix4x4f_CreateIdentity so that this can be written as a constexpr.
            constexpr XrMatrix4x4f Identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
            xr_math_operators_nodiscard inline XrMatrix4x4f FromPose(const XrPosef& pose)
            {
                XrMatrix4x4f m;
                XrMatrix4x4f_CreateFromRigidTransform(&m, &pose);
                return m;
            }
            xr_math_operators_nodiscard inline XrMatrix4x4f FromTranslationRotationScale(const XrVector3f& translation,
                                                                                         const XrQuaternionf& rotation,
                                                                                         const XrVector3f& scale)
            {
                XrMatrix4x4f result;
                XrMatrix4x4f_CreateTranslationRotationScale(&result, &translation, &rotation, &scale);
                return result;
            }
            xr_math_operators_nodiscard inline XrMatrix4x4f InvertRigidBody(const XrMatrix4x4f& m)
            {
                XrMatrix4x4f result;
                XrMatrix4x4f_InvertRigidBody(&result, &m);
                return result;
            }
            xr_math_operators_nodiscard inline XrMatrix4x4f Transposed(const XrMatrix4x4f& m)
            {
                XrMatrix4x4f result;
                XrMatrix4x4f_Transpose(&result, &m);
                return result;
            }
        }  // namespace Matrix

    }  // namespace math_operators

}  // namespace openxr
