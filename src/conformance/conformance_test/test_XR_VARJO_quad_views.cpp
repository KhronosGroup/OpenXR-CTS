// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "availability_helper.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "conformance_utils.h"
#include "utilities/feature_availability.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <openxr/openxr.h>

#include <stdint.h>
#include <array>
#include <vector>

using Catch::Matchers::VectorContains;

namespace Conformance
{
    namespace
    {
        const auto kExtensionRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_VARJO_quad_views};
        const auto kPromotedCoreRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_1};
        const auto kOverallRequirements = Availability{kExtensionRequirements, kPromotedCoreRequirements};

        constexpr uint32_t kFourViews = 4;

        std::vector<XrViewConfigurationType> getViewConfigurations(XrInstance instance, XrSystemId systemId)
        {
            uint32_t countOutput = 0;
            std::vector<XrViewConfigurationType> vctArray;
            REQUIRE(xrEnumerateViewConfigurations(instance, systemId, 0, &countOutput, nullptr) == XR_SUCCESS);

            REQUIRE_NOTHROW(vctArray.resize(countOutput, XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM));
            countOutput = 0;

            REQUIRE(xrEnumerateViewConfigurations(instance, systemId, (uint32_t)vctArray.size(), &countOutput, vctArray.data()) ==
                    XR_SUCCESS);
            REQUIRE(countOutput == vctArray.size());
            return vctArray;
        }

        // This tests everything without calling xrLocateViews
        void StereoWithFoveatedInsetNonInteractive(const FeatureSet& featureSet, bool mustSupportVct)
        {
            auto extensions = SkipOrGetExtensions("Stereo with foveated inset/quad views", GetGlobalData(), featureSet);
            AutoBasicInstance instance(extensions, AutoBasicInstance::createSystemId);
            XrSystemId systemId = instance.systemId;
            // xrEnumerateViewConfigurations
            std::vector<XrViewConfigurationType> vctArray = getViewConfigurations(instance, systemId);

            if (mustSupportVct) {
                REQUIRE_THAT(vctArray, VectorContains(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET));
            }
            else if (!VectorContains(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET).match(vctArray)) {
                SKIP("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET not enumerated as supported.");
            }

            // xrEnumerateViewConfigurationViews
            uint32_t countOutput = 0;

            std::vector<XrViewConfigurationView> vcvArray{kFourViews, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
            SECTION("Pass zero, get four")
            {
                REQUIRE(xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET,
                                                          0, &countOutput, nullptr) == XR_SUCCESS);
                REQUIRE(countOutput == kFourViews);
            }
            SECTION("Enum Views")
            {
                REQUIRE(xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET,
                                                          kFourViews, &countOutput, vcvArray.data()) == XR_SUCCESS);
                REQUIRE(countOutput == kFourViews);
            }
        }

    }  // namespace

    TEST_CASE("XR_VARJO_quad_views", "[XR_VARJO_quad_views]")
    {
        FeatureSet enabled;
        GetGlobalData().PopulateVersionAndEnabledExtensions(enabled);
        if (!kOverallRequirements.IsSatisfiedBy(enabled)) {
            SECTION("Requirements not enabled")
            {
                AutoBasicSession session(AutoBasicSession::OptionFlags::createSession);

                std::vector<XrViewConfigurationType> vctArray = getViewConfigurations(session.GetInstance(), session.GetSystemId());

                REQUIRE_THAT(vctArray, !VectorContains(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET));
            }
        }

        StereoWithFoveatedInsetNonInteractive(kExtensionRequirements, true);
    }

    namespace
    {
        void CheckStereoInsetLocateViews(CompositionHelper& compositionHelper, XrSpace space, XrTime predictedDisplayTime)
        {
            auto viewData = compositionHelper.LocateViews(space, predictedDisplayTime);
            const auto& views = std::get<std::vector<XrView>>(viewData);

            for (int eye = 0; eye < 2; eye++) {
                CAPTURE(eye);
                REQUIRE(views[eye].fov.angleDown <= views[eye + 2].fov.angleDown);
                REQUIRE(views[eye].fov.angleLeft <= views[eye + 2].fov.angleLeft);
                REQUIRE(views[eye + 2].fov.angleRight <= views[eye].fov.angleRight);
                REQUIRE(views[eye + 2].fov.angleUp <= views[eye].fov.angleUp);
                // Assert bitwise equality of poses, because the spec says "equal"
                REQUIRE(XrPosefCPP{views[eye + 2].pose} == XrPosefCPP{views[eye].pose});
            }
        }

        // this one does the xrLocateViews
        void StereoWithFoveatedInsetNonInteractiveFOV(const FeatureSet& featureSet)
        {
            if (!GetGlobalData().IsUsingGraphicsPlugin()) {
                // Nothing to check - no graphics plugin means no frame submission
                SKIP("Cannot test view location without a graphics plugin");
            }

            auto extensions = SkipOrGetExtensions("Stereo with foveated inset/quad views", GetGlobalData(), featureSet);

            InstanceREQUIRE instance;
            {
                XrInstance instanceRaw{XR_NULL_HANDLE_CPP};
                XRC_CHECK_THROW_XRCMD(CreateBasicInstance(&instanceRaw, true, extensions));
                instance.adopt(instanceRaw);
            }

            // XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO / XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET does not need to be supported, even with the extension enabled.

            // Explicitly naming view config type and ignoring whatever was configured on the command line
            CompositionHelper compositionHelper("Quad Views", instance.get(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET,
                                                true);
            XrSession session = compositionHelper.GetSession();

            XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP());

            InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
            interactionManager.AttachActionSets();

            compositionHelper.BeginSession();

            SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

            // Not interactive, we auto-advance after testing a number of frames.
            size_t frameCount = 0;
            const size_t maxFrames = 50;
            // Must process at least maxFrames in this time to pass.
            // Session state has already reached READY (waited in BeginSession).
            auto testTimeout = 10s;
            CountdownTimer countdownTimer(testTimeout);

            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;
                if (XrCompositionLayerBaseHeader* projLayer = simpleProjectionLayerHelper.TryGetUpdatedProjectionLayer(frameState)) {
                    layers.push_back(projLayer);
                }

                CheckStereoInsetLocateViews(compositionHelper, viewSpace, frameState.predictedDisplayTime);
                bool keepRunning = (frameCount < maxFrames) && !countdownTimer.IsTimeUp();
                ++frameCount;
                keepRunning &= compositionHelper.PollEvents();
                compositionHelper.EndFrame(frameState.predictedDisplayTime, std::move(layers));
                return keepRunning;
            };

            RenderLoop(session, updateLayers).Loop();
        }
    }  // namespace

    TEST_CASE("XR_VARJO_quad_views-fov", "[XR_VARJO_quad_views]")
    {
        StereoWithFoveatedInsetNonInteractiveFOV(kExtensionRequirements);
    }

    TEST_CASE("StereoWithFoveatedInset", "[XR_VERSION_1_1]")
    {
        StereoWithFoveatedInsetNonInteractiveFOV(kPromotedCoreRequirements);
    }

    namespace
    {
        void StereoWithFoveatedInsetInteractive(const FeatureSet& featureSet)
        {
            if (!GetGlobalData().IsUsingGraphicsPlugin()) {
                // Nothing to check - no graphics plugin means no frame submission
                SKIP("Cannot test view location without a graphics plugin");
            }

            auto extensions = SkipOrGetExtensions("Stereo with foveated inset/quad views", GetGlobalData(), featureSet);

            InstanceREQUIRE instance;
            {
                XrInstance instanceRaw{XR_NULL_HANDLE_CPP};
                XRC_CHECK_THROW_XRCMD(CreateBasicInstance(&instanceRaw, true, extensions));
                instance.adopt(instanceRaw);
            }

            // Explicitly naming view config type and ignoring whatever was configured on the command line
            CompositionHelper compositionHelper("Quad Views", instance.get(),
                                                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET);
            InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_separate.png", "Stereo inset views.");
            XrSession session = compositionHelper.GetSession();

            XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosefCPP());

            InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
            interactionManager.AttachActionSets();

            compositionHelper.BeginSession();

            SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

            auto updateLayers = [&](const XrFrameState& frameState) {
                std::vector<XrCompositionLayerBaseHeader*> layers;
                if (XrCompositionLayerBaseHeader* projLayer = simpleProjectionLayerHelper.TryGetUpdatedProjectionLayer(frameState)) {
                    layers.push_back(projLayer);
                }
                // User is more likely to do interesting things (e.g. eye tracking) during this interactive test.
                CheckStereoInsetLocateViews(compositionHelper, viewSpace, frameState.predictedDisplayTime);

                return interactiveLayerManager.EndFrame(frameState, layers);
            };

            RenderLoop(session, updateLayers).Loop();
        }
    }  // namespace

    TEST_CASE("XR_VARJO_quad_views-interactive", "[XR_VARJO_quad_views][composition][interactive][no_auto]")
    {
        StereoWithFoveatedInsetInteractive(kExtensionRequirements);
    }

    TEST_CASE("StereoWithFoveatedInset-interactive", "[XR_VERSION_1_1][composition][interactive][no_auto]")
    {
        StereoWithFoveatedInsetInteractive(kPromotedCoreRequirements);
    }
}  // namespace Conformance
