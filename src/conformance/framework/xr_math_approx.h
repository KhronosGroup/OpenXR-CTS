// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "utilities/xr_math_operators.h"
#include <string>

namespace openxr
{
    namespace math_operators
    {
        namespace Vector
        {

            //! Approximate comparison wrapper for an XrVector3f
            class Approx
            {
            public:
                //! Construct an Approx wrapper for an XrVector3f
                explicit Approx(const XrVector3f& value) : m_value(value)
                {
                }

                //! Set the tolerance of the approximate equality comparison
                Approx& tolerance(float newTolerance)
                {
                    assert(newTolerance >= 0);
                    m_tolerance = newTolerance;
                    return *this;
                }
                friend bool operator==(const XrVector3f& lhs, Approx const& rhs)
                {
                    return ApproxEqual(rhs.m_value, lhs, rhs.m_tolerance);
                }
                friend bool operator==(Approx const& lhs, const XrVector3f& rhs)
                {
                    return operator==(rhs, lhs);
                }
                friend bool operator!=(XrVector3f const& lhs, Approx const& rhs)
                {
                    return !operator==(lhs, rhs);
                }
                friend bool operator!=(Approx const& lhs, XrVector3f const& rhs)
                {
                    return !operator==(rhs, lhs);
                }
                std::string toString() const;

            private:
                XrVector3f m_value;
                float m_tolerance{0.001f};
            };
        }  // namespace Vector

        namespace Quat
        {

            //! Approximate comparison wrapper for an XrQuaternionf
            class Approx
            {
            public:
                //! Construct an Approx wrapper for an XrQuaternionf
                explicit Approx(const XrQuaternionf& value) : m_value(value)
                {
                }

                //! Set the tolerance of the approximate equality comparison
                Approx& tolerance(float newTolerance)
                {
                    assert(newTolerance >= 0);
                    m_tolerance = newTolerance;
                    return *this;
                }
                friend bool operator==(const XrQuaternionf& lhs, Approx const& rhs)
                {
                    return ApproxEqual(rhs.m_value, lhs, rhs.m_tolerance);
                }
                friend bool operator==(Approx const& lhs, const XrQuaternionf& rhs)
                {
                    return operator==(rhs, lhs);
                }
                friend bool operator!=(XrQuaternionf const& lhs, Approx const& rhs)
                {
                    return !operator==(lhs, rhs);
                }
                friend bool operator!=(Approx const& lhs, XrQuaternionf const& rhs)
                {
                    return !operator==(rhs, lhs);
                }
                std::string toString() const;

            private:
                XrQuaternionf m_value;
                float m_tolerance{DegToRad(0.5f)};
            };
        }  // namespace Quat

        namespace Pose
        {

            //! Approximate comparison wrapper for an XrPosef
            class Approx
            {
            public:
                //! Construct an Approx wrapper for an XrPosef
                explicit Approx(const XrPosef& value) : m_value(value)
                {
                }

                //! Set the tolerance of the approximate position equality comparison
                Approx& positionTolerance(float newTolerance)
                {
                    assert(newTolerance >= 0);
                    m_positionTolerance = newTolerance;
                    return *this;
                }

                //! Set the tolerance of the approximate angular equality comparison
                Approx& angularTolerance(float newTolerance)
                {
                    assert(newTolerance >= 0);
                    m_angularTolerance = newTolerance;
                    return *this;
                }
                friend bool operator==(const XrPosef& lhs, Approx const& rhs)
                {
                    return ApproxEqual(rhs.m_value, lhs, rhs.m_positionTolerance, rhs.m_angularTolerance);
                }
                friend bool operator==(Approx const& lhs, const XrPosef& rhs)
                {
                    return operator==(rhs, lhs);
                }
                friend bool operator!=(XrPosef const& lhs, Approx const& rhs)
                {
                    return !operator==(lhs, rhs);
                }
                friend bool operator!=(Approx const& lhs, XrPosef const& rhs)
                {
                    return !operator==(rhs, lhs);
                }
                std::string toString() const;

            private:
                XrPosef m_value;
                float m_positionTolerance{0.001f};
                float m_angularTolerance{DegToRad(0.5f)};
            };
        }  // namespace Pose
    }      // namespace math_operators
}  // namespace openxr
