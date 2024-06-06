# Changelog for OpenXR-CTS Repo

<!--
Copyright (c) 2020-2024, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

Update log for the OpenXR-CTS repo on GitHub. Updates are
in reverse chronological order starting with the latest public release.

This summarizes the periodic public releases, not individual commits. Releases
on GitHub are generally done as single large patches at the release point,
collecting together the resolution of many Khronos internal issues, along with
any public pull requests that have been accepted. In this repository in
particular, since it is primarily software, pull requests may be integrated as
they are accepted even between periodic updates. However, versions that are not
signed tags on the `approved` branch are not valid for conformance submission.

## OpenXR CTS 1.1.37.0 (2024-06-06)

- Conformance Tests
  - Fix: Action test when constrained to right-hand only.
    ([internal MR 3244](https://gitlab.khronos.org/openxr/openxr/merge_requests/3244))
  - Fix: Skip StereoWithFoveatedInset-interactive if runtime does not support it.
    ([internal MR 3350](https://gitlab.khronos.org/openxr/openxr/merge_requests/3350))
  - Fix: Dangling pointer in action test.
    ([internal MR 3357](https://gitlab.khronos.org/openxr/openxr/merge_requests/3357))
  - Fix: Resolve Vulkan validation warning in some tests.
    ([internal MR 3367](https://gitlab.khronos.org/openxr/openxr/merge_requests/3367))
  - Improvement: Use absolute epsilons for pose comparison in action test.
    ([internal MR 3244](https://gitlab.khronos.org/openxr/openxr/merge_requests/3244))
  - Improvement: Improve user message in action test.
    ([internal MR 3244](https://gitlab.khronos.org/openxr/openxr/merge_requests/3244))
  - Improvement: Only suggest binding the proceed action to ".../click" binding
    paths, rather than all binding paths of boolean type, to avoid accidental
    activation.
    ([internal MR 3312](https://gitlab.khronos.org/openxr/openxr/merge_requests/3312))
  - Improvement: Support hiding parts of models in the glTF/PBR subsystem.
    ([internal MR 3314](https://gitlab.khronos.org/openxr/openxr/merge_requests/3314))
  - Improvement: Code cleanup.
    ([internal MR 3323](https://gitlab.khronos.org/openxr/openxr/merge_requests/3323))
  - Improvement: Use new `XR_API_VERSION_1_0` and `XR_API_VERSION_1_1` defines.
    ([internal MR 3329](https://gitlab.khronos.org/openxr/openxr/merge_requests/3329))
  - Improvement: Relax too strict palm/grip_surface pose assumptions.
    ([internal MR 3345](https://gitlab.khronos.org/openxr/openxr/merge_requests/3345))
  - Improvement: Add missing extension name tags to test cases.
    ([internal MR 3355](https://gitlab.khronos.org/openxr/openxr/merge_requests/3355))
  - Improvement: Code cleanup and documentation in helper utilities.
    ([internal MR 3356](https://gitlab.khronos.org/openxr/openxr/merge_requests/3356))
  - Improvement: Code cleanups and clang-tidy fixes.
    ([internal MR 3357](https://gitlab.khronos.org/openxr/openxr/merge_requests/3357),
    [internal MR 3357](https://gitlab.khronos.org/openxr/openxr/merge_requests/3357))
  - Improvement: Improve readability of test sources.
    ([internal MR 3358](https://gitlab.khronos.org/openxr/openxr/merge_requests/3358))
  - Improvement: Fix duplicated inaccurate code comment.
    ([internal MR 3372](https://gitlab.khronos.org/openxr/openxr/merge_requests/3372))
  - New test: Add interactive tests for `XR_KHR_composition_layer_equirect` and
    `XR_KHR_composition_layer_equirect2`.
    ([internal MR 2882](https://gitlab.khronos.org/openxr/openxr/merge_requests/2882))
  - New test: Add test which creates a session but does not call
    `xrDestroySession`.
    ([internal MR 3247](https://gitlab.khronos.org/openxr/openxr/merge_requests/3247))

## OpenXR CTS 1.1.36.0 (2024-04-25)

This new release supports testing both OpenXR 1.0 and OpenXR 1.1 runtimes, and
defaults to OpenXR 1.1 mode. See the README for more details.

- Conformance Tests
  - Fix: In multithreading test, only verify written portion of string buffer is
    UTF-8.
    ([internal MR 3232](https://gitlab.khronos.org/openxr/openxr/merge_requests/3232))
  - Fix: Increase `eps` for hand-tracking conformance tests.
    ([internal MR 3233](https://gitlab.khronos.org/openxr/openxr/merge_requests/3233))
  - Fix: Remove invalid interpretation of `XrInstanceProperties::runtimeVersion`.
    ([internal MR 3275](https://gitlab.khronos.org/openxr/openxr/merge_requests/3275))
  - Fix: Correct typo in CLI help text.
    ([internal MR 3302](https://gitlab.khronos.org/openxr/openxr/merge_requests/3302))
  - Fix: Correct typo in sample command lines in README.
    ([internal MR 3326](https://gitlab.khronos.org/openxr/openxr/merge_requests/3326))
  - Improvement: Update Android compile SDK version (to 33), NDK version (to 23.2),
    and build tools version (to 34.0.0).
    ([internal MR 2992](https://gitlab.khronos.org/openxr/openxr/merge_requests/2992))
  - Improvement: Reduce duplication of environment variable getters and setters.
    ([internal MR 3039](https://gitlab.khronos.org/openxr/openxr/merge_requests/3039))
  - Improvement: Enhancements to existing test of `XR_EXT_local_floor`.
    ([internal MR 3154](https://gitlab.khronos.org/openxr/openxr/merge_requests/3154),
    [internal issue 2150](https://gitlab.khronos.org/openxr/openxr/issues/2150),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3327](https://gitlab.khronos.org/openxr/openxr/merge_requests/3327))
  - Improvement: Use generated data from the XML in existing action tests rather
    than hardcoded tables.
    ([internal MR 3224](https://gitlab.khronos.org/openxr/openxr/merge_requests/3224),
    [internal issue 2063](https://gitlab.khronos.org/openxr/openxr/issues/2063),
    [internal MR 3306](https://gitlab.khronos.org/openxr/openxr/merge_requests/3306),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3321](https://gitlab.khronos.org/openxr/openxr/merge_requests/3321))
  - Improvement: Automatically enabled extension(s) needed for the interaction
    profile specified on the command line.
    ([internal MR 3224](https://gitlab.khronos.org/openxr/openxr/merge_requests/3224),
    [internal issue 2063](https://gitlab.khronos.org/openxr/openxr/issues/2063),
    [internal MR 3306](https://gitlab.khronos.org/openxr/openxr/merge_requests/3306),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3321](https://gitlab.khronos.org/openxr/openxr/merge_requests/3321))
  - Improvement: Code cleanup.
    ([internal MR 3257](https://gitlab.khronos.org/openxr/openxr/merge_requests/3257),
    [internal MR 3273](https://gitlab.khronos.org/openxr/openxr/merge_requests/3273),
    [internal MR 3208](https://gitlab.khronos.org/openxr/openxr/merge_requests/3208),
    [internal MR 3241](https://gitlab.khronos.org/openxr/openxr/merge_requests/3241))
  - Improvement: Allow `VK_FORMAT_R8G8_SRGB` in swapchains test
    ([internal MR 3258](https://gitlab.khronos.org/openxr/openxr/merge_requests/3258))
  - Improvement: Support specifying API version (1.0 or 1.1) - defaults to 1.1.
    ([internal MR 3274](https://gitlab.khronos.org/openxr/openxr/merge_requests/3274),
    [internal issue 2205](https://gitlab.khronos.org/openxr/openxr/issues/2205),
    [internal MR 3296](https://gitlab.khronos.org/openxr/openxr/merge_requests/3296),
    [internal MR 3297](https://gitlab.khronos.org/openxr/openxr/merge_requests/3297),
    [internal issue 2236](https://gitlab.khronos.org/openxr/openxr/issues/2236),
    [internal MR 3298](https://gitlab.khronos.org/openxr/openxr/merge_requests/3298),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318))
  - Improvement: Use spec-provided constants for inspecting enums for core vs
    extension origination.
    ([internal MR 3275](https://gitlab.khronos.org/openxr/openxr/merge_requests/3275))
  - New test: Automated test of core OpenXR 1.1 feature `LOCAL_FLOOR` reference
    space.
    ([internal MR 3154](https://gitlab.khronos.org/openxr/openxr/merge_requests/3154),
    [internal issue 2150](https://gitlab.khronos.org/openxr/openxr/issues/2150),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3327](https://gitlab.khronos.org/openxr/openxr/merge_requests/3327))
  - New test: Interactive test of `LOCAL_FLOOR` reference space (in both extension
    and promoted to core).
    ([internal MR 3154](https://gitlab.khronos.org/openxr/openxr/merge_requests/3154),
    [internal issue 2150](https://gitlab.khronos.org/openxr/openxr/issues/2150),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3327](https://gitlab.khronos.org/openxr/openxr/merge_requests/3327))
  - New test: Test for `xrLocateSpacesKHR` (from `XR_KHR_locate_spaces`) and
    `xrLocateSpaces` (promoted to core OpenXR 1.1).
    ([internal MR 3208](https://gitlab.khronos.org/openxr/openxr/merge_requests/3208),
    [internal issue 2149](https://gitlab.khronos.org/openxr/openxr/issues/2149))
  - New test: Verify correct handling of all interaction profile paths and their
    input component paths (accept vs reject suggested binding), in the "default"
    configuration of the instance, using generated data from the XML.
    ([internal MR 3224](https://gitlab.khronos.org/openxr/openxr/merge_requests/3224),
    [internal issue 2063](https://gitlab.khronos.org/openxr/openxr/issues/2063),
    [internal MR 3306](https://gitlab.khronos.org/openxr/openxr/merge_requests/3306),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3321](https://gitlab.khronos.org/openxr/openxr/merge_requests/3321))
  - New test: Created ProjectionDepth interactive test to visually verify behavior
    of `XR_FB_composition_layer_depth_test` extension.
    ([internal MR 3229](https://gitlab.khronos.org/openxr/openxr/merge_requests/3229))
  - New test: Automated and interactive tests for the "stereo with foveated inset"
    view configuration type (promoted to core OpenXR 1.1), as well as its extension
    predecessor `XR_VARJO_quad_views`.
    ([internal MR 3241](https://gitlab.khronos.org/openxr/openxr/merge_requests/3241),
    [internal issue 2152](https://gitlab.khronos.org/openxr/openxr/issues/2152),
    [internal MR 3310](https://gitlab.khronos.org/openxr/openxr/merge_requests/3310),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318))
  - New test: Additional test for grip_surface pose identifier (promoted to core
    OpenXR 1.1), as well as extension `XR_EXT_palm_pose`.
    ([internal MR 3245](https://gitlab.khronos.org/openxr/openxr/merge_requests/3245),
    [internal issue 2151](https://gitlab.khronos.org/openxr/openxr/issues/2151),
    [internal MR 3318](https://gitlab.khronos.org/openxr/openxr/merge_requests/3318),
    [internal MR 3328](https://gitlab.khronos.org/openxr/openxr/merge_requests/3328))
  - New test: Created non-interactive test for `XR_FB_space_warp` extension.
    ([internal MR 3278](https://gitlab.khronos.org/openxr/openxr/merge_requests/3278))

## OpenXR CTS 1.0.34.0 (2024-02-29)

- Conformance Tests
  - Fix: Correct the warning for when Wrist Z variance is above the 14 degree
    threshold.
    ([internal MR 3043](https://gitlab.khronos.org/openxr/openxr/merge_requests/3043))
  - Improvement: Code cleanup and documentation in the conformance layer.
    ([internal MR 3044](https://gitlab.khronos.org/openxr/openxr/merge_requests/3044))
  - Improvement: Make the conformance layer throw a distinct error when it
    encounters a handle whose creation it did not wrap.
    ([internal MR 3089](https://gitlab.khronos.org/openxr/openxr/merge_requests/3089))
  - Improvement: Mention in the instructions/README that the conformance automation
    extension may not be used for conformance submissions, and write a comment
    about this to the XML output when it is in use for easier identification.
    ([internal MR 3143](https://gitlab.khronos.org/openxr/openxr/merge_requests/3143))
  - Improvement: Reduce the maximum time allowed for transitioning session state in
    debug mode from 1 hour to 1 minute, and add a notice message in debug mode
    explaining this.
    ([internal MR 3151](https://gitlab.khronos.org/openxr/openxr/merge_requests/3151))
  - New test: Validate that `XrEventDataInteractionProfileChanged` is only queued
    during xrSyncActions using the conformance layer.
    ([internal MR 3044](https://gitlab.khronos.org/openxr/openxr/merge_requests/3044),
    [internal issue 1883](https://gitlab.khronos.org/openxr/openxr/issues/1883),
    [internal MR 3211](https://gitlab.khronos.org/openxr/openxr/merge_requests/3211))
  - New test: "SpaceOffset" interactive test validates the results of calling
    xrLocateSpace on spaces created with a non-identity pose. This tests some of
    the same math that Interactive Throw is intended to test, but with automatic
    pass/fail detection and better troubleshooting assistance and debugging
    visualization.
    ([internal MR 3058](https://gitlab.khronos.org/openxr/openxr/merge_requests/3058),
    [internal issue 1855](https://gitlab.khronos.org/openxr/openxr/issues/1855))

## OpenXR CTS 1.0.33.0 (2024-01-18)

- Conformance Tests
  - Fix: Refactor Pbr::Model into an immutable Pbr::Model and Pbr::Instance that
    holds the state for one drawn instance of the model. This corrects the known
    issue in the self-tests mentioned in a previous changelog.
    ([internal MR 3079](https://gitlab.khronos.org/openxr/openxr/merge_requests/3079),
    [internal issue 2139](https://gitlab.khronos.org/openxr/openxr/issues/2139),
    [internal MR 3141](https://gitlab.khronos.org/openxr/openxr/merge_requests/3141))
  - Fix: Avoid artificial error precedence requirement in test for
    `XR_ERROR_GRAPHICS_DEVICE_INVALID`, by making sure to call the "check graphics
    requirements" function if applicable.
    ([internal MR 3093](https://gitlab.khronos.org/openxr/openxr/merge_requests/3093),
    [internal issue 2155](https://gitlab.khronos.org/openxr/openxr/issues/2155))
  - Fix: Remove extra `xrSyncActions` call in `test_glTFRendering` to resolve
    interaction issue.
    ([internal MR 3107](https://gitlab.khronos.org/openxr/openxr/merge_requests/3107),
    [internal issue 2163](https://gitlab.khronos.org/openxr/openxr/issues/2163))
  - Fix: Skip `XR_MSFT_controller_model` interactive test if extension is not
    supported.
    ([internal MR 3146](https://gitlab.khronos.org/openxr/openxr/merge_requests/3146),
    [internal issue 2187](https://gitlab.khronos.org/openxr/openxr/issues/2187))
  - Improvement: Adjust `StringToPath` utility function to be easier to use.
    ([internal MR 2076](https://gitlab.khronos.org/openxr/openxr/merge_requests/2076))
  - Improvement: Simplify how some tests refer to the main OpenXR handles.
    ([internal MR 3023](https://gitlab.khronos.org/openxr/openxr/merge_requests/3023))
  - Improvement: Make `AutoBasicSession` use `EventReader` to support event
    multiplexing.
    ([internal MR 3023](https://gitlab.khronos.org/openxr/openxr/merge_requests/3023))
  - Improvement: Do not require system support for `XR_EXT_eye_gaze_interaction`
    before running XrPath and interaction profile related tests. Paths are valid as
    long as the extension is offered and enabled, regardless of whether there is
    system support for eye tracking.
    ([internal MR 3055](https://gitlab.khronos.org/openxr/openxr/merge_requests/3055))
  - Improvement: General code cleanup, warning fixes, clang-tidy fixes, and
    refactoring to improve flexibility and maintainability.
    ([internal MR 3082](https://gitlab.khronos.org/openxr/openxr/merge_requests/3082),
    [internal MR 3023](https://gitlab.khronos.org/openxr/openxr/merge_requests/3023))
  - Improvement: On Android, log using the "FATAL" severity before triggering an
    abort from the conformance layer.
    ([internal MR 3087](https://gitlab.khronos.org/openxr/openxr/merge_requests/3087))
  - Improvement: Exclude loader negotiation functions (added to XML and ratified
    spec in 1.0.33) from the list of functions automatically tested by the
    conformance suite.
    ([internal MR 3113](https://gitlab.khronos.org/openxr/openxr/merge_requests/3113))
  - New test: Check behavior for actions created without subaction paths, but
    queried using subaction paths.
    ([internal MR 3068](https://gitlab.khronos.org/openxr/openxr/merge_requests/3068))

## OpenXR CTS 1.0.32.1 (2023-12-14)

A notable change in this release, is that the build system now checks for git
commit/tag information at configure time and reports this information in the CTS
logs. If you have taken any porting steps that involve changing the build
system, be sure to update your changes accordingly. See the README for more
information.

There is one known issue with the new PBR rendering subsystem, but it only
affects running a self test under Vulkan, which is not required for conformance
submissions. It will be fixed in the next release.

- Conformance Tests
  - Fix: Handle the loader passing `xrInitializeLoaderKHR` calls to enabled API
    layers if `XR_KHR_loader_init` is enabled, per ratified update to that
    extension.
    ([internal MR 2703](https://gitlab.khronos.org/openxr/openxr/merge_requests/2703),
    [internal MR 3033](https://gitlab.khronos.org/openxr/openxr/merge_requests/3033))
  - Fix: comment typo in environment source.
    ([internal MR 2991](https://gitlab.khronos.org/openxr/openxr/merge_requests/2991))
  - Fix: Correct linking to GLX when glvnd is not found on the system.
    ([internal MR 3000](https://gitlab.khronos.org/openxr/openxr/merge_requests/3000))
  - Fix: Warning/build fix
    ([internal MR 3008](https://gitlab.khronos.org/openxr/openxr/merge_requests/3008))
  - Fix: Correct the object naming of command lists on D3D12.
    ([internal MR 3066](https://gitlab.khronos.org/openxr/openxr/merge_requests/3066))
  - Improvement: Add PBR rendering subsystem for loading and rendering of glTF
    assets.
    ([internal MR 2501](https://gitlab.khronos.org/openxr/openxr/merge_requests/2501),
    [internal issue 1726](https://gitlab.khronos.org/openxr/openxr/issues/1726),
    [internal MR 2758](https://gitlab.khronos.org/openxr/openxr/merge_requests/2758),
    [internal MR 3038](https://gitlab.khronos.org/openxr/openxr/merge_requests/3038),
    [internal MR 3081](https://gitlab.khronos.org/openxr/openxr/merge_requests/3081))
  - Improvement: Clean up our CMake build substantially, correcting dependencies
    and narrowing the scope of includes.
    ([internal MR 2886](https://gitlab.khronos.org/openxr/openxr/merge_requests/2886))
  - Improvement: Include git revision information in all reports, and generate test
    warnings in case of not matching a release tag, etc.
    ([internal MR 2964](https://gitlab.khronos.org/openxr/openxr/merge_requests/2964),
    [internal issue 2041](https://gitlab.khronos.org/openxr/openxr/issues/2041))
  - Improvement: Build system cleanup.
    ([internal MR 2987](https://gitlab.khronos.org/openxr/openxr/merge_requests/2987))
  - Improvement: Update configuration for Doxygen source-code documentation
    generator/extractor.
    ([internal MR 2988](https://gitlab.khronos.org/openxr/openxr/merge_requests/2988))
  - Improvement: Use "matchers" rather than STL algorithms to verify that supported
    environment blend modes do not include an invalid value.
    ([internal MR 2994](https://gitlab.khronos.org/openxr/openxr/merge_requests/2994))
  - New test: Interactive (rendering) test of `XR_MSFT_controller_model` as an
    initial usage of the glTF/PBR rendering.
    ([internal MR 2501](https://gitlab.khronos.org/openxr/openxr/merge_requests/2501),
    [internal issue 1726](https://gitlab.khronos.org/openxr/openxr/issues/1726),
    [internal MR 2758](https://gitlab.khronos.org/openxr/openxr/merge_requests/2758),
    [internal MR 3038](https://gitlab.khronos.org/openxr/openxr/merge_requests/3038),
    [internal MR 3081](https://gitlab.khronos.org/openxr/openxr/merge_requests/3081))
  - New test: Try zero XrTime values in hand tracking joints test.
    ([internal MR 2951](https://gitlab.khronos.org/openxr/openxr/merge_requests/2951))

## OpenXR CTS 1.0.30.0 (2023-10-12)

- Conformance Tests
  - Fix: Replace early returns with `SKIP()`.
    ([internal MR 2898](https://gitlab.khronos.org/openxr/openxr/merge_requests/2898),
    [OpenXR-CTS issue 60](https://github.com/KhronosGroup/OpenXR-CTS/issues/60),
    [internal issue 2072](https://gitlab.khronos.org/openxr/openxr/issues/2072))
  - Fix: Remove infinite loop in `Timed_Pipelined_Frame_Submission` in error case.
    ([internal MR 2915](https://gitlab.khronos.org/openxr/openxr/merge_requests/2915))
  - Fix: Test failure count API
    ([internal MR 2940](https://gitlab.khronos.org/openxr/openxr/merge_requests/2940),
    [internal issue 2072](https://gitlab.khronos.org/openxr/openxr/issues/2072),
    [internal MR 2965](https://gitlab.khronos.org/openxr/openxr/merge_requests/2965),
    [internal MR 2999](https://gitlab.khronos.org/openxr/openxr/merge_requests/2999))
  - Fix: Fix waiting for `xrWaitSwapchainImage` timeout cases.
    ([internal MR 2944](https://gitlab.khronos.org/openxr/openxr/merge_requests/2944))
  - Fix: Enable build with clang, clang-cl, and GCC (MinGW64) on Windows.
    ([internal MR 2948](https://gitlab.khronos.org/openxr/openxr/merge_requests/2948),
    [internal MR 2975](https://gitlab.khronos.org/openxr/openxr/merge_requests/2975))
  - Fix: Do not request hand tracking joint poses with an `XrTime` of 0: it is
    invalid. Be sure to be in "FOCUSED" since we want input data.
    ([internal MR 2977](https://gitlab.khronos.org/openxr/openxr/merge_requests/2977))
  - Improvement: Add validation of test tags to CTS.
    ([internal MR 2924](https://gitlab.khronos.org/openxr/openxr/merge_requests/2924),
    [internal issue 2050](https://gitlab.khronos.org/openxr/openxr/issues/2050),
    [internal issue 2062](https://gitlab.khronos.org/openxr/openxr/issues/2062))
  - Improvement: Make conformance layer initialization clear and consistent with
    other layers in the official OpenXR repos.
    ([internal MR 2926](https://gitlab.khronos.org/openxr/openxr/merge_requests/2926))
  - Improvement: Update Khronos registry URLs in comments.
    ([internal MR 2935](https://gitlab.khronos.org/openxr/openxr/merge_requests/2935))
  - Improvement: Add conformance test library API output value for Catch2 error
    conditions.
    ([internal MR 2940](https://gitlab.khronos.org/openxr/openxr/merge_requests/2940),
    [internal issue 2072](https://gitlab.khronos.org/openxr/openxr/issues/2072),
    [internal MR 2965](https://gitlab.khronos.org/openxr/openxr/merge_requests/2965),
    [internal MR 2999](https://gitlab.khronos.org/openxr/openxr/merge_requests/2999))
  - Improvement: Add OpenGL 3.3 functions to gfxwrapper, an internal utility
    library used by the CTS.
    ([internal MR 2941](https://gitlab.khronos.org/openxr/openxr/merge_requests/2941))
  - New test: Add additional tests for `XR_EXT_debug_utils` based on the test app
    `loader_test`.
    ([internal MR 2861](https://gitlab.khronos.org/openxr/openxr/merge_requests/2861))
  - New test: Check that no interaction profile is returned as current before
    calling `xrSyncActions`.
    ([internal MR 2897](https://gitlab.khronos.org/openxr/openxr/merge_requests/2897),
    [internal issue 1942](https://gitlab.khronos.org/openxr/openxr/issues/1942))

## OpenXR CTS 1.0.29.0 (2023-09-07)

- Conformance Tests
  - Fix: Use actual acquired image index in swapchain rendering test.
    ([internal MR 2746](https://gitlab.khronos.org/openxr/openxr/merge_requests/2746))
  - Fix: Do not use Catch2 assertion macros in graphics plugin methods that may be
    called before the first test case execution begins.
    ([internal MR 2756](https://gitlab.khronos.org/openxr/openxr/merge_requests/2756),
    [internal issue 1387](https://gitlab.khronos.org/openxr/openxr/issues/1387))
  - Fix: spelling.
    ([internal MR 2766](https://gitlab.khronos.org/openxr/openxr/merge_requests/2766))
  - Fix: Fix `<queries>` element contents in Android manifest.
    ([internal MR 2840](https://gitlab.khronos.org/openxr/openxr/merge_requests/2840),
    [internal issue 2053](https://gitlab.khronos.org/openxr/openxr/issues/2053))
  - Fix: Allow building CTS with mingw compiler.
    ([internal MR 2850](https://gitlab.khronos.org/openxr/openxr/merge_requests/2850))
  - Fix: Do not create an `XrInstance` during XML writing process, to prevent
    possible crash if one already exists.
    ([internal MR 2927](https://gitlab.khronos.org/openxr/openxr/merge_requests/2927))
  - Improvement: Refactor utilities that do not depend on Catch2 into a separate
    internal library.
    ([internal MR 2669](https://gitlab.khronos.org/openxr/openxr/merge_requests/2669))
  - Improvement: Refactor and standardize creation of swapchain image format
    tables, fixing some Vulkan invalid usage.
    ([internal MR 2685](https://gitlab.khronos.org/openxr/openxr/merge_requests/2685),
    [internal issue 1978](https://gitlab.khronos.org/openxr/openxr/issues/1978))
  - Improvement: Make composition test help/example world locked but based on
    initial view, for more natural reading.
    ([internal MR 2689](https://gitlab.khronos.org/openxr/openxr/merge_requests/2689))
  - Improvement: Cleanup and code quality work.
    ([internal MR 2704](https://gitlab.khronos.org/openxr/openxr/merge_requests/2704),
    [internal MR 2717](https://gitlab.khronos.org/openxr/openxr/merge_requests/2717),
    [internal MR 2784](https://gitlab.khronos.org/openxr/openxr/merge_requests/2784),
    [internal MR 2785](https://gitlab.khronos.org/openxr/openxr/merge_requests/2785),
    [internal MR 2808](https://gitlab.khronos.org/openxr/openxr/merge_requests/2808),
    [internal MR 2809](https://gitlab.khronos.org/openxr/openxr/merge_requests/2809))
  - Improvement: Add separate license file for gradlew and gradlew.bat
    ([internal MR 2725](https://gitlab.khronos.org/openxr/openxr/merge_requests/2725))
  - Improvement: Optionally poll `xrGetSystem` before running test cases.
    ([internal MR 2735](https://gitlab.khronos.org/openxr/openxr/merge_requests/2735),
    [OpenXR-CTS issue 53](https://github.com/KhronosGroup/OpenXR-CTS/issues/53),
    [internal issue 1947](https://gitlab.khronos.org/openxr/openxr/issues/1947))
  - Improvement: Select the first enumerated environment blend mode by default,
    rather than always using "opaque"
    ([internal MR 2736](https://gitlab.khronos.org/openxr/openxr/merge_requests/2736),
    [internal issue 1950](https://gitlab.khronos.org/openxr/openxr/issues/1950))
  - Improvement: Migrate more tests to use the `SKIP` macro when appropriate.
    ([internal MR 2737](https://gitlab.khronos.org/openxr/openxr/merge_requests/2737),
    [internal issue 1932](https://gitlab.khronos.org/openxr/openxr/issues/1932))
  - Improvement: Change background color based on selected blend mode: black for
    additive and transparent for alpha blend.
    ([internal MR 2883](https://gitlab.khronos.org/openxr/openxr/merge_requests/2883),
    [internal issue 1949](https://gitlab.khronos.org/openxr/openxr/issues/1949))
  - Improvement: Add extra information to errors in case of CTS timeouts.
    ([internal MR 2889](https://gitlab.khronos.org/openxr/openxr/merge_requests/2889))
  - Improvement: Remove conditional `XR_KHR_headless` support as the extension is
    not part of OpenXR 1.0.
    ([internal MR 2901](https://gitlab.khronos.org/openxr/openxr/merge_requests/2901))
  - Improvement: Remove empty `XR_EXT_performance_settings` test that was never
    implemented
    ([internal MR 2902](https://gitlab.khronos.org/openxr/openxr/merge_requests/2902))
  - Improvement: Fix names of tests to not have spaces, and adjust tags so that the
    instructions in the README will cause all tests to be executed.
    ([internal MR 2924](https://gitlab.khronos.org/openxr/openxr/merge_requests/2924))
  - New test: Verify two-call idiom behavior of `XR_MSFT_controller_model` as well
    as handling of invalid model keys.
    ([internal MR 2387](https://gitlab.khronos.org/openxr/openxr/merge_requests/2387),
    [internal MR 2858](https://gitlab.khronos.org/openxr/openxr/merge_requests/2858))
  - New test: Added `XR_EXT_plane_detection` extension.
    ([internal MR 2510](https://gitlab.khronos.org/openxr/openxr/merge_requests/2510),
    [internal MR 2751](https://gitlab.khronos.org/openxr/openxr/merge_requests/2751),
    [internal MR 2676](https://gitlab.khronos.org/openxr/openxr/merge_requests/2676))
  - New test: Add non-interactive test for `XR_EXT_palm_pose` vendor extension.
    ([internal MR 2672](https://gitlab.khronos.org/openxr/openxr/merge_requests/2672))
  - New test: Add joint query to non-interactive test for `XR_EXT_hand_tracking`.
    ([internal MR 2729](https://gitlab.khronos.org/openxr/openxr/merge_requests/2729),
    [internal MR 2795](https://gitlab.khronos.org/openxr/openxr/merge_requests/2795),
    [internal MR 2858](https://gitlab.khronos.org/openxr/openxr/merge_requests/2858),
    [internal MR 2916](https://gitlab.khronos.org/openxr/openxr/merge_requests/2916))
  - New test: Add test for calling `xrAcquireSwapchainImage` multiple times without
    calling `xrEndFrame`.
    ([internal MR 2730](https://gitlab.khronos.org/openxr/openxr/merge_requests/2730))
  - New test: Add additional tests for `XR_EXT_debug_utils` based on the test app
    `loader_test`.
    ([internal MR 2775](https://gitlab.khronos.org/openxr/openxr/merge_requests/2775))
  - New test: Add checks for palm position and palm and wrist orientation to
    `XR_EXT_hand_tracking` interactive tests.
    ([internal MR 2798](https://gitlab.khronos.org/openxr/openxr/merge_requests/2798))
  - New test: Add unbound action set to action sets test.
    ([internal MR 2862](https://gitlab.khronos.org/openxr/openxr/merge_requests/2862),
    [internal issue 2043](https://gitlab.khronos.org/openxr/openxr/issues/2043))
  - New test: Add conformance test for calling `xrDestroyInstance` from a different
    thread to `xrCreateInstance`, and `xrDestroySession` on a different thread to
    `xrCreateSession`.
    ([internal MR 2863](https://gitlab.khronos.org/openxr/openxr/merge_requests/2863))
  - New test: Add interactive conformance test for infrequently updated swapchains.
    ([internal MR 2873](https://gitlab.khronos.org/openxr/openxr/merge_requests/2873))
  - New test: Add conformance tests for `xrCreateSession` failing, then passing
    ([internal MR 2884](https://gitlab.khronos.org/openxr/openxr/merge_requests/2884))
  - New test: Test `xrSyncActions` with no active action sets.
    ([internal MR 2903](https://gitlab.khronos.org/openxr/openxr/merge_requests/2903))
  - New test: Test calling `xrLocateSpace` with timestamps up to 1s old.
    ([internal MR 2904](https://gitlab.khronos.org/openxr/openxr/merge_requests/2904))

## OpenXR CTS 1.0.27.0 (2023-05-10)

This release contains a large number of new or improved tests. It is expected
that many existing runtimes may initially fail some of these; implementers
should work to resolve these issues as soon as possible.

This also contains updated instructions for running tests and submitting
results, now that the test suite has a custom reporter that simplifies review of
results, and now that an improved method of running the tests is available on
Android.

- Registry
  - All changes found in 1.0.27.
- Conformance Tests
  - Fix: Make the comment about the optional handle validation accurate.
    ([internal MR 2545](https://gitlab.khronos.org/openxr/openxr/merge_requests/2545))
  - Fix: Use the last waited timestamp in Action Spaces test, to avoid asking for
    an older time in the second xrLocateSpace call than the first.
    ([internal MR 2592](https://gitlab.khronos.org/openxr/openxr/merge_requests/2592))
  - Fix: Inclusive language updates.
    ([internal MR 2648](https://gitlab.khronos.org/openxr/openxr/merge_requests/2648))
  - Fix: Update URLs with branch names in scripts.
    ([internal MR 2648](https://gitlab.khronos.org/openxr/openxr/merge_requests/2648))
  - Fix: Specify `XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT` when creating swapchains the
    test will copy to.
    ([internal MR 2683](https://gitlab.khronos.org/openxr/openxr/merge_requests/2683),
    [internal issue 1977](https://gitlab.khronos.org/openxr/openxr/issues/1977))
  - Fix: Skip some tests for Vulkan image format that is stencil-only, as a
    temporary workaround for validation failures. More complete fix coming in the
    next release.
    ([internal MR 2692](https://gitlab.khronos.org/openxr/openxr/merge_requests/2692),
    [internal issue 1978](https://gitlab.khronos.org/openxr/openxr/issues/1978))
  - Improvement: Refactor how swapchain images are created and used in tests, to
    normalize across all graphics APIs and prepare for incoming new tests.
    ([internal MR 2432](https://gitlab.khronos.org/openxr/openxr/merge_requests/2432),
    [internal MR 2531](https://gitlab.khronos.org/openxr/openxr/merge_requests/2531),
    [internal MR 2519](https://gitlab.khronos.org/openxr/openxr/merge_requests/2519))
  - Improvement: Attempt to render to (nearly) all swapchain image formats
    enumerated by a runtime, to test the ability to actually use those formats.
    ([internal MR 2383](https://gitlab.khronos.org/openxr/openxr/merge_requests/2383),
    [internal issue 1456](https://gitlab.khronos.org/openxr/openxr/issues/1456),
    [internal MR 2542](https://gitlab.khronos.org/openxr/openxr/merge_requests/2542),
    [internal MR 2554](https://gitlab.khronos.org/openxr/openxr/merge_requests/2554),
    [internal issue 1908](https://gitlab.khronos.org/openxr/openxr/issues/1908),
    [internal MR 2625](https://gitlab.khronos.org/openxr/openxr/merge_requests/2625),
    [internal MR 2643](https://gitlab.khronos.org/openxr/openxr/merge_requests/2643),
    [internal MR 2644](https://gitlab.khronos.org/openxr/openxr/merge_requests/2644),
    [internal MR 2660](https://gitlab.khronos.org/openxr/openxr/merge_requests/2660),
    [internal MR 2661](https://gitlab.khronos.org/openxr/openxr/merge_requests/2661),
    [internal MR 2686](https://gitlab.khronos.org/openxr/openxr/merge_requests/2686),
    [internal issue 1975](https://gitlab.khronos.org/openxr/openxr/issues/1975))
  - Improvement: Upgrade Catch2 to 3.3.2 (new major release, reduced build time by
    split compilation among other improvements, addition of a "SKIP" macro, and
    more)
    ([internal MR 2429](https://gitlab.khronos.org/openxr/openxr/merge_requests/2429),
    [internal MR 2530](https://gitlab.khronos.org/openxr/openxr/merge_requests/2530),
    [internal MR 2635](https://gitlab.khronos.org/openxr/openxr/merge_requests/2635))
  - Improvement: Clean up single-controller action spaces test, make more readable.
    ([internal MR 2481](https://gitlab.khronos.org/openxr/openxr/merge_requests/2481))
  - Improvement: Update bundled `stb_image.h` from 2.22 to 2.27.
    ([internal MR 2522](https://gitlab.khronos.org/openxr/openxr/merge_requests/2522))
  - Improvement: Update bundled `stb_truetype.h` from 1.21 to 1.26.
    ([internal MR 2522](https://gitlab.khronos.org/openxr/openxr/merge_requests/2522))
  - Improvement: Code cleanup, warning fixes, and comment fixes.
    ([internal MR 2523](https://gitlab.khronos.org/openxr/openxr/merge_requests/2523),
    [internal MR 2483](https://gitlab.khronos.org/openxr/openxr/merge_requests/2483),
    [internal MR 2582](https://gitlab.khronos.org/openxr/openxr/merge_requests/2582),
    [internal MR 2583](https://gitlab.khronos.org/openxr/openxr/merge_requests/2583),
    [internal MR 2608](https://gitlab.khronos.org/openxr/openxr/merge_requests/2608),
    [internal MR 2622](https://gitlab.khronos.org/openxr/openxr/merge_requests/2622),
    [internal MR 2625](https://gitlab.khronos.org/openxr/openxr/merge_requests/2625),
    [internal MR 2629](https://gitlab.khronos.org/openxr/openxr/merge_requests/2629))
  - Improvement: Update Vulkan plugin to use the newer `VK_EXT_debug_utils`
    extension (if available), and provide names for most Vulkan objects used by the
    CTS to aid in debugging.  (Utility code shared with hello_xr.)
    ([internal MR 2524](https://gitlab.khronos.org/openxr/openxr/merge_requests/2524),
    [internal MR 2579](https://gitlab.khronos.org/openxr/openxr/merge_requests/2579),
    [internal MR 2637](https://gitlab.khronos.org/openxr/openxr/merge_requests/2637),
    [internal MR 2687](https://gitlab.khronos.org/openxr/openxr/merge_requests/2687),
    [internal issue 1967](https://gitlab.khronos.org/openxr/openxr/issues/1967))
  - Improvement: Remove third-party dependencies in `external/include/utils`.
    ([internal MR 2528](https://gitlab.khronos.org/openxr/openxr/merge_requests/2528))
  - Improvement: Implement custom test reporter (based on the junit-style XML
    reporter) with additional information needed for evaluating conformance
    results.
    ([internal MR 2543](https://gitlab.khronos.org/openxr/openxr/merge_requests/2543),
    [internal issue 1460](https://gitlab.khronos.org/openxr/openxr/issues/1460),
    [internal issue 1952](https://gitlab.khronos.org/openxr/openxr/issues/1952))
  - Improvement: Teach the CTS how to parse and format bitmasks (if appropriately
    marked up in source).
    ([internal MR 2545](https://gitlab.khronos.org/openxr/openxr/merge_requests/2545),
    [internal MR 2606](https://gitlab.khronos.org/openxr/openxr/merge_requests/2606))
  - Improvement: Clean up Catch2 globals better after a test run.
    ([internal MR 2546](https://gitlab.khronos.org/openxr/openxr/merge_requests/2546))
  - Improvement: Add warning in `xrAttachSessionActionSets` test if runtime returns
    `XR_SUCCESS` rather than `XR_SESSION_NOT_FOCUSED`.
    ([internal MR 2548](https://gitlab.khronos.org/openxr/openxr/merge_requests/2548))
  - Improvement: Update all `XrStructureType` initialization to use standard OpenXR
    style.
    ([internal MR 2557](https://gitlab.khronos.org/openxr/openxr/merge_requests/2557))
  - Improvement: Make sure that we do not expect a subAction path that we are not
    testing to become alive in `xrSuggestInteractionProfileBindings_order`.
    ([internal MR 2567](https://gitlab.khronos.org/openxr/openxr/merge_requests/2567))
  - Improvement: Respect enabled interaction profile in D-Pad binding test, also
    a slight improvement/refactor of test.
    ([internal MR 2574](https://gitlab.khronos.org/openxr/openxr/merge_requests/2574))
  - Improvement: Skip requiring/initializing a graphics plugin if one of the
    following console-output-only flags is provided: `--list-tests`, `--list-tags`,
    `--list-listeners`, `--list-reporters`
    ([internal MR 2622](https://gitlab.khronos.org/openxr/openxr/merge_requests/2622),
    [internal issue 1968](https://gitlab.khronos.org/openxr/openxr/issues/1968),
    [internal MR 2675](https://gitlab.khronos.org/openxr/openxr/merge_requests/2675))
  - Improvement: Remove spaces from all test names and some top-level section names
    for ease of specifying tests.
    ([internal MR 2623](https://gitlab.khronos.org/openxr/openxr/merge_requests/2623))
  - Improvement: Fix crash under unusual circumstances in D3D graphics plugins.
    ([internal MR 2625](https://gitlab.khronos.org/openxr/openxr/merge_requests/2625))
  - Improvement: Add a new way of specifying test parameters on Android, using
    "intent extras". (The previous method using `setprop` is left in place for
    those use cases that benefit, and was modified to be lower priority and to
    remove the pre-Catch2-3.x constraint of only a single reporter.)
    ([internal MR 2632](https://gitlab.khronos.org/openxr/openxr/merge_requests/2632),
    [internal issue 1828](https://gitlab.khronos.org/openxr/openxr/issues/1828))
  - Improvement: Delete conformance_layer introspection code which is not required
    with the Android loader.
    ([internal MR 2650](https://gitlab.khronos.org/openxr/openxr/merge_requests/2650))
  - Improvement: Delete conformance_layer introspection code which is not required
    with the Android loader.
    ([internal MR 2650](https://gitlab.khronos.org/openxr/openxr/merge_requests/2650))
  - Improvement: Separate Android Native App Glue from conformance test
    implementation code.
    ([internal MR 2664](https://gitlab.khronos.org/openxr/openxr/merge_requests/2664))
  - New test: conformance test for interrupting haptics effects with new calls to
    `xrApplyHapticFeedback`.
    ([internal MR 2463](https://gitlab.khronos.org/openxr/openxr/merge_requests/2463),
    [internal MR 2529](https://gitlab.khronos.org/openxr/openxr/merge_requests/2529))
  - New test: Add conformance test for `XR_EXT_local_floor`
    ([internal MR 2503](https://gitlab.khronos.org/openxr/openxr/merge_requests/2503))
  - New test: add check that `XR_KHR_win32_convert_win32_performance_counter` does
    not convert an invalid QPC time
    ([internal MR 2506](https://gitlab.khronos.org/openxr/openxr/merge_requests/2506))
  - New test: Validate `XR_TYPE_VIEW` is checked.
    ([internal MR 2537](https://gitlab.khronos.org/openxr/openxr/merge_requests/2537))
  - New test: Add test for vendor extension `XR_META_headset_id`.
    ([internal MR 2609](https://gitlab.khronos.org/openxr/openxr/merge_requests/2609))

## OpenXR CTS 1.0.26.0 (2022-12-01)

- Registry
  - All changes found in 1.0.23 through 1.0.26.
- Conformance Tests
  - Fix: Warnings raised by Clang on various platforms.
    ([internal MR 2197](https://gitlab.khronos.org/openxr/openxr/merge_requests/2197))
  - Fix: Resolve some warnings and a use-after-move.
    ([internal MR 2224](https://gitlab.khronos.org/openxr/openxr/merge_requests/2224))
  - Fix: Remove references to invalid typeless equivalent for
    `DXGI_FORMAT_R11G11B10_FLOAT`
    ([internal MR 2247](https://gitlab.khronos.org/openxr/openxr/merge_requests/2247))
  - Fix: Remove call to swapbuffers to fix OpenGL frame timing.
    ([internal MR 2249](https://gitlab.khronos.org/openxr/openxr/merge_requests/2249))
  - Fix: Conformance layer issues related to `XR_FB_spatial_entity`
    ([internal MR 2314](https://gitlab.khronos.org/openxr/openxr/merge_requests/2314))
  - Fix: Android conformance initialization detection for platforms without native
    windows.
    ([internal MR 2373](https://gitlab.khronos.org/openxr/openxr/merge_requests/2373))
  - Fix: Not all Android devices have `/sdcard` as a writable path, use the
    `app->activity->externalDataPath` path.
    ([internal MR 2374](https://gitlab.khronos.org/openxr/openxr/merge_requests/2374))
  - Fix: Check function pointer validity before usage in conformance layer to
    verify conformance and avoid segfault
    ([internal MR 2382](https://gitlab.khronos.org/openxr/openxr/merge_requests/2382))
  - Fix: Remove dependency on action space `locationFlags` remaining constant
    between calls to `xrSyncActions` in test "Action Spaces".
    ([internal MR 2424](https://gitlab.khronos.org/openxr/openxr/merge_requests/2424))
  - Fix: Revise some code which generated -Wswitch errors
    ([internal MR 2478](https://gitlab.khronos.org/openxr/openxr/merge_requests/2478))
  - Fix: Correct domain in Android package identifier.
    ([internal MR 2505](https://gitlab.khronos.org/openxr/openxr/merge_requests/2505))
  - Fix: Bump Android Gradle Plugin to 7.0.4 to fix building on M1 device.
    ([OpenXR-CTS PR 43](https://github.com/KhronosGroup/OpenXR-CTS/pull/43))
  - Improvement: Clarify wording of a warning.
    ([internal MR 2196](https://gitlab.khronos.org/openxr/openxr/merge_requests/2196))
  - Improvement: Be more permissive of next chain order in conformance layer.
    ([internal MR 2202](https://gitlab.khronos.org/openxr/openxr/merge_requests/2202))
  - Improvement: Update Catch2 from `2.9.2` to `2.13.9`.
    ([internal MR 2203](https://gitlab.khronos.org/openxr/openxr/merge_requests/2203),
    [internal MR 2372](https://gitlab.khronos.org/openxr/openxr/merge_requests/2372))
  - Improvement: Remove unused `GetRuntimeMajorMinorVersion` function from
    conformance test suite
    ([internal MR 2218](https://gitlab.khronos.org/openxr/openxr/merge_requests/2218))
  - Improvement: Adding `org.khronos.openxr.intent.category.IMMERSIVE_HMD` category
    to intent-filter for `AndroidManifest.xml`, to indicate immersive application
    ([internal MR 2219](https://gitlab.khronos.org/openxr/openxr/merge_requests/2219))
  - Improvement: In case of internal error in the conformance layer, provide some
    logging to support troubleshooting.
    ([internal MR 2224](https://gitlab.khronos.org/openxr/openxr/merge_requests/2224))
  - Improvement: In interactive conformance tests for controllers, improve
    instructions and embed a modified version of the spec diagram of grip and aim
    pose.
    ([internal MR 2228](https://gitlab.khronos.org/openxr/openxr/merge_requests/2228))
  - Improvement: Code/design documentation in the conformance framework.
    ([internal MR 2320](https://gitlab.khronos.org/openxr/openxr/merge_requests/2320))
  - Improvement: General cleanups and code quality improvements.
    ([internal MR 2321](https://gitlab.khronos.org/openxr/openxr/merge_requests/2321),
    [internal MR 2340](https://gitlab.khronos.org/openxr/openxr/merge_requests/2340),
    [internal MR 2341](https://gitlab.khronos.org/openxr/openxr/merge_requests/2341),
    [internal MR 2345](https://gitlab.khronos.org/openxr/openxr/merge_requests/2345),
    [internal MR 2320](https://gitlab.khronos.org/openxr/openxr/merge_requests/2320),
    [internal MR 2380](https://gitlab.khronos.org/openxr/openxr/merge_requests/2380))
  - Improvement: Add Android support for API Layers, and ship the conformance layer
    in the conformance APK.
    ([internal MR 2350](https://gitlab.khronos.org/openxr/openxr/merge_requests/2350))
  - Improvement: Use generated reflection headers to exhaustively check event data
    structures.
    ([internal MR 2357](https://gitlab.khronos.org/openxr/openxr/merge_requests/2357),
    [internal issue 1798](https://gitlab.khronos.org/openxr/openxr/issues/1798))
  - Improvement: Add additional tests for `XR_EXT_debug_utils`.
    ([internal MR 2358](https://gitlab.khronos.org/openxr/openxr/merge_requests/2358))
  - Improvement: Add conformance test for action space creation before bindings
    suggestion.
    ([internal MR 2360](https://gitlab.khronos.org/openxr/openxr/merge_requests/2360),
    [internal issue 1610](https://gitlab.khronos.org/openxr/openxr/issues/1610))
  - Improvement: Add the ability to render arbitrary, simple meshes to all graphics
    plugins, not just cubes.
    ([internal MR 2376](https://gitlab.khronos.org/openxr/openxr/merge_requests/2376),
    [internal issue 1454](https://gitlab.khronos.org/openxr/openxr/issues/1454),
    [internal issue 1776](https://gitlab.khronos.org/openxr/openxr/issues/1776))
  - Improvement: Show velocity/trajectory in Interactive Throw with coordinate axes
    (gnomons)
    ([internal MR 2376](https://gitlab.khronos.org/openxr/openxr/merge_requests/2376),
    [internal issue 1454](https://gitlab.khronos.org/openxr/openxr/issues/1454),
    [internal issue 1776](https://gitlab.khronos.org/openxr/openxr/issues/1776))
  - Improvement: Fix OpenGL stability problems in multithreaded test.
    ([internal MR 2379](https://gitlab.khronos.org/openxr/openxr/merge_requests/2379))
  - Improvement: Add conformance test for interaction profile paths which may be
    system use only.
    ([internal MR 2386](https://gitlab.khronos.org/openxr/openxr/merge_requests/2386))
  - Improvement: Remove `PATH_PREFIX` from RGBAImage as it is unused.
    ([internal MR 2388](https://gitlab.khronos.org/openxr/openxr/merge_requests/2388),
    [internal MR 2436](https://gitlab.khronos.org/openxr/openxr/merge_requests/2436))
  - Improvement: Add support for passing conformance with only a single controller.
    ([internal MR 2415](https://gitlab.khronos.org/openxr/openxr/merge_requests/2415),
    [internal issue 1812](https://gitlab.khronos.org/openxr/openxr/issues/1812),
    [internal MR 2445](https://gitlab.khronos.org/openxr/openxr/merge_requests/2445),
    [internal MR 2484](https://gitlab.khronos.org/openxr/openxr/merge_requests/2484))
  - Improvement: Adjust two REQUIRES in `xrAttachSessionActionSets` test to just
    require success rather than `XR_SUCCESS` (unconditional success)
    ([internal MR 2442](https://gitlab.khronos.org/openxr/openxr/merge_requests/2442))
  - Improvement: Simplify platform plugin for CTS for POSIX platforms.
    ([internal MR 2443](https://gitlab.khronos.org/openxr/openxr/merge_requests/2443),
    [internal MR 2436](https://gitlab.khronos.org/openxr/openxr/merge_requests/2436))
  - Improvement: Clarify a warning.
    ([internal MR 2469](https://gitlab.khronos.org/openxr/openxr/merge_requests/2469))
  - Improvement: Add `android.permission.VIBRATE` permission needed by some
    runtimes for the controller haptics.
    ([internal MR 2486](https://gitlab.khronos.org/openxr/openxr/merge_requests/2486))
  - Improvement: Add `[no_auto]` tag to indicate a interactive tests without
    `XR_EXT_conformance_automation` support.
    ([internal MR 2487](https://gitlab.khronos.org/openxr/openxr/merge_requests/2487))
  - Improvement: Comment out unused variable in palm pose test.
    ([OpenXR-CTS PR 40](https://github.com/KhronosGroup/OpenXR-CTS/pull/40))
  - New test: Add tests for `XR_EXT_palm_pose`
    ([internal MR 2112](https://gitlab.khronos.org/openxr/openxr/merge_requests/2112))
  - New test: Add tests for `XR_EXT_dpad_binding`.
    ([internal MR 2159](https://gitlab.khronos.org/openxr/openxr/merge_requests/2159),
    [internal MR 2271](https://gitlab.khronos.org/openxr/openxr/merge_requests/2271))
  - New test: Add a conformance test for `XR_EXT_eye_gaze_interaction` extension.
    ([internal MR 2202](https://gitlab.khronos.org/openxr/openxr/merge_requests/2202))
  - New test: Add additional tests for `XR_KHR_convert_timespec_time` and
    `XR_KHR_win32_convert_performance_counter_time`
    ([internal MR 2238](https://gitlab.khronos.org/openxr/openxr/merge_requests/2238))
  - New test: Add conformance test for `XR_EXT_debug_utils`
    ([internal MR 2242](https://gitlab.khronos.org/openxr/openxr/merge_requests/2242))
  - New test: Add dedicated conformance test for `xrLocateViews`
    ([internal MR 2300](https://gitlab.khronos.org/openxr/openxr/merge_requests/2300),
    [internal issue 1738](https://gitlab.khronos.org/openxr/openxr/issues/1738))
  - New test: Add interactive conformance test for `XR_KHR_visibility_mask`
    ([internal MR 2343](https://gitlab.khronos.org/openxr/openxr/merge_requests/2343),
    [internal MR 2472](https://gitlab.khronos.org/openxr/openxr/merge_requests/2472))
  - New test: Add conformance test for consistency of suggested bindings order.
    ([internal MR 2359](https://gitlab.khronos.org/openxr/openxr/merge_requests/2359))
  - New test: Add conformance test for vendor extension
    `XR_META_performance_metrics`.
    ([internal MR 2422](https://gitlab.khronos.org/openxr/openxr/merge_requests/2422))
  - ci: Publish Android APK files on releases
    ([OpenXR-CTS PR 45](https://github.com/KhronosGroup/OpenXR-CTS/pull/45))

## OpenXR CTS 1.0.22.1 (Approved 2022-01-13)

- Registry
  - All changes found in 1.0.15 through 1.0.22.
- Conformance Tests
  - Fix: Do not require optional extensions on Android.
    ([internal MR 1949](https://gitlab.khronos.org/openxr/openxr/merge_requests/1949),
    [internal issue 1480](https://gitlab.khronos.org/openxr/openxr/issues/1480),
    [internal issue 1481](https://gitlab.khronos.org/openxr/openxr/issues/1481),
    [OpenXR-CTS issue 5](https://github.com/KhronosGroup/OpenXR-CTS/issues/5),
    [OpenXR-CTS issue 6](https://github.com/KhronosGroup/OpenXR-CTS/issues/6))
  - Fix: Resolve invalid handle error on `xrEnumerateBoundSourcesForAction` in
    multithreaded test.
    ([internal MR 2094](https://gitlab.khronos.org/openxr/openxr/merge_requests/2094))
  - Fix: Check graphics plugin usage to allow `XR_MND_headless` to be used with
    non-interactive conformance tests.
    ([internal MR 2163](https://gitlab.khronos.org/openxr/openxr/merge_requests/2163))
  - Fix: Vulkan validation and OpenGL context usage issues in conformance test
    suite.
    ([internal MR 2165](https://gitlab.khronos.org/openxr/openxr/merge_requests/2165))
  - Fix: Update Catch2 from `2.9.2` to `2.13.8` to fix builds on newer Linux
    distributions.
    ([internal MR 2203](https://gitlab.khronos.org/openxr/openxr/merge_requests/2203))
  - Fix: Add more formats to pick from in OpenGL; do not use sRGB as rendering is
    broken with that.
    ([OpenXR-CTS PR 20](https://github.com/KhronosGroup/OpenXR-CTS/pull/20))
  - Fix: Make "Grip and Aim Pose" and "Projection Mutable Field-of-View" tests
    visible.
    ([OpenXR-CTS PR 21](https://github.com/KhronosGroup/OpenXR-CTS/pull/21))
  - Fix: Read access violation for D3D12 device on shutdown.
    ([OpenXR-CTS PR 22](https://github.com/KhronosGroup/OpenXR-CTS/pull/22))
  - Fix: Mutable field-of-view X and Y flip for non-symmetrical FOVs.
    ([OpenXR-CTS PR 23](https://github.com/KhronosGroup/OpenXR-CTS/pull/23))
  - Fix: Make some failures caused by missing layer flag implementations more
    obvious.
    ([OpenXR-CTS PR 25](https://github.com/KhronosGroup/OpenXR-CTS/pull/25))
  - Fix: Converted all conformance tests to use SRGB 8-bit textures since some
    runtimes don't support linear 8-bit textures.
    ([OpenXR-CTS PR 26](https://github.com/KhronosGroup/OpenXR-CTS/pull/26))
  - Fix: Avoid submitting projection layers when the referenced swapchain hasn't
    been used yet.
    ([OpenXR-CTS PR 27](https://github.com/KhronosGroup/OpenXR-CTS/pull/27))
  - Improvement: Add Android build system, using new cross-vendor Android loader,
    and fix some runtime errors in Android-specific code.
    ([internal MR 1949](https://gitlab.khronos.org/openxr/openxr/merge_requests/1949),
    [internal issue 1425](https://gitlab.khronos.org/openxr/openxr/issues/1425))
  - Improvement: Use Asset Manager for assets on Android, and fix build.
    ([internal MR 1950](https://gitlab.khronos.org/openxr/openxr/merge_requests/1950))
  - Improvement: Refactor `xrGetInstanceProcAddr` implementation in conformance
    layer to avoid deeply-nested `if ... else` blocks. (Some compilers have limits
    we were nearing or hitting.)
    ([internal MR 2050](https://gitlab.khronos.org/openxr/openxr/merge_requests/2050))
  - Improvement: Add device reuse test to `XR_KHR_D3D11_enable` test.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - Improvement: Add device reuse test to `XR_KHR_D3D12_enable` test.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - Improvement: Add device reuse test to `XR_KHR_opengl_enable` test.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - Improvement: Add support for `XR_KHR_vulkan_enable2` to conformance test suite.
    ([internal MR 2073](https://gitlab.khronos.org/openxr/openxr/merge_requests/2073))
  - Improvement: Add tests for `xrApplyHapticFeedback` and `xrLocateSpace` to
    `multithreading` test in the conformance test suite.
    ([internal MR 2077](https://gitlab.khronos.org/openxr/openxr/merge_requests/2077))
  - Improvement: Add swapchain create and destroy test to graphics enable tests.
    ([internal MR 2086](https://gitlab.khronos.org/openxr/openxr/merge_requests/2086))
  - Improvement: Check `XrPerfSettings*EXT` enums in conformance layer, which also
    solves a compiler warning.
    ([internal MR 2107](https://gitlab.khronos.org/openxr/openxr/merge_requests/2107))
  - Improvement: Shutdown graphics plugin after running tests in the conformance
    test suite.
    ([internal MR 2132](https://gitlab.khronos.org/openxr/openxr/merge_requests/2132))
  - Improvement: Implement D3D11 graphics validator to validate usage flags.
    ([internal MR 2139](https://gitlab.khronos.org/openxr/openxr/merge_requests/2139))
  - Improvement: Adjust interactive tests to keep submitting frames while waiting,
    to avoid missing many frames while doing input-related tests.
    ([internal MR 2142](https://gitlab.khronos.org/openxr/openxr/merge_requests/2142))
  - Improvement: Include NVIDIA-defined and AMD-defined exported symbols to signal
    favoring high performance/discrete graphics devices for test application.
    ([internal MR 2156](https://gitlab.khronos.org/openxr/openxr/merge_requests/2156))
  - Improvement: Fix Android building and add documentation on building for
    Android.
    ([OpenXR-CTS PR 33](https://github.com/KhronosGroup/OpenXR-CTS/pull/33),
    [OpenXR-CTS issue 31](https://github.com/KhronosGroup/OpenXR-CTS/issues/31),
    [internal MR 2198](https://gitlab.khronos.org/openxr/openxr/merge_requests/2198))
  - New test: Add `XR_KHR_vulkan_enable` test to validate simple failure and
    success cases.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - New test: Add `XR_KHR_opengl_es_enable` test to validate simple failure and
    success cases.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - New test: Add `XR_KHR_vulkan_enable2` test to validate simple failure and
    success cases.
    ([internal MR 2073](https://gitlab.khronos.org/openxr/openxr/merge_requests/2073))
  - New test: Add tests for `XR_EXT_hand_tracking` to validate basic API usage.
    ([internal MR 2164](https://gitlab.khronos.org/openxr/openxr/merge_requests/2164))

## OpenXR CTS 1.0.14.1 (2021-01-27)

Note that the procedure for generating your conformance submission has changed slightly.

- Registry
  - All changes found in 1.0.13, and 1.0.14.
- Conformance Tests
  - Build: Initial setup of CMake for conformance build on Android. (Not complete -
    no gradle part.)
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - Fix: Properly apply function attributes to fix build of conformance layer on
    Android for ARM.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910),
    [OpenXR-CTS/#3](https://github.com/KhronosGroup/OpenXR-CTS/issues/3),
    [internal issue 1479](https://gitlab.khronos.org/openxr/openxr/issues/1479))
  - Fix: Use `android.app.NativeActivity` correctly in place of NativeActivity
    subclass for the conformance tests.
    ([internal MR 1965](https://gitlab.khronos.org/openxr/openxr/merge_requests/1965),
    [internal MR 1976](https://gitlab.khronos.org/openxr/openxr/merge_requests/1976))
  - Fix: The D3D12, OpenGL, and Vulkan graphics plugins sometimes did not update
    their swapchain image context maps due to rare key collisions.
    ([OpenXR-CTS/#4](https://github.com/KhronosGroup/OpenXR-CTS/pull/4))
  - Fix: Removed extra check that would fail if the CTS was compiled against a
    version of `openxr.h` that included extensions that added results that were
    newer than the results present in the version of `openxr.h` that the runtime
    was compiled against.
    ([OpenXR-CTS/#8](https://github.com/KhronosGroup/OpenXR-CTS/pull/8))
  - Fix: Fixed conformance failures on runtimes where x and y components can have
    different last changed times
    ([OpenXR-CTS/#9](https://github.com/KhronosGroup/OpenXR-CTS/pull/9))
  - Fix: "Projection Mutable Field-of-View" was an old, broken version of the test.
    It is reset to the intended version of the code now.
    ([OpenXR-CTS/#10](https://github.com/KhronosGroup/OpenXR-CTS/pull/10))
  - Fix: `CopyRGBAImage` was using the wrong array slice when setting image
    barriers. This broke the "Subimage Tests" case on some hardware/drivers.
    ([OpenXR-CTS/#11](https://github.com/KhronosGroup/OpenXR-CTS/pull/11))
  - Fix: Added `WaitForGpu` call at the end of `RenderView` in
    `D3D12GraphicsPlugin`. Without this the array and wide swapchain tests failed
    on some hardware/driver versions. This is not ideal behavior, but it fixes the
    test for now, and has been noted for future fixing in a better way.
    ([OpenXR-CTS/#12](https://github.com/KhronosGroup/OpenXR-CTS/pull/12))
  - Fix: Allow negated quaternion to be equivalent to the expected orientation
    value. This test only cares about orientation, not which path the rotation
    took.
    ([OpenXR-CTS/#13](https://github.com/KhronosGroup/OpenXR-CTS/pull/13))
  - Fix: The test assumed that X and Y components of a vector2 action would have
    exactly the same timestamp. Changed that to check that the vector2 action would
    have the most recent of those two timestamps instead.
    ([OpenXR-CTS/#14](https://github.com/KhronosGroup/OpenXR-CTS/pull/14),
    [internal issue 1490](https://gitlab.khronos.org/openxr/openxr/issues/1490))
  - Fix: The test checked for "float value == X" and then checked for its binary
    thresholded value after a subsequent call to xrSyncActions. Changed the test so
    both checks happen in the same input frame.
    ([OpenXR-CTS/#14](https://github.com/KhronosGroup/OpenXR-CTS/pull/14))
  - Fix: Added support for `GL_SRGB8` textures to OpenGL tests.
    ([OpenXR-CTS/#15](https://github.com/KhronosGroup/OpenXR-CTS/pull/15))
  - Fix: Increased allowed pipeline overhead threshold to 50%. The purpose of the
    test is to ensure the runtime isn't serializing (100% overhead) so this is a
    safe increase.
    ([OpenXR-CTS/#16](https://github.com/KhronosGroup/OpenXR-CTS/pull/16))
  - Fix: Use `int64_t` for YieldSleep calculations, to not overflow.
    ([OpenXR-CTS/#17](https://github.com/KhronosGroup/OpenXR-CTS/pull/17))
  - Improvement: Modify the test instructions to change the output format and
    increase its content, and require the console output to be submitted as well.
    This makes it easier to review and provides a more complete look at both passed
    and failed tests.
    ([internal MR 1953](https://gitlab.khronos.org/openxr/openxr/merge_requests/1953))
  - Lower the amount of time that the renderer blocks. The CTS is not a highly
    optimized application and due to thread scheduling and extra GPU waits 90% CPU
    wait makes it fairly tight to fit everything inside of a single display period.
    ([OpenXR-CTS/#18](https://github.com/KhronosGroup/OpenXR-CTS/pull/18))
  - New test: Verify that triangles returned from `XR_KHR_visibility_mask` are
    counter-clockwise.
    ([internal MR 1943](https://gitlab.khronos.org/openxr/openxr/merge_requests/1943))
  - New test: Add tests for `xrBeginFrame` call order violations
    ([OpenXR-CTS/#7](https://github.com/KhronosGroup/OpenXR-CTS/pull/7))

## OpenXR CTS 1.0.12.1 (2020-10-01)

- Registry
  - All changes found in 1.0.10, 1.0.11, and 1.0.12.
- Conformance Tests
  - Fix: Fix Vulkan image layout transitions
    ([internal MR 1876](https://gitlab.khronos.org/openxr/openxr/merge_requests/1876))
  - Fix: Images were being copied upside-down in OpenGL ES.
    ([internal MR 1899](https://gitlab.khronos.org/openxr/openxr/merge_requests/1899))
  - Fix: Issues around `xrInitializeLoaderKHR`.
    ([internal MR 1922](https://gitlab.khronos.org/openxr/openxr/merge_requests/1922))
  - Fix: Fix some interactive tests like "Grip and Aim Pose" which exposed D3D12
    validation errors due to bugs in the D3D12 graphics plugin.
    ([OpenXR-CTS/#1](https://github.com/KhronosGroup/OpenXR-CTS/pull/1))
  - Fix: Use `REQUIRE` macro in main thread only, in `Timed Pipelined Frame
    Submission` to prevent race condition.
    ([OpenXR-CTS/#2](https://github.com/KhronosGroup/OpenXR-CTS/pull/2))
  - Improvement: Enable use of glslangValidator to compile shaders if shaderc is
    not available.
    ([internal MR 1857](https://gitlab.khronos.org/openxr/openxr/merge_requests/1857))
  - Improvement: Add options to not run tests that require disconnecting devices.
    ([internal MR 1862](https://gitlab.khronos.org/openxr/openxr/merge_requests/1862))
  - Improvement: `xrStructureTypeToString`, `xrResultToString`: Make test more
    lenient, so it will also accept the "generic" generated value for enumerant
    values defined by an extension that is not currently enabled.
    ([internal MR 1864](https://gitlab.khronos.org/openxr/openxr/merge_requests/1864))
  - Improvement: Improve language usage in code and comments to be more respectful.
    ([internal MR 1881](https://gitlab.khronos.org/openxr/openxr/merge_requests/1881))
  - Improvement: Handle the new `XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING`
    return code, and move all checks for this code to the `xrCreateSession` test,
    from the individual graphics bindings tests.
    ([internal MR 1882](https://gitlab.khronos.org/openxr/openxr/merge_requests/1882),
    [OpenXR-Docs/#53](https://github.com/KhronosGroup/OpenXR-Docs/issues/53),
    [internal issue 1397](https://gitlab.khronos.org/openxr/openxr/issues/1397))
  - Improvement: Automatically enable a number of extensions, if present, that can
    be enabled without side-effects.
    ([internal MR 1897](https://gitlab.khronos.org/openxr/openxr/merge_requests/1897))
  - Improvement: Use the `XR_KHR_loader_init_android` extension on Android instead
    of vendor-specific code.
    ([internal MR 1903](https://gitlab.khronos.org/openxr/openxr/merge_requests/1903))

## OpenXR CTS 1.0.9.2 (2020-06-18)

- Registry
  - No significant changes
- Conformance Tests
  - New test: Add test for quads composed in hand spaces.
    ([internal MR 1767](https://gitlab.khronos.org/openxr/openxr/merge_requests/1767))
  - New test: Add test for mutable field-of-view composition.
    ([internal MR 1830](https://gitlab.khronos.org/openxr/openxr/merge_requests/1830))
  - Improvement: Pre-fill action state structs a few times with non-zero data.
    ([internal MR 1828](https://gitlab.khronos.org/openxr/openxr/merge_requests/1828))
  - Improvement: interactive action test improvements.
    ([internal MR 1771](https://gitlab.khronos.org/openxr/openxr/merge_requests/1771))
  - Improvement: Test that `xrBeginFrame` returns `XR_FRAME_DISCARDED` after a failed
    `xrEndFrame`
    ([internal MR 1775](https://gitlab.khronos.org/openxr/openxr/merge_requests/1775))
  - Improvement: Avoid clearing buffers in Vulkan RenderView
    ([internal MR 1777](https://gitlab.khronos.org/openxr/openxr/merge_requests/1777))
  - Improvement: Switch from `aim` to `grip` for interactive throw test.
    ([internal MR 1781](https://gitlab.khronos.org/openxr/openxr/merge_requests/1781))
  - Improvement: Be more strict with invalid enum values for view configuration types.
    ([internal MR 1792](https://gitlab.khronos.org/openxr/openxr/merge_requests/1792))
  - Improvement: Be more strict with invalid enum values for view configuration types.
    ([internal MR 1793](https://gitlab.khronos.org/openxr/openxr/merge_requests/1793))
  - Improvement: Reduce complexity of "State query functions" interactive test.
    ([internal MR 1813](https://gitlab.khronos.org/openxr/openxr/merge_requests/1813))
  - Improvement: Get conformance running on Android.
    ([internal MR 1819](https://gitlab.khronos.org/openxr/openxr/merge_requests/1819))
  - Improvement: Add turn-on message to "Repeated state query calls return the same
    value" test.
    ([internal MR 1821](https://gitlab.khronos.org/openxr/openxr/merge_requests/1821))
  - Fix: Remove protected content bit from D3D depth formats
    ([internal MR 1820](https://gitlab.khronos.org/openxr/openxr/merge_requests/1820))
  - Fix: Reset the `D3D12CommandAllocator` every frame to fix the memory leak in
    the conformance test
    ([internal MR 1803](https://gitlab.khronos.org/openxr/openxr/merge_requests/1803))
  - Fix: Fix conformance for UWP
    ([internal MR 1807](https://gitlab.khronos.org/openxr/openxr/merge_requests/1807),
    [internal MR 1815](https://gitlab.khronos.org/openxr/openxr/merge_requests/1815))
  - Fix: Gamepad conformance
    ([internal MR 1808](https://gitlab.khronos.org/openxr/openxr/merge_requests/1808))
  - Fix: use sRGB swapchain format for PNGs.
    ([internal MR 1809](https://gitlab.khronos.org/openxr/openxr/merge_requests/1809))
  - Fix: `conformance_cli` exit code
    ([internal MR 1810](https://gitlab.khronos.org/openxr/openxr/merge_requests/1810))
  - Fix: texture row pitch in D3D12 graphics plugin
    ([internal MR 1811](https://gitlab.khronos.org/openxr/openxr/merge_requests/1811))
  - Fix: Avoid non-positive numbers for depth extension.
    ([internal MR 1776](https://gitlab.khronos.org/openxr/openxr/merge_requests/1776))
  - Fix: Get OpenGL plugin working
    ([internal MR 1777](https://gitlab.khronos.org/openxr/openxr/merge_requests/1777),
    [internal MR 1790](https://gitlab.khronos.org/openxr/openxr/merge_requests/1790),
    [internal MR 1791](https://gitlab.khronos.org/openxr/openxr/merge_requests/1791))
  - Fix: Do not send blend mode `MAX_ENUM` to runtime.
    ([internal MR 1782](https://gitlab.khronos.org/openxr/openxr/merge_requests/1782),
    [internal issue 1362](https://gitlab.khronos.org/openxr/openxr/issues/1362))
  - Fix: Fix `VkClearRect` in Vulkan plugin.
    ([internal MR 1829](https://gitlab.khronos.org/openxr/openxr/merge_requests/1829))

## OpenXR CTS 1.0.9 (2020-05-29)

Internal release, corresponding to the first voted-upon conformance suite.
