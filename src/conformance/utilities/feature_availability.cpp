// Copyright (c) 2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "feature_availability.h"
#include <cstdint>
#include <sstream>
#include "utilities/utils.h"
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

namespace Conformance
{
#define XRC_ENUM_FEATURES(_)    \
    _(XR_VERSION_1_0, 0)        \
    _(XR_LOADER_VERSION_1_0, 0) \
    _(XR_VERSION_1_1, 0)

    namespace
    {

        class TermJoiner
        {
        public:
            static constexpr char kFeatureSetTermJoinChar{'+'};
            static constexpr char kAvailabilityTermJoinChar{','};

            explicit TermJoiner(char joinChar) : m_joinChar(joinChar)
            {
            }

            void AddTerm(const char* term)
            {
                if (m_termCount != 0) {
                    m_stream << m_joinChar;
                }
                m_stream << term;
                ++m_termCount;
            }

            std::string ToString() const
            {
                return m_stream.str();
            }

            void Reset()
            {
                m_termCount = 0;
                m_stream = std::ostringstream();
            }

        private:
            const char m_joinChar;
            size_t m_termCount{0};
            std::ostringstream m_stream;
        };

    }  // namespace

    const char* FeatureBitToString(FeatureBitIndex bit)
    {

#define RETURN_BIT(EXT_NAME, NUM)         \
    case FeatureBitIndex::BIT_##EXT_NAME: \
        return "XR_" #EXT_NAME;

        switch (bit) {
            XRC_ENUM_FEATURES(RETURN_BIT)
            XR_LIST_EXTENSIONS(RETURN_BIT)
        default:
            return "INVALID";
        }

#undef RETURN_BIT
    }
    FeatureBitIndex FeatureNameToBitIndex(const std::string& extNameString)
    {

#define CHECK_BIT_NAME(EXT_NAME, NUM)           \
    if (#EXT_NAME == extNameString) {           \
        return FeatureBitIndex::BIT_##EXT_NAME; \
    }
        XRC_ENUM_FEATURES(CHECK_BIT_NAME)
        XR_LIST_EXTENSIONS(CHECK_BIT_NAME)

        // No matching name found
        return FeatureBitIndex::FEATURE_COUNT;
    }

    static void FeatureSetToString(const FeatureSet& featureSet, TermJoiner& joiner)
    {
#define PRINT_BIT(EXT_NAME, NUM)                           \
    if (featureSet.Get(FeatureBitIndex::BIT_##EXT_NAME)) { \
        joiner.AddTerm(#EXT_NAME);                         \
    }

        // No reflection macro for versions
        XRC_ENUM_FEATURES(PRINT_BIT)

        // Check all known extensions
        XR_LIST_EXTENSIONS(PRINT_BIT)
#undef PRINT_BIT
    }

    FeatureSet::FeatureSet(XrVersion coreVersion)
    {
        const auto major = XR_VERSION_MAJOR(coreVersion);
        const auto minor = XR_VERSION_MINOR(coreVersion);
        if (major == 1) {
            // 1.x for any x
            get_XR_VERSION_1_0() = true;
            if (minor >= 1) {
                // 1.1 and later 1.x
                get_XR_VERSION_1_1() = true;
            }
            // TODO 1.2, etc repeats similarly
        }
    }

    FeatureSet::FeatureSet(const std::initializer_list<FeatureBitIndex>& features) : FeatureSet()
    {
        for (auto feature : features) {
            Get(feature) = true;
        }
    }

    FeatureSet FeatureSet::VersionsOnly(const FeatureSet& other)
    {
#define OR_FEAT(FEAT, NUM) | ((uint32_t)FeatureBitIndex::BIT_##FEAT)
        uint32_t mask = 0 XRC_ENUM_FEATURES(OR_FEAT);
        return FeatureSet(other.m_bits & feat_bitset(mask));
    }

    FeatureSet FeatureSet::operator+(const FeatureSet& other) const
    {
        FeatureSet ret;
        for (uint32_t i = 0; i < (uint32_t)FeatureBitIndex::FEATURE_COUNT; ++i) {
            FeatureBitIndex bit(static_cast<FeatureBitIndex>(i));
            ret.Get(bit) = Get(bit) || other.Get(bit);
        }
        return ret;
    }

    FeatureSet& FeatureSet::operator+=(const FeatureSet& other)
    {
        if (&other != this) {
            *this = *this + other;
        }
        return *this;
    }

    bool FeatureSet::IsSatisfiedBy(const FeatureSet& availFeatures) const
    {
        return (m_bits & availFeatures.m_bits) == m_bits;
    }

    std::string FeatureSet::ToString() const
    {
        TermJoiner joiner{TermJoiner::kFeatureSetTermJoinChar};

        FeatureSetToString(*this, joiner);
        return joiner.ToString();
    }

    std::vector<const char*> FeatureSet::GetExtensions() const
    {
#define APPEND_EXT(EXT_NAME, NUM)               \
    if (Get(FeatureBitIndex::BIT_##EXT_NAME)) { \
        ret.push_back(#EXT_NAME);               \
    }

        std::vector<const char*> ret;
        // Check all known extensions
        XR_LIST_EXTENSIONS(APPEND_EXT)
        return ret;
#undef APPEND_EXT
    }

    bool FeatureSet::SetByExtensionNameString(const std::string& extNameString)
    {
        auto index = FeatureNameToBitIndex(extNameString);
        if (index == FeatureBitIndex::FEATURE_COUNT) {
            return false;
        }
        Get(index) = true;
        return true;
    }

    Availability::Availability(const FeatureSet& features) : m_conjunctions({features})
    {
    }

    Availability::Availability(const std::initializer_list<FeatureSet>& featureSets) : m_conjunctions(featureSets)
    {
    }

    bool Availability::IsSatisfiedBy(const FeatureSet& availFeatures) const
    {
        if (m_conjunctions.empty()) {
            // trivially satisfied, anything goes.
            return true;
        }

        for (const auto& featureSet : m_conjunctions) {
            if (featureSet.IsSatisfiedBy(availFeatures)) {
                return true;
            }
        }
        return false;
    }

    std::string Availability::ToString() const
    {
        TermJoiner availJoiner{TermJoiner::kAvailabilityTermJoinChar};
        TermJoiner featureSetJoiner{TermJoiner::kFeatureSetTermJoinChar};
        for (const FeatureSet& featureSet : m_conjunctions) {
            FeatureSetToString(featureSet, featureSetJoiner);
            availJoiner.AddTerm(featureSetJoiner.ToString().c_str());
            featureSetJoiner.Reset();
        }
        return availJoiner.ToString();
    }

}  // namespace Conformance
