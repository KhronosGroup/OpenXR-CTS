# Changelog for OpenXR-CTS Repo

Update log for the OpenXR-CTS repo on GitHub. Updates are
in reverse chronological order starting with the latest public release.

This summarizes the periodic public releases, not individual commits. Releases
on GitHub are generally done as single large patches at the release point,
collecting together the resolution of many Khronos internal issues, along with
any public pull requests that have been accepted. In this repository in
particular, since it is primarily software, pull requests may be integrated as
they are accepted even between periodic updates. However, versions that are not
signed tags on the `approved` branch are not valid for conformance submission.

## OpenXR CTS 1.0.9.2 (2020-06-18)

- Registry
  - No significant changes
- Conformance Tests
  - New test: Add test for quads composed in hand spaces.
    ([internal MR 1767](https://gitlab.khronos.org/openxr/openxr/merge_requests/1767))
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
