// Copyright (c) 2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"
#include "utilities/feature_availability.h"
#include <ostream>

namespace Conformance
{
    inline std::ostream& operator<<(std::ostream& os, const FeatureSet& featureSet)
    {
        os << featureSet.ToString();
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const Availability& availability)
    {
        os << availability.ToString();
        return os;
    }

    namespace
    {
        const FeatureSet fsEmpty{};
        FeatureSet fsOnePointZero{FeatureBitIndex::BIT_XR_VERSION_1_0};
        FeatureSet fsOnePointZeroPlusOpenGL{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_KHR_opengl_enable};
    }  // namespace

    TEST_CASE("FeatureSet", "")
    {
        CHECK(fsEmpty.ToString() == "");
        CHECK(fsEmpty.IsSatisfiedBy(FeatureSet{}));

        CHECK(fsEmpty.IsSatisfiedBy(fsOnePointZero));

        CHECK(fsOnePointZero.IsSatisfiedBy(fsOnePointZero));
        CHECK_FALSE(fsOnePointZero.IsSatisfiedBy(fsEmpty));
        CHECK(fsOnePointZero.ToString() == "XR_VERSION_1_0");

        CHECK(fsOnePointZero.IsSatisfiedBy(fsOnePointZeroPlusOpenGL));
        CHECK(fsOnePointZeroPlusOpenGL.IsSatisfiedBy(fsOnePointZeroPlusOpenGL));
        CHECK_FALSE(fsOnePointZeroPlusOpenGL.IsSatisfiedBy(fsOnePointZero));
        CHECK(fsOnePointZeroPlusOpenGL.ToString() == "XR_VERSION_1_0+XR_KHR_opengl_enable");

        // Bit access
        CHECK(fsOnePointZeroPlusOpenGL.Get(FeatureBitIndex::BIT_XR_VERSION_1_0));
        CHECK_FALSE(fsOnePointZeroPlusOpenGL.Get(FeatureBitIndex::BIT_XR_LOADER_VERSION_1_0));
        CHECK(fsOnePointZeroPlusOpenGL.get_XR_KHR_opengl_enable());
        CHECK_FALSE(fsOnePointZero.get_XR_KHR_opengl_enable());
        CHECK_FALSE(fsOnePointZeroPlusOpenGL.get_XR_KHR_opengl_es_enable());
    }

    TEST_CASE("FeatureSetAvailability", "")
    {
        CHECK(Availability{}.ToString() == "");
        CHECK(Availability{}.IsSatisfiedBy(FeatureSet{}));

        CHECK_FALSE(Availability{fsOnePointZero}.IsSatisfiedBy(FeatureSet{}));
        CHECK(Availability{fsOnePointZero}.IsSatisfiedBy(fsOnePointZero));
        CHECK(Availability{fsOnePointZero}.IsSatisfiedBy(fsOnePointZeroPlusOpenGL));
        CHECK_FALSE(Availability{fsOnePointZeroPlusOpenGL}.IsSatisfiedBy(fsOnePointZero));

        // nobody would ever do this but it gives us a test case
        const FeatureSet fsLoader{FeatureBitIndex::BIT_XR_LOADER_VERSION_1_0};
        Availability avOneZeroOrLoader{fsOnePointZero, fsLoader};
        CAPTURE(fsLoader);
        CAPTURE(avOneZeroOrLoader);
        CHECK(avOneZeroOrLoader.IsSatisfiedBy(fsOnePointZero));
        CHECK(avOneZeroOrLoader.IsSatisfiedBy(fsOnePointZeroPlusOpenGL));
        CHECK(avOneZeroOrLoader.IsSatisfiedBy(fsLoader));
        CHECK_FALSE(avOneZeroOrLoader.IsSatisfiedBy(fsEmpty));
        CHECK(avOneZeroOrLoader.ToString() == "XR_VERSION_1_0,XR_LOADER_VERSION_1_0");

        CHECK(FeatureSet::VersionsOnly(fsOnePointZeroPlusOpenGL) == fsOnePointZero);

        {
            INFO("Check iterators");
            auto front = *avOneZeroOrLoader.begin();
            CHECK(front == fsOnePointZero);

            auto it = avOneZeroOrLoader.begin();
            ++it;
            REQUIRE(it != avOneZeroOrLoader.end());
            CHECK(*it == fsLoader);

            ++it;
            CHECK(it == avOneZeroOrLoader.end());
        }

        {
            INFO("Regression test");
            Availability req{FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_EXT_dpad_binding}};
            CAPTURE(req);
            CHECK_FALSE(
                req.IsSatisfiedBy(FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_KHR_composition_layer_cylinder,
                                             FeatureBitIndex::BIT_XR_EXT_debug_utils, FeatureBitIndex::BIT_XR_KHR_vulkan_enable2}));
        }
    }
}  // namespace Conformance
