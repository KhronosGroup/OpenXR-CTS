// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "xr_math_approx.h"
#include "conformance_framework.h"

#include <string>
#include <sstream>

namespace openxr
{
    namespace math_operators
    {

        namespace Quat
        {
            std::string Quat::Approx::toString() const
            {
                std::ostringstream oss;
                oss << "Approx( " << ::Catch::StringMaker<XrQuaternionf>::convert(m_value) << " )";
                return oss.str();
            }
        }  // namespace Quat
        namespace Vector
        {
            std::string Approx::toString() const
            {
                std::ostringstream oss;
                oss << "Approx( " << ::Catch::StringMaker<XrVector3f>::convert(m_value) << " )";
                return oss.str();
            }
        }  // namespace Vector
        namespace Pose
        {
            std::string Approx::toString() const
            {

                std::ostringstream oss;
                oss << "Approx( " << ::Catch::StringMaker<XrPosef>::convert(m_value) << " )";
                return oss.str();
            }
        }  // namespace Pose
    }      // namespace math_operators
}  // namespace openxr
