# Changelog for OpenXR-CTS Repo

<!--
Copyright (c) 2020-2022, The Khronos Group Inc.

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
  - Improvement: Add device re-use test to `XR_KHR_D3D11_enable` test.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - Improvement: Add device re-use test to `XR_KHR_D3D12_enable` test.
    ([internal MR 2054](https://gitlab.khronos.org/openxr/openxr/merge_requests/2054))
  - Improvement: Add device re-use test to `XR_KHR_opengl_enable` test.
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
