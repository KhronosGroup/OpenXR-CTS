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
