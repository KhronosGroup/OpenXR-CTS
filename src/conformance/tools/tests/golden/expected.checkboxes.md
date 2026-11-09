# CTS Submission Summary

- XML files parsed: 2
- Failures: 1
- Errors: 1
- Skips: 2
- Warnings: 1

## Review Notes

- **Submission completeness: 6 issue(s) found.**
- **Failures or errors are present**; inspect the detail sections.
- Metadata differs across XML files: Runtime instance properties, Available instance extensions.
- 1 notable skip(s) (and 1 likely-trivial) -- see Skips.

## Submission Completeness

- [ ] `XR_KHR_vulkan_enable2` is advertised, but no automated run used graphicsPlugin `Vulkan2`.
- [ ] `XR_KHR_vulkan_enable2` is advertised, but no `[composition][interactive]` run used graphicsPlugin `Vulkan2`.
- [ ] `XR_KHR_D3D11_enable` is advertised, but no automated run used graphicsPlugin `D3D11`.
- [ ] `XR_KHR_D3D11_enable` is advertised, but no `[composition][interactive]` run used graphicsPlugin `D3D11`.
- [ ] No `[scenario][interactive]` run found in this submission.
- [ ] No `[actions][interactive]` run found in this submission.

## Runs

| File | Step | Graphics | Filters | Success | Failure | Skipped | Errors |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: |
| automated_vulkan.xml | automated | Vulkan | exclude:[interactive] | 1 | 1 | 2 | 0 |
| composition_vulkan.xml | composition | Vulkan | [composition][interactive] | 1 | 0 | 0 | 1 |

## Failures

- [ ] `automated_vulkan.xml` `FailingCase` at `test_Fail.cpp:99`

```text
FAILED:
CHECK(false)
at /tmp/openxr-cts/src/conformance/conformance_test/test_Fail.cpp:99
```

## Errors

- [ ] `composition_vulkan.xml` `SourceCodeRevision` at `conformance_test.cpp(180)`

```text
FAILED:
  REQUIRE_NOTHROW( [] { throw std::runtime_error("injected for golden test fixture"); }() )
injected for golden test fixture
at C:\openxr-cts\src\conformance\conformance_test\conformance_test.cpp(180)
```

## Skips

### Notable

- [ ] **1 skip(s)** at `test_XR_EXT_eye_gaze_interaction.cpp:44`
  - Message: System does not support eye gaze interaction
  - Tests:
    - `EyeGazeSkip`

### Likely-trivial

Skips of tests tagged with at least one `XR_*` feature the runtime doesn't advertise (extension absent, or `XR_VERSION_M_N` higher than any queried apiVersion).

- [ ] **1 skip(s)** at `test_XR_EXT_dpad_binding.cpp:796`
  - Message: XR_EXT_dpad_binding not supported
  - Tests:
    - `DpadBindingSkip`

## Warnings

- [ ] **1 warning(s)** at `test_Warn.cpp:12`
  - Inputs: `automated_vulkan.xml` (1x)
  - Messages: Conformance layer warning: hello (1x)

## Extensions

| Extension | Status | Tagged tests | Skipped | Failed |
| --- | --- | ---: | ---: | ---: |
| `XR_EXT_dpad_binding` | not advertised, tagged tests present | 1 | 1 | 0 |
| `XR_EXT_eye_gaze_interaction` | advertised, all tagged tests skipped | 1 | 1 | 0 |
| `XR_EXT_palm_pose` | advertised, no tagged tests seen | 0 | 0 | 0 |
| `XR_KHR_D3D11_enable` | advertised, no tagged tests seen | 0 | 0 | 0 |
| `XR_KHR_vulkan_enable` | advertised, no tagged tests seen | 0 | 0 | 0 |
| `XR_KHR_vulkan_enable2` | advertised, no tagged tests seen | 0 | 0 | 0 |

## Metadata Inconsistencies

- [ ] **Runtime instance properties**
  - Shared by all inputs: (none)
  - automated_vulkan.xml adds:
    - `(api=1.0.0, rv=1.2.3, name='Synthetic Runtime')`
  - composition_vulkan.xml adds:
    - `(api=1.0.0, rv=1.2.4, name='Synthetic Runtime')`
- [ ] **Available instance extensions**
  - Shared by all inputs:
    - `XR_EXT_eye_gaze_interaction`
    - `XR_EXT_palm_pose`
    - `XR_KHR_D3D11_enable`
    - `XR_KHR_vulkan_enable`
  - automated_vulkan.xml adds: (nothing beyond baseline)
  - composition_vulkan.xml adds:
    - `XR_KHR_vulkan_enable2`
