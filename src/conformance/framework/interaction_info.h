// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "interaction_info_generated.h"

namespace Conformance
{
    struct InputSourcePathAvailData
    {
        const char* Path;
        XrActionType Type;
        InteractionProfileAvailability Availability;
        bool systemOnly = false;
    };
    using InputSourcePathAvailCollection = std::initializer_list<InputSourcePathAvailData>;

    struct InteractionProfileAvailMetadata
    {
        /// Path string - a string literal
        const char* InteractionProfilePathString;
        /// The path string with `/interaction_profile/` prefix removed, for use as a Catch2 parameter
        const char* InteractionProfileShortname;

        /// Top level user paths
        std::vector<const char*> TopLevelPaths;

        /// Index into @ref kInteractionAvailabilities
        InteractionProfileAvailability Availability;
        InputSourcePathAvailCollection InputSourcePaths;
    };

    /// Get the generated list of all interaction profiles with availability and other metadata
    const std::vector<InteractionProfileAvailMetadata>& GetAllInteractionProfiles();
    inline const InteractionProfileAvailMetadata& GetInteractionProfile(InteractionProfileIndex profile)
    {
        return GetAllInteractionProfiles()[(size_t)profile];
    }
    inline const InteractionProfileAvailMetadata& GetSimpleInteractionProfile()
    {
        return GetInteractionProfile(InteractionProfileIndex::Profile_khr_simple_controller);
    }
}  // namespace Conformance
