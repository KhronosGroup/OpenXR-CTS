// Copyright (c) 2019-2024, The Khronos Group Inc.
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

#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_vector.hpp"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "matchers.h"
#include "report.h"
#include "availability_helper.h"
#include "utilities/feature_availability.h"
#include "utilities/utils.h"
#include "conformance_utils.h"
#include "two_call.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <openxr/openxr.h>
#include <algorithm>
#include <vector>

using namespace Conformance;

namespace Conformance
{
    using Catch::Matchers::VectorContains;
    namespace
    {
        const auto kExtensionRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_0, FeatureBitIndex::BIT_XR_EXT_local_floor};
        const auto kPromotedCoreRequirements = FeatureSet{FeatureBitIndex::BIT_XR_VERSION_1_1};
        const auto kOverallRequirements = Availability{kExtensionRequirements, kPromotedCoreRequirements};
    };  // namespace

    static bool InBounds(XrSpaceLocation& loc, XrExtent2Df& bounds)
    {
        if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT &&
              loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)) {
            return false;
        }
        return loc.pose.position.x >= -bounds.width / 2.0 &&   //
               loc.pose.position.x <= bounds.width / 2.0 &&    //
               loc.pose.position.z >= -bounds.height / 2.0 &&  //
               loc.pose.position.z <= bounds.height / 2.0 &&   //
               loc.pose.position.y >= 0;
    }

    // Checks whether space is inside the baseSpaceType's reference space bounds rect and above its floor level
    static bool CheckInBounds(XrSession session, XrReferenceSpaceType baseSpaceType, XrSpaceLocation& loc, bool& outHasBounds)
    {
        XrExtent2Df bounds;
        XrResult result = xrGetReferenceSpaceBoundsRect(session, baseSpaceType, &bounds);

        REQUIRE_THAT(result, In<XrResult>({XR_SUCCESS, XR_SPACE_BOUNDS_UNAVAILABLE}));

        outHasBounds = (result == XR_SUCCESS);

        if (!outHasBounds) {
            return false;
        }

        REQUIRE((loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT && loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT));

        return InBounds(loc, bounds);
    }

    static bool tracked(XrSpaceLocation& location)
    {
        return (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT &&
                location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT);
    }
    static bool valid(XrSpaceLocation& location)
    {
        return (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    }

    // Waits until space location has orientation and position valid flags, or if positionTracked is true, tracked flags. Times out after 10 seconds.
    static XrSpaceLocation WaitForSpaceValidOrTracked(XrTime time, XrSpace baseSpace, XrSpace space, bool positionTracked)
    {
        XrSpaceLocation spaceLoc{XR_TYPE_SPACE_LOCATION};

        WaitUntilPredicateWithTimeout(
            [&]() {
                xrLocateSpace(space, baseSpace, time, &spaceLoc);
                if (positionTracked) {
                    return tracked(spaceLoc);
                }
                else {
                    return valid(spaceLoc);
                }
            },
            std::chrono::seconds(10), std::chrono::milliseconds(5));

        return spaceLoc;
    }

    static void MatchXZ(XrTime time, XrSpace baseSpace, XrSpace space1, bool requirePositionTracked1, XrSpace space2,
                        bool requirePositionTracked2)
    {
        XrSpaceLocation space1Loc = WaitForSpaceValidOrTracked(time, baseSpace, space1, requirePositionTracked1);
        if (requirePositionTracked1) {
            REQUIRE(tracked(space1Loc));
        }
        else {
            REQUIRE(valid(space1Loc));
        }

        XrSpaceLocation space2Loc = WaitForSpaceValidOrTracked(time, baseSpace, space2, requirePositionTracked2);
        if (requirePositionTracked2) {
            REQUIRE(tracked(space2Loc));
        }
        else {
            REQUIRE(valid(space2Loc));
        }

        constexpr float epsilon = 0.1f;
        REQUIRE(space1Loc.pose.position.x == Catch::Approx(space2Loc.pose.position.x).margin(epsilon));
        REQUIRE(space1Loc.pose.position.z == Catch::Approx(space2Loc.pose.position.z).margin(epsilon));

        REQUIRE(space1Loc.pose.orientation.x == Catch::Approx(space2Loc.pose.orientation.x).margin(epsilon));
        REQUIRE(space1Loc.pose.orientation.y == Catch::Approx(space2Loc.pose.orientation.y).margin(epsilon));
        REQUIRE(space1Loc.pose.orientation.z == Catch::Approx(space2Loc.pose.orientation.z).margin(epsilon));
        REQUIRE(space1Loc.pose.orientation.w == Catch::Approx(space2Loc.pose.orientation.w).margin(epsilon));
    }

    static void MatchY(XrTime time, XrSpace baseSpace, XrSpace space1, bool requirePositionTracked1, XrSpace space2,
                       bool requirePositionTracked2)
    {
        XrSpaceLocation space1Loc = WaitForSpaceValidOrTracked(time, baseSpace, space1, requirePositionTracked1);
        if (requirePositionTracked1) {
            REQUIRE(tracked(space1Loc));
        }
        else {
            REQUIRE(valid(space1Loc));
        }

        XrSpaceLocation space2Loc = WaitForSpaceValidOrTracked(time, baseSpace, space2, requirePositionTracked2);
        if (requirePositionTracked2) {
            REQUIRE(tracked(space2Loc));
        }
        else {
            REQUIRE(valid(space2Loc));
        }

        constexpr float epsilon = 0.1f;
        REQUIRE(space1Loc.pose.position.y == Catch::Approx(space2Loc.pose.position.y).margin(epsilon));
    }

    static inline void SharedLocalFloorAutomated(const FeatureSet& featureSet)
    {
        GlobalData& globalData = GetGlobalData();
        const std::vector<const char*> extensions = SkipOrGetExtensions("Local floor", globalData, featureSet);

        // See if it is explicitly enabled by default
        FeatureSet enabled;
        globalData.PopulateVersionAndEnabledExtensions(enabled);
        if (!kOverallRequirements.IsSatisfiedBy(enabled)) {
            SECTION("Requirements not enabled")
            {
                AutoBasicInstance instance;
                AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

                std::vector<XrReferenceSpaceType> refSpaceTypes =
                    CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
                REQUIRE_THAT(refSpaceTypes, !Catch::Matchers::VectorContains(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT));
            }
        }

        SECTION("Validate creation")
        {
            AutoBasicInstance instance(extensions);
            AutoBasicSession session(AutoBasicSession::OptionFlags::createSession, instance);

            std::vector<XrReferenceSpaceType> refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
            REQUIRE_THAT(refSpaceTypes, Catch::Matchers::Contains(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT));

            XrReferenceSpaceCreateInfo localFloorCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            localFloorCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
            localFloorCreateInfo.poseInReferenceSpace = XrPosefCPP();

            XrSpace localFloor = XR_NULL_HANDLE;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &localFloorCreateInfo, &localFloor), XR_SUCCESS);
        }

        SECTION("Validate correctness")
        {
            AutoBasicInstance instance(extensions);
            AutoBasicSession session(AutoBasicSession::createInstance | AutoBasicSession::createSession | AutoBasicSession::beginSession |
                                         AutoBasicSession::createSwapchains | AutoBasicSession::createSpaces,
                                     instance);

            // Get frames iterating to the point of app focused state. This will draw frames along the way.
            FrameIterator frameIterator(&session);
            frameIterator.RunToSessionState(XR_SESSION_STATE_FOCUSED);

            // Render one frame to get a predicted display time for the xrLocateSpace calls.
            FrameIterator::RunResult runResult = frameIterator.SubmitFrame();
            REQUIRE(runResult == FrameIterator::RunResult::Success);
            XrTime time = frameIterator.frameState.predictedDisplayTime;

            XrSpace viewSpace = XR_NULL_HANDLE;
            XrSpace localSpace = XR_NULL_HANDLE;
            XrSpace stageSpace = XR_NULL_HANDLE;
            XrSpace localFloorSpace = XR_NULL_HANDLE;
            // A local space that is created with the y offset of stage space relative to local space.
            // This space must be equivalent to LOCAL_FLOOR.
            XrSpace localStageYOffsetSpace = XR_NULL_HANDLE;

            bool stageSpaceSupported = false;
            bool stageSpaceHasBounds = false;
            bool localInStageBounds = false;

            XrSpaceLocation localInViewLoc{XR_TYPE_SPACE_LOCATION};
            XrSpaceLocation localInStageLoc{XR_TYPE_SPACE_LOCATION};
            XrSpaceLocation localFloorInViewLoc{XR_TYPE_SPACE_LOCATION};

            XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            createInfo.poseInReferenceSpace = XrPosefCPP();

            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &viewSpace), XR_SUCCESS);

            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &localSpace), XR_SUCCESS);

            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
            REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &localFloorSpace), XR_SUCCESS);

            // Without LOCAL position valid in VIEW space, this test can't do much.
            localInViewLoc = WaitForSpaceValidOrTracked(time, viewSpace, localSpace, false);
            REQUIRE(valid(localInViewLoc));

            // If LOCAL is valid in VIEW space, LOCAL_FLOOR should be valid too as a fixed offset extension of LOCAL.
            localFloorInViewLoc = WaitForSpaceValidOrTracked(time, localFloorSpace, viewSpace, false);
            REQUIRE(valid(localFloorInViewLoc));

            SECTION("Match LOCAL_FLOOR and LOCAL orientation")
            {
                // Check only that LOCAL_FLOOR matches LOCAL XZ position and orientation in LOCAL space.
                // LOCAL in LOCAL and LOCAL_FLOOR in LOCAL are assumed to be tracked.
                MatchXZ(time, localSpace, localSpace, true, localFloorSpace, true);
            }

            SECTION("Match LOCAL_FLOOR and STAGE Y origin")
            {

                // If stage space is supported, check that LOCAL_FLOOR matches a LOCAL space that is created with LOCAL-to-STAGE y offset.
                std::vector<XrReferenceSpaceType> refSpaceTypes =
                    CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
                stageSpaceSupported = VectorContains(XR_REFERENCE_SPACE_TYPE_STAGE).match(refSpaceTypes);

                if (stageSpaceSupported) {
                    createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
                    REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &stageSpace), XR_SUCCESS);

                    localInStageLoc = WaitForSpaceValidOrTracked(time, stageSpace, localSpace, false);
                    REQUIRE(valid(localInStageLoc));

                    localInStageBounds = CheckInBounds(session, XR_REFERENCE_SPACE_TYPE_STAGE, localInStageLoc, stageSpaceHasBounds);

                    XrPosef yOffsetPose = XrPosefCPP();
                    yOffsetPose.position.y = -localInStageLoc.pose.position.y;

                    createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                    createInfo.poseInReferenceSpace = yOffsetPose;
                    REQUIRE_RESULT(xrCreateReferenceSpace(session, &createInfo, &localStageYOffsetSpace), XR_SUCCESS);
                }

                bool matchStageFloor = stageSpaceSupported && valid(localInStageLoc) && stageSpaceHasBounds && localInStageBounds;

                if (!matchStageFloor) {
                    std::ostringstream ss;
                    ss << "Skipping LOCAL_FLOOR and STAGE Y origin equality test because at least one of the conditions is not met:"
                       << '\n'                                                         //
                       << "STAGE supported      : " << stageSpaceSupported << '\n'     //
                       << "LOCAL in STAGE valid : " << valid(localInStageLoc) << '\n'  //
                       << "STAGE has bounds     : " << stageSpaceHasBounds << '\n'     //
                       << "LOCAL in STAGE bounds: " << localInStageBounds << '\n'      //
                       << "If \"STAGE has bounds\" is true but \"LOCAL in STAGE bounds\" is false, please repeat the test with the tracked device inside STAGE bounds!";
                    SKIP(ss.str());
                }
                else {
                    // Check that LOCAL_FLOOR matches LOCAL XZ position and orientation *and* STAGE Y position in STAGE space.
                    // LOCAL (and by extension LOCAL_FLOOR) is only required to be VALID in STAGE, not TRACKED. STAGE in STAGE is assumed to be tracked.
                    MatchXZ(time, stageSpace, localSpace, false, localFloorSpace, false);
                    MatchY(time, stageSpace, stageSpace, true, localFloorSpace, false);

                    // Check that LOCAL_FLOOR matches LOCAL XZ position and orientation *and* LOCAL-with-negative-LOCAL-to-STAGE-Y-offset Y position in STAGE space.
                    // LOCAL (and by extension LOCAL_FLOOR) is only required to be VALID in STAGE, not TRACKED.
                    MatchXZ(time, stageSpace, localStageYOffsetSpace, false, localFloorSpace, false);
                    MatchY(time, stageSpace, localStageYOffsetSpace, false, localFloorSpace, false);
                }
            }
        }
    }
    TEST_CASE("XR_EXT_local_floor", "[XR_EXT_local_floor]")
    {
        SharedLocalFloorAutomated(kExtensionRequirements);
    }

    TEST_CASE("XR_VERSION_1_1-local_floor", "[XR_VERSION_1_1]")
    {
        SharedLocalFloorAutomated(kPromotedCoreRequirements);
    }

    static void SharedLocalFloorInteractive(const FeatureSet& featureSet, const char* testName, XrReferenceSpaceType refSpaceType,
                                            const char* instructions)
    {
        GlobalData& globalData = GetGlobalData();

        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Not using graphics, which the test requires");
        }

        FeatureSet available;
        globalData.PopulateVersionAndAvailableExtensions(available);
        if (!featureSet.IsSatisfiedBy(available)) {
            SKIP("Local floor not supported via " << featureSet.ToString());
        }

        CompositionHelper compositionHelper(testName, featureSet.GetExtensions());

        XrSession session = compositionHelper.GetSession();

        // STAGE space is optional
        std::vector<XrReferenceSpaceType> refSpaceTypes = CHECK_TWO_CALL(XrReferenceSpaceType, {}, xrEnumerateReferenceSpaces, session);
        if (refSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE && !VectorContains(XR_REFERENCE_SPACE_TYPE_STAGE).match(refSpaceTypes)) {
            SKIP("XR_REFERENCE_SPACE_TYPE_STAGE not supported");
        }

        XrSpace refSpace = compositionHelper.CreateReferenceSpace(refSpaceType, XrPosefCPP{});
        XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, XrPosefCPP{});
        XrSpace localFloorSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT, XrPosefCPP{});

        // Set up composition projection layer and swapchains (one swapchain per view).
        std::vector<XrSwapchain> swapchains;
        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(refSpace);
        {
            const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();
            for (uint32_t j = 0; j < projLayer->viewCount; j++) {
                const XrSwapchain swapchain = compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(
                    viewProperties[j].recommendedImageRectWidth, viewProperties[j].recommendedImageRectHeight));
                const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, 0);
                swapchains.push_back(swapchain);
            }
        }

        InteractiveLayerManager interactiveLayerManager(compositionHelper, "local_floor.png", instructions);
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        auto update = [&](const XrFrameState& frameState) {
            std::vector<Cube> renderedCubes;

            XrSpaceLocation localFloorRefLoc{XR_TYPE_SPACE_LOCATION};
            REQUIRE_RESULT(xrLocateSpace(localFloorSpace, refSpace, frameState.predictedDisplayTime, &localFloorRefLoc), XR_SUCCESS);

            renderedCubes.push_back(Cube{/* pose */ {localFloorRefLoc.pose.orientation, localFloorRefLoc.pose.position},
                                         /* scale: */ {0.5f, 0.01f, 0.5f}});

            XrSpaceLocation localRefLoc{XR_TYPE_SPACE_LOCATION};
            REQUIRE_RESULT(xrLocateSpace(localSpace, refSpace, frameState.predictedDisplayTime, &localRefLoc), XR_SUCCESS);

            renderedCubes.push_back(Cube{/* pose */ {localRefLoc.pose.orientation, localRefLoc.pose.position},
                                         /* scale: */ {0.2f, 0.2f, 0.2f}});

            auto viewData = compositionHelper.LocateViews(refSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each viewport of the wide swapchain using the projection layer view fov and pose.
                for (size_t view = 0; view < views.size(); view++) {
                    compositionHelper.AcquireWaitReleaseImage(swapchains[view],  //
                                                              [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                                                  GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                                                                  const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                                                                  const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                                                                  GetGlobalData().graphicsPlugin->RenderView(
                                                                      projLayer->views[view], swapchainImage,
                                                                      RenderParams().Draw(renderedCubes));
                                                              });
                }
                layers.push_back({reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer)});
            }

            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, update).Loop();
    }

    namespace
    {
        const char* kLocalInstructions =
            "This test includes a 0.5m x 0.5m floor rendered at local floor space in local space. "
            "A 0.2m x 0.2m x 0.2m cube is rendered at local space. "
            "Ensure that the rendered floor is at the height of the physical floor.";
        const char* kStageInstructions =
            "This test includes a 0.5m x 0.5m floor rendered at local floor space in stage space. "
            "A 0.2m x 0.2m x 0.2m cube is rendered at local space. "
            "Ensure that the rendered floor is at the height of the physical floor.";

    }  // namespace

    TEST_CASE("XR_EXT_local_floor-local", "[XR_EXT_local_floor][scenario][interactive][no_auto]")
    {
        SharedLocalFloorInteractive(kExtensionRequirements, "local_floor local", XR_REFERENCE_SPACE_TYPE_LOCAL, kLocalInstructions);
    }
    TEST_CASE("local_floor-local", "[XR_VERSION_1_1][scenario][interactive][no_auto]")
    {
        SharedLocalFloorInteractive(kPromotedCoreRequirements, "1.1 local_floor local", XR_REFERENCE_SPACE_TYPE_LOCAL, kLocalInstructions);
    }

    // These are separate since stage is optional and thus they include a skip
    TEST_CASE("XR_EXT_local_floor-stage", "[XR_EXT_local_floor][scenario][interactive][no_auto]")
    {
        SharedLocalFloorInteractive(kExtensionRequirements, "local_floor stage", XR_REFERENCE_SPACE_TYPE_STAGE, kStageInstructions);
    }
    TEST_CASE("local_floor-stage", "[XR_VERSION_1_1][scenario][interactive][no_auto]")
    {
        SharedLocalFloorInteractive(kPromotedCoreRequirements, "1.1 local_floor stage", XR_REFERENCE_SPACE_TYPE_STAGE, kStageInstructions);
    }
}  // namespace Conformance
