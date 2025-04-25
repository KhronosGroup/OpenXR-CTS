// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "interaction_info_generated.h"

namespace Conformance
{
    class Availability;
    struct BindingPathData
    {
        const char* Path;
        XrActionType Type;
        InteractionProfileAvailability Availability;
        bool systemOnly = false;
    };
    using BindingPathDataCollection = std::initializer_list<BindingPathData>;

    struct InteractionProfileAvailMetadata
    {
        /// Path string - a string literal
        const char* InteractionProfilePathString;
        /// The path string with `/interaction_profile/` prefix removed, for use as a Catch2 parameter
        const char* InteractionProfileShortname;

        /// Top level user paths and their availability
        std::vector<std::pair<const char*, InteractionProfileAvailability>> TopLevelPaths;

        /// Index into @ref kInteractionAvailabilities
        InteractionProfileAvailability Availability;
        BindingPathDataCollection BindingPaths;
    };

    /// Get the generated list of all interaction profiles with availability and other metadata
    const std::vector<InteractionProfileAvailMetadata>& GetAllInteractionProfiles();

    /// Get the generated metadata for a given interaction profile index enumerant.
    inline const InteractionProfileAvailMetadata& GetInteractionProfile(InteractionProfileIndex profile)
    {
        return GetAllInteractionProfiles()[(size_t)profile];
    }

    /// Get the generated metadata for `/interaction_profile/khr/simple_controller`
    inline const InteractionProfileAvailMetadata& GetSimpleInteractionProfile()
    {
        return GetInteractionProfile(InteractionProfileIndex::Profile_khr_simple_controller);
    }

    /// Look up an interaction profile availability enum to find the equivalent full @ref Availability object.
    inline const Availability& GetInteractionProfileAvailability(const InteractionProfileAvailability& avail)
    {
        return kInteractionAvailabilities[(size_t)avail];
    }
}  // namespace Conformance
