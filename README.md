OpenXR Conformance Test Suite
=============================

<!--
Copyright (c) 2019-2021, The Khronos Group Inc.

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

Running CTS
-----------

For each run, you will also need to preserve the console output (or equivalent)
as it is important for interpreting the results.

1. Run the automated tests (non-interactive tests) for every graphics API that is supported.

    Example:

        conformance_cli "exclude:[interactive]" -G d3d11 -s -r junit -o automated_d3d11.xml
        conformance_cli "exclude:[interactive]" -G d3d12 -s -r junit -o automated_d3d12.xml
        conformance_cli "exclude:[interactive]" -G vulkan -s -r junit -o automated_vulkan.xml
        conformance_cli "exclude:[interactive]" -G opengl -s -r junit -o automated_opengl.xml

    Notes:
    * Some tests require that a begun session progresses to `XR_SESSION_STATE_FOCUSED`.
    * Some tests require valid view tracking (`XR_VIEW_STATE_ORIENTATION_VALID_BIT & XR_VIEW_STATE_POSITION_VALID_BIT`).

2. Run the interactive composition tests for every graphics API that is supported.

    Example:

        conformance_cli "[composition][interactive]" -G d3d11 -s -r junit -o interactive_composition_d3d11.xml
        conformance_cli "[composition][interactive]" -G d3d12 -s -r junit -o interactive_composition_d3d12.xml
        conformance_cli "[composition][interactive]" -G vulkan -s -r junit -o interactive_composition_vulkan.xml
        conformance_cli "[composition][interactive]" -G opengl -s -r junit -o interactive_composition_opengl.xml

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

        conformance_cli "[scenario][interactive]" -G opengl -s -r junit -o interactive_scenarios.xml

    Notes:
    * The runtime must support `khr/simple_controller`. If it cannot be included
      for some reason, use of another interaction profile may be performed
      through the porting process

4. Run the interactive action tests for every interaction profile your runtime
   can bind completely to a device. Run the tests for at least one graphics API.

   Example:

        conformance_cli "[actions][interactive]" -G d3d11 -I "khr/simple_controller" -s -r junit -o interactive_action_simple_controller.xml
        conformance_cli "[actions][interactive]" -G d3d11 -I "microsoft/motion_controller" -s -r junit -o interactive_action_microsoft_motion_controller.xml
        conformance_cli "[actions][interactive]" -G d3d11 -I "oculus/touch_controller" -s -r junit -o interactive_action_oculus_touch_controller.xml
        conformance_cli "[actions][interactive]" -G d3d11 -I "htc/vive_controller" -s -r junit -o interactive_action_htc_vive_controller.xml

   Note that the `microsoft/xbox_controller` interaction profile only needs to
   run against the gamepad tests:

        conformance_cli "[gamepad]" -G d3d11 -I "microsoft/xbox_controller" -s -r junit -o interactive_action_microsoft_xbox_controller.xml

   Notes:
   * A person must use the OpenXR action system input by following the displayed
     instructions.
   * The interaction profile paths specified with `-I` must have the
     "/interaction_profile/" prefix stripped to avoid a parsing bug in Catch2.

5. Bundle up the conformance test suite output XML files, the console output,
   and the other requirements of the OpenXR section of the Conformance Process
   document <https://www.khronos.org/conformance/adopters>, and submit to
   Khronos for review and approval by the OpenXR Working Group.

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

   The output XML file(s) from running the interactive tests, 1 per supported
   graphics API, therefore one or more of the following generated output files:

        interactive_composition_d3d11.xml
        interactive_composition_d3d12.xml
        interactive_composition_opengl.xml
        interactive_composition_gles.xml
        interactive_composition_vulkan.xml

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

   Note that only the tagged releases on the OpenXR-CTS repo are accepted
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
codes. Test results are contained in the output XML files. Each
test case section contains XML tag Result, for example:

    <OverallResult success="true"/>

With the results of the entire run summarized in the outermost scope of the XML file:

    <OverallResults successes="<number of successes>" failures="0" expectedFailures="0"/>
