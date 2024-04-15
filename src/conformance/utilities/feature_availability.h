// Copyright (c) 2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#include <bitset>
#include <initializer_list>
#include <string>
#include <vector>
#include "utilities/utils.h"

namespace Conformance
{
    /// An enum containing all "features" (core versions and extensions, essentially)
    /// known in OpenXR.
    /// This is intended mainly for use with @ref FeatureSet, which uses it internally
    /// to index a bitset.
    /// Most elements are automatically generated using the reflection header:
    /// the values have the same name as the extension but with the leading XR_ removed.
    enum class FeatureBitIndex
    {
        /// OpenXR 1.0
        BIT_XR_VERSION_1_0 = 0,
        /// OpenXR 1.0, loader/negotiation API (generates a separate header)
        BIT_XR_LOADER_VERSION_1_0 = 1,
        /// OpenXR 1.1
        BIT_XR_VERSION_1_1 = 2,

    // generating all values for extensions here, prepending BIT_
#define DEF_BIT(EXT_NAME, NUM) BIT_##EXT_NAME,
        XR_LIST_EXTENSIONS(DEF_BIT)

        /// Last element, not a real feature, used for size and invalid returns.
        FEATURE_COUNT,
    };

    /// Return the feature (core version or extension) name for a given feature bit, or "UNKNOWN".
    ///
    /// @relates FeatureBitIndex
    const char* FeatureBitToString(FeatureBitIndex bit);

    /// Return a feature bit for the given extension name, if known,
    /// otherwise returns @ref FeatureBitIndex::FEATURE_COUNT
    ///
    /// Fairly slow because it is doing a lot of naive string comparisons.
    ///
    /// @relates FeatureBitIndex
    FeatureBitIndex FeatureNameToBitIndex(const std::string& extNameString);

    /// A set of features (core versions and extensions).
    ///
    /// Can be used to reflect a set of enabled extensions, or one way to
    /// satisfy the feature requirements of some entity.
    ///
    /// @see Availability, FeatureBitIndex
    class FeatureSet
    {
    public:
        /// Default constructor: all false
        FeatureSet() = default;

        /// Construct from a core version (patch ignored)
        explicit FeatureSet(XrVersion coreVersion);

        /// Construct from an initializer list of feature bit indices.
        explicit FeatureSet(const std::initializer_list<FeatureBitIndex>& features);

        /// Extract only the versions from the feature set
        static FeatureSet VersionsOnly(const FeatureSet& other);

        /// Return the union
        FeatureSet operator+(const FeatureSet& other) const;

        /// Update to the union
        FeatureSet& operator+=(const FeatureSet& other);

        /// Return true if this feature set, considered as requirements,
        /// is satisfied by the given available features @p availFeatures.
        /// That is, return true if the current feature set is a subset or
        /// equal to @p availFeatures
        bool IsSatisfiedBy(const FeatureSet& availFeatures) const;

        /// Format this feature set as a string
        std::string ToString() const;

        /// The number of features enabled
        size_t CountFeaturesEnabled() const
        {
            return m_bits.count();
        }

        /// Get the enabled extension names.
        /// These are all string literals, statically allocated.
        std::vector<const char*> GetExtensions() const;

        using feat_bitset = std::bitset<static_cast<size_t>(FeatureBitIndex::FEATURE_COUNT)>;

        /// Access a (non-const) reference to a bit for a feature by enum/index
        feat_bitset::reference Get(FeatureBitIndex feature)
        {
            return m_bits[static_cast<size_t>(feature)];
        }

        /// Access the value of a bit for a feature by enum/index (const)
        bool Get(FeatureBitIndex feature) const
        {
            return m_bits[static_cast<size_t>(feature)];
        }

        /// Set the bit for an extension name using its string.
        /// Slow - avoid if possible!
        /// Returns true if we recognized it.
        bool SetByExtensionNameString(const std::string& extNameString);

        friend bool operator==(const FeatureSet& lhs, const FeatureSet& rhs)
        {
            return lhs.m_bits == rhs.m_bits;
        }

// This makes an equivalent of the two "Get" functions, with the parameter already specified,
// for every known extension with get_ prepended
#define MAKE_EXT_ACCESSOR(EXT_NAME, NUM)             \
    feat_bitset::reference get_##EXT_NAME()          \
    {                                                \
        return Get(FeatureBitIndex::BIT_##EXT_NAME); \
    }                                                \
    bool get_##EXT_NAME() const                      \
    {                                                \
        return Get(FeatureBitIndex::BIT_##EXT_NAME); \
    }
        MAKE_EXT_ACCESSOR(XR_VERSION_1_0, 0)
        MAKE_EXT_ACCESSOR(XR_VERSION_1_1, 0)
        XR_LIST_EXTENSIONS(MAKE_EXT_ACCESSOR)
#undef MAKE_EXT_ACCESSOR

    private:
        /// Construct from a bitset
        explicit FeatureSet(const feat_bitset& bits) noexcept : m_bits(bits)
        {
        }
        feat_bitset m_bits;
    };

    /// Information on when an entity is available.
    ///
    /// In 'disjunctive normal form' - an OR of ANDs.
    /// In practice, this means it holds a collection of @ref FeatureSet structures,
    /// one of which must be satisfied.
    ///
    /// Since all availability should be statically known, there is no ability to modify
    /// objects of this class after construction.
    class Availability
    {
    public:
        /// Empty: always available. Usually not what you want.
        Availability() = default;

        /// Construct from a single feature set
        explicit Availability(const FeatureSet& features);

        /// Construct from a list of feature sets
        explicit Availability(const std::initializer_list<FeatureSet>& featureSets);

        /// Return true if some feature set in this availability
        /// is satisfied by the given available features @p availFeatures.
        /// Always returns true (trivially) if empty.
        bool IsSatisfiedBy(const FeatureSet& availFeatures) const;

        /// Format this availability as a string
        std::string ToString() const;

        using iterator = typename std::vector<FeatureSet>::const_iterator;

        iterator begin() const
        {
            return m_conjunctions.begin();
        }

        iterator end() const
        {
            return m_conjunctions.end();
        }

    private:
        std::vector<FeatureSet> m_conjunctions;
    };
}  // namespace Conformance
