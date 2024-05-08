// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include "utilities/feature_availability.h"
#include "conformance_framework.h"

#include <vector>

namespace Conformance
{
    /// Skip the remainder of the test if the features are not available,
    /// otherwise return the extensions to enable
    static inline std::vector<const char*> SkipOrGetExtensions(const char* functionality, const GlobalData& globalData,
                                                               const FeatureSet& requiredFeatures)
    {
        FeatureSet available;
        globalData.PopulateVersionAndAvailableExtensions(available);
        if (!requiredFeatures.IsSatisfiedBy(available)) {
            SKIP(functionality << " not supported via " << requiredFeatures.ToString());
        }
        return requiredFeatures.GetExtensions();
    }
}  // namespace Conformance
