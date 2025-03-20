// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include "utilities/feature_availability.h"
#include "conformance_framework.h"

#include <vector>

namespace Conformance
{
    /// Skip the remainder of the test if the features are not available.
    inline void SkipIfNotSatisfiable(const char* functionality, const GlobalData& globalData, const FeatureSet& requiredFeatures)
    {
        // FeatureSets with no version set are assumed to not care about the version.
        // (This is consistent with logic in GetDesiredVersion used by CreateBasicInstance etc.)
        if (requiredFeatures.AsMaxSetVersion() != 0) {
            if (requiredFeatures.AsMaxSetVersion() < Options::Get().minApiVersionValue) {
                SKIP("Required version for test is below CLI-specified minApiVersion");
            }
        }

        FeatureSet available;
        globalData.PopulateMaxSupportedVersionAndAvailableExtensions(available);
        if (!requiredFeatures.IsSatisfiedBy(available)) {
            SKIP(functionality << " not supported via " << requiredFeatures.ToString());
        }
    }
}  // namespace Conformance
