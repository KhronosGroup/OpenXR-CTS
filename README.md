OpenXR Conformance Test Suite
=============================

<!--
Copyright (c) 2019-2023, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

OpenXR Conformance Test Suite is a collection of tests covering the breadth of
the OpenXR API. Some tests have been grouped by tags depending on the
involvement of the tester/invoker (e.g. `[interactive]`) and the area of the
test (e.g. `[composition]` and `[actions]`). Interactive tests for validating
layer composition must run for all graphics APIs supported by the runtime and
action tests must run for all interaction profiles supported by the runtime. For
this reason the suite of tests is run multiple times with different
configurations.

`conformance_cli`, a command-line interface application, is provided for running
on PCs and other devices and platforms which support this form of application.
`conformance_cli` also demonstrates how to build an application which can
interop with the `conformance_test` shared library. If the device being tested
does not support a command-line interface, a host application must be built for
the device which the OpenXR runtime will run on. The conformance host must
invoke `conformance_test`, the test suite shared library.

When you plan to submit for conformance, you must observe a few considerations
to ensure that the build system has accurate source code revision information
available to embed in the test suite and output reports. You must build from a
git repo (forked from either the internal Gitlab or public GitHub) with tags
available (a full clone, not shallow). You also must either perform a clean
build, from an empty binary tree, or at least run `cmake` immediately before
building to pick up current source tree status. If your "porting" process (as
described by the conformance process documents) involves replacing the build
system, you must populate the revision data constants in
`utilities/git_revision.cpp.in` accurately. The contents of that file affect all
"ctsxml" format outputs, as well as an automated "SourceCodeRevision" test that
warns if it cannot identify an approved release. (It only checks for the
presence of an appropriately-named tag: it does not check for a signature on the
tag, so if you have added tags to your repo it may not warn if you are not on a
release.)

Running CTS
-----------

For each run, you will also need to preserve the console output (or equivalent)
as it is important for interpreting the results.

1. Run the automated tests (non-interactive tests) for every graphics API that is supported.

    Example:

        conformance_cli "exclude:[interactive]" -G d3d11 --reporter ctsxml::out=automated_d3d11.xml
        conformance_cli "exclude:[interactive]" -G d3d12 --reporter ctsxml::out=automated_d3d12.xml
        conformance_cli "exclude:[interactive]" -G vulkan --reporter ctsxml::out=automated_vulkan.xml
        conformance_cli "exclude:[interactive]" -G vulkan2 --reporter ctsxml::out=automated_vulkan2.xml
        conformance_cli "exclude:[interactive]" -G opengl --reporter ctsxml::out=automated_opengl.xml

    Notes:
    * Some tests require that a begun session progresses to `XR_SESSION_STATE_FOCUSED`.
    * Some tests require valid view tracking (`XR_VIEW_STATE_ORIENTATION_VALID_BIT & XR_VIEW_STATE_POSITION_VALID_BIT`).
    * If you need to specify a different environment blend mode than
      `XR_ENVIRONMENT_BLEND_MODE_OPAQUE`, pass something like the following
      additionally: `-B Additive` or `--environmentBlendMode Additive`

2. Run the interactive composition tests for every graphics API that is supported.

    Example:

        conformance_cli "[composition][interactive]" -G d3d11 --reporter ctsxml::out=interactive_composition_d3d11.xml
        conformance_cli "[composition][interactive]" -G d3d12 --reporter ctsxml::out=interactive_composition_d3d12.xml
        conformance_cli "[composition][interactive]" -G vulkan --reporter ctsxml::out=interactive_composition_vulkan.xml
        conformance_cli "[composition][interactive]" -G vulkan2 --reporter ctsxml::out=interactive_composition_vulkan2.xml
        conformance_cli "[composition][interactive]" -G opengl --reporter ctsxml::out=interactive_composition_opengl.xml

    Notes:
    * The runtime must support `khr/simple_controller` to manually pass or fail
      each test through the `input/menu/click` and `input/select/click` input
      paths. The tester must evaluate the composed output and pass or fail the
      tests by comparing it to the provided expected result image. While
      `khr/simple_controller` isn't required for conformance, it is strongly
      encouraged. If it cannot be included for some reason, use of another
      interaction profile may be performed through the porting process

3. Run the interactive scenario tests. Run the tests for at least one graphics API.

    Example:

        conformance_cli "[scenario][interactive]" -G opengl --reporter ctsxml::out=interactive_scenarios.xml

    Notes:
    * The runtime must support `khr/simple_controller`. If it cannot be included
      for some reason, use of another interaction profile may be performed
      through the porting process

4. Run the interactive action tests for every interaction profile your runtime
   can bind completely to a device. Run the tests for at least one graphics API.

   Example:

        conformance_cli "[actions][interactive]" -G d3d11 -I "khr/simple_controller" --reporter ctsxml::out=interactive_action_simple_controller.xml
        conformance_cli "[actions][interactive]" -G d3d11 -I "microsoft/motion_controller" --reporter ctsxml::out=interactive_action_microsoft_motion_controller.xml
        conformance_cli "[actions][interactive]" -G d3d11 -I "oculus/touch_controller" --reporter ctsxml::out=interactive_action_oculus_touch_controller.xml
        conformance_cli "[actions][interactive]" -G d3d11 -I "htc/vive_controller" --reporter ctsxml::out=interactive_action_htc_vive_controller.xml

   Note that the `microsoft/xbox_controller` interaction profile only needs to
   run against the `[gamepad]` tests:

        conformance_cli "[gamepad]" -G d3d11 -I "microsoft/xbox_controller" --reporter ctsxml::out=interactive_action_microsoft_xbox_controller.xml

   Notes:
   * A person must use the OpenXR action system input by following the displayed
     instructions.
   * The interaction profile paths specified with `-I` must have the
     "/interaction_profile/" prefix stripped to avoid a parsing bug in Catch2.

5. Bundle up the conformance test suite output XML files, the console output,
   and the other requirements of the OpenXR section of the Conformance Process
   document <https://www.khronos.org/conformance/adopters>, and submit to
   Khronos for review and approval by the OpenXR Working Group.

Android Specifics
-----------------

The APK includes a NativeActivity that runs the CTS tests, along with the
Conformance Layer. The activity accepts the equivalent of the command line
arguments described above using "Intent Extras" instead. `adb shell` may be used
to start the CTS and pass intent extras, for example as follows:

     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "exclude:[interactive]" -e graphicsPlugin vulkan -e xmlFilename automated_vulkan.xml

which is the rough equivalent of the Vulkan-related command in item 1 above.

* `--esa` specifies a "string array" extra. The activity checks for one named
  `args` to treat as command line arguments to pass to the core test. This is
  comma-delimited, a comma may be escaped using `\,` per the `am` documentation.
* After processing the string array, if any, a number of the string-related
  command line arguments are checked as their own separate string intent extras.
  For example, you may specify the graphics plugin as Vulkan by
  `-e graphicsPlugin vulkan`. The extra name is the "full" option name. Any of
  these found to be non-empty are appended to the end of the simulated command
  line arguments.
* If a graphics plugin has not been specified in the preceding steps, arguments
  are added to use the OpenGLES plugin by default, to make usage from launchers
  easier.
* Finally, unless a boolean extra named `skipXml` is found and true, arguments
  are added to generate a file with xml output in the application's writable
  data. You can see this path in `logcat` and retrieve it from the device using
  `adb` or other methods (USB file transfer mode, etc.). The filename may be set
  using a string intent extra named `xmlFilename`.

You will need to translate the sample command lines in the preceding section to
this format using intent extras, or create a launcher activity that generates
those intents. Samples translated include:

     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "exclude:[interactive]" -e graphicsPlugin vulkan -e xmlFilename automated_vulkan.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "exclude:[interactive]" -e graphicsPlugin vulkan2 -e xmlFilename automated_vulkan2.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "exclude:[interactive]" -e graphicsPlugin opengles -e xmlFilename automated_opengles.xml

     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[composition][interactive]" -e graphicsPlugin vulkan -e xmlFilename interactive_composition_vulkan.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[composition][interactive]" -e graphicsPlugin vulkan2 -e xmlFilename interactive_composition_vulkan2.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[composition][interactive]" -e graphicsPlugin opengles -e xmlFilename interactive_composition_opengles.xml

     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[scenario][interactive]" -e xmlFilename interactive_scenarios.xml

     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[actions][interactive],-I,khr/simple_controller" -e xmlFilename interactive_action_simple_controller.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[actions][interactive],-I,microsoft/motion_controller" -e xmlFilename interactive_action_microsoft_motion_controller.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[actions][interactive],-I,oculus/touch_controller" -e xmlFilename interactive_action_oculus_touch_controller.xml
     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[gamepad],-I,htc/vive_controller" -e xmlFilename interactive_action_htc_vive_controller.xml

     adb shell am start-activity -S -n org.khronos.openxr.cts/android.app.NativeActivity --esa args "[actions][interactive],-I,microsoft/xbox_controller" -e xmlFilename interactive_action_microsoft_xbox_controller.xml

If you need to specify a different environment blend mode than
`XR_ENVIRONMENT_BLEND_MODE_OPAQUE`, pass something like the following
additionally: `-e environmentBlendMode Additive`

Alternatively, you may pass space-separated args using
`setprop debug.xr.conform.args "args here"` as in earlier versions of the CTS;
however, this does not allow you to specify arguments with spaces and does not
allow you to set the output filename. A property set this way persists until the
device restarts.

Interactive self-tests
----------------------

Some interactive tests are primarily a test of mechanisms within the CTS, rather
than runtime functionality. These are labeled with the tag `[self_test]` rather
than `[scenario]`, `[actions]`, or `[composition]`. While it is good to run
these, and doing so may help troubleshoot failures with tests that build on,
submission of a CTS results package. Currently, the only self-tests are for the
PBR/glTF rendering subsystem. They synchronously load very large, artificial
test assets, originally from the "glTF-Sample-Models" repository, to test
specific details of the renderer.

To run the self-tests, commands similar to the following can be used:

        conformance_cli "[self_test][interactive]" -G d3d11 --reporter ctsxml::out=interactive_self_test_d3d11.xml
        conformance_cli "[self_test][interactive]" -G d3d12 --reporter ctsxml::out=interactive_self_test_d3d12.xml
        conformance_cli "[self_test][interactive]" -G vulkan --reporter ctsxml::out=interactive_self_test_vulkan.xml
        conformance_cli "[self_test][interactive]" -G vulkan2 --reporter ctsxml::out=interactive_self_test_vulkan2.xml
        conformance_cli "[self_test][interactive]" -G opengl --reporter ctsxml::out=interactive_self_test_opengl.xml

Conformance Submission Package Requirements
-------------------------------------------

The submission package must include:

1. The bundle of output XML files as generated in the above Running CTS section
2. The console output produced by the CTS runs above.
3. Information on the build of conformance used in generating the results.
4. Conformance statement.

Details:

1. The bundle of output XML files:

   One or more automated test result XML files, 1 per graphics API supported,
   therefore one or more of the following generated output files:

        automated_d3d11.xml
        automated_d3d12.xml
        automated_opengl.xml
        automated_gles.xml
        automated_vulkan.xml
        automated_vulkan2.xml

   The output XML file(s) from running the interactive tests, 1 per supported
   graphics API, therefore one or more of the following generated output files:

        interactive_composition_d3d11.xml
        interactive_composition_d3d12.xml
        interactive_composition_opengl.xml
        interactive_composition_gles.xml
        interactive_composition_vulkan.xml
        interactive_composition_vulkan2.xml

   At least one output file from running the interactive scenario tests on a
   single graphics API (more is better):

        interactive_scenarios.xml

   The output XML file(s) from running the interactive action tests, 1 per
   supported interaction profile, therefore one or more of the following
   generated output files. This list below are example files, each platform may
   have their own controllers though simple_controller is expected to be
   supported at a minimum.

        interactive_action_simple_controller.xml
        interactive_action_microsoft_xbox_controller.xml
        interactive_action_microsoft_motion_controller.xml
        interactive_action_oculus_touch_controller.xml
        interactive_action_valve_index_controller.xml
        interactive_action_htc_vive_controller.xml

2. The console output produced by the CTS runs above.

   Each test suite run starts by printing test configuration data, and ends by
   printing a "Report" showing details of the runtime and environment
   (extensions, etc) used in that run. A few tests produce console output
   in-between that does not show up in the result XML. It is important to have
   this data for the interpretation of the results.

3. Information on the build of conformance used in generating the results:

   Files containing the result of the commands, `git status` and `git log` from
   the CTS directory:

        git_status.txt
        git_log.txt

   If there were changes required to pass the conformance test suite, a diff of
   the changes from a tagged release build of the suite should be included as well:

        git_diff.txt

   Note that **only the tagged releases on the OpenXR-CTS repo are accepted**
   without a diff: the latest such release will always be on the "approved"
   branch. The default "devel" branch is useful during development, but has not
   yet been voted on by the working group and is thus ineligible for submissions
   without a full diff. If the devel branch works better for you, you may
   consider encouraging the working group to tag a new release of conformance.

4. Conformance Statement

   A file containing information regarding the submission called
   `statement-<adopter>.txt`

        CONFORM_VERSION:         <git tag of CTS release>
        PRODUCT:                 <string-value>
        CPU:                     <string-value>
        OS:                      <string-value>

        WARNING_EXPLANATIONS:    <optional> <paragraph describing why the warnings present in the conformance logs are not indications of conformance failure>
        WAIVERS:                 <optional> <paragraph describing waiver requests for non-conformant test results>

   The actual submission package consists of the above set of files which must
   be bundled into a gzipped tar file named
   `XR<API major><API minor>_<adopter><_info>.tgz`. `<API major>` is the major
   version of the OpenXR API specification, `<API minor>` is the minor version
   of the OpenXR API specification. `<adopter>` is the name of the Adopting
   member company, or some recognizable abbreviation. The `<_info>` field is
   optional. It may be used to uniquely identify a submission by OS, platform,
   date, or other criteria when making multiple submissions. For example, a
   company XYZ may make a submission for an OpenXR 1.0 implementation named
   `XR10_XYZ_PRODUCTA_Windows10.tgz`.

Waivers
-------

Any test failures due to presumed bugs in the conformance tests not matching
specification behavior should be submitted as issues with potential fixes
against the conformance suite. Waivers are requested for test failures where the
underlying platform fails to meet the expected specification behavior. These are
requested in the statement file as described above. Enough detail should be
provided such that submission reviewers can judge the potential impact and risk
to the ecosystem of approving the submission.

Conformance Criteria
--------------------

A conformance run is considered passing if all tests finish with allowed result
codes, and all warnings are acceptably explained to describe why they are not a
conformance failure. Test results are contained in the output XML files, which
are an extension of the common "*Unit" schema with some custom elements. Each
test case leaf section is reached by a run of its own, and is recorded with a
`testcase` tag, e.g.:

    <testcase classname="global" name="Swapchains/Swapchain creation test parameters" time="1.207" status="run">

If all assertions in that case passed, there are no child elements to the
`testcase` tag. However, `testcase` tags may contain a warning, failure, or
error:

    <cts:warning type="WARN">

or

    <failure message="" type="">

or

    <error message="" type="">

With the results of the entire run summarized in `testsuite` tag (listing the
number of assertions):

    <testsuite errors="0" failures="0" tests="<number of successful assertions>" time="1.407">

as well as in the `cts:results` tag in `cts:ctsConformanceReport` (listing the
number of top level test cases):

    <cts:results testSuccessCount="1" testFailureCount="0"/>

**Any error or failure when testing core or KHR extension functionality means your runtime is not conformant.**

Any warnings **may** indicate non-conformance and should be explained in the
submission package.

Test Tags
---------

Tests should be categorized using tags. The following common tags are used:

* `[actions]`: indicates that this test is an "Action" test.
* `[composition]`: indicates that this test requires the tester to do a visual comparison.
* `[interactive]`: indicates that this test requires user input.
* `[no_auto]`: indicates that this interactive test cannot be automated by the `XR_EXT_conformance_automation` extension.
* `[scenario]`: indicates that the tester must perform a certain set of actions to pass the test.

If a test requires an extension the extension name should be listed as a tag.
See for example the `test_XR_KHR_visibility_mask.cpp`
