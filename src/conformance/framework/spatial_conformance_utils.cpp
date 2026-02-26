
// Copyright (c) 2019-2026 The Khronos Group Inc.
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

#include "spatial_conformance_utils.h"

#include <openxr/openxr.h>

#include <catch2/matchers/catch_matchers_vector.hpp>

#include "conformance_framework.h"
#include "utilities/throw_helpers.h"

using Catch::Matchers::VectorContains;

namespace Conformance
{

    bool IsSpatialCapabilitySupported(XrInstance instance, XrSystemId systemId, XrSpatialCapabilityEXT capability)
    {
        auto xrEnumerateSpatialCapabilitiesEXT =
            GetInstanceExtensionFunction<PFN_xrEnumerateSpatialCapabilitiesEXT>(instance, "xrEnumerateSpatialCapabilitiesEXT");

        uint32_t capabilityCount = 0;
        XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialCapabilitiesEXT(instance, systemId, 0, &capabilityCount, nullptr));
        std::vector<XrSpatialCapabilityEXT> supportedCapabilities(capabilityCount);
        XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialCapabilitiesEXT(instance, systemId, static_cast<uint32_t>(supportedCapabilities.size()),
                                                                &capabilityCount, supportedCapabilities.data()));
        return VectorContains(capability).match(supportedCapabilities);
    }

    bool IsSpatialComponentSupported(XrInstance instance, XrSystemId systemId, XrSpatialCapabilityEXT capability,
                                     XrSpatialComponentTypeEXT component)
    {
        auto xrEnumerateSpatialCapabilityComponentTypesEXT =
            GetInstanceExtensionFunction<PFN_xrEnumerateSpatialCapabilityComponentTypesEXT>(
                instance, "xrEnumerateSpatialCapabilityComponentTypesEXT");

        XrSpatialCapabilityComponentTypesEXT capabilityComponents{XR_TYPE_SPATIAL_CAPABILITY_COMPONENT_TYPES_EXT};
        XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialCapabilityComponentTypesEXT(instance, systemId, capability, &capabilityComponents));
        std::vector<XrSpatialComponentTypeEXT> supportedComponents(capabilityComponents.componentTypeCountOutput);
        capabilityComponents.componentTypeCapacityInput = static_cast<uint32_t>(supportedComponents.size());
        capabilityComponents.componentTypes = supportedComponents.data();
        XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialCapabilityComponentTypesEXT(instance, systemId, capability, &capabilityComponents));

        return VectorContains(component).match(supportedComponents);
    }

    bool IsSpatialCapabilityFeatureSupported(XrInstance instance, XrSystemId systemId, XrSpatialCapabilityEXT capability,
                                             XrSpatialCapabilityFeatureEXT feature)
    {
        auto xrEnumerateSpatialCapabilityFeaturesEXT =
            GetInstanceExtensionFunction<PFN_xrEnumerateSpatialCapabilityFeaturesEXT>(instance, "xrEnumerateSpatialCapabilityFeaturesEXT");

        uint32_t capabilityFeatureCountOutput;
        XRC_CHECK_THROW_XRCMD(
            xrEnumerateSpatialCapabilityFeaturesEXT(instance, systemId, capability, 0, &capabilityFeatureCountOutput, nullptr));
        std::vector<XrSpatialCapabilityFeatureEXT> supportedFeatures(capabilityFeatureCountOutput);
        XRC_CHECK_THROW_XRCMD(xrEnumerateSpatialCapabilityFeaturesEXT(instance, systemId, capability, capabilityFeatureCountOutput,
                                                                      &capabilityFeatureCountOutput, supportedFeatures.data()));

        return VectorContains(feature).match(supportedFeatures);
    }

}  // namespace Conformance
