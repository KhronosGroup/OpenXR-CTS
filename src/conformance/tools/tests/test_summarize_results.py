#!/usr/bin/env python3
#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

import difflib
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path
from typing import Any

TESTS_DIR = Path(__file__).resolve().parent
TOOLS_DIR = TESTS_DIR.parents[0]
sys.path.insert(0, str(TOOLS_DIR))

from cts_results import aggregate, model, parse, render  # noqa: E402

GOLDEN_DIR = TESTS_DIR / "golden"


def xml_text(
    *,
    filters="exclude:[interactive]",
    graphics_plugin="Vulkan",
    runtime_name="Synthetic Runtime",
    runtime_version="1.2.3",
    api_version="1.0.0",
    revision="openxr-cts-test",
    available_extensions=None,
    additional_runtime_instance_properties=None,
    include_runtime_instance_properties=True,
    test_options_extra="",
    testcase_body="",
    tags=None,
):
    available_extensions = available_extensions or []
    extension_xml = "\n".join(
        f'        <cts:extensionProperties extensionName="{name}" extensionVersion="1"/>'
        for name in available_extensions
    )
    runtime_major, runtime_minor, runtime_patch = runtime_version.split(".")
    api_major, api_minor, api_patch = api_version.split(".")
    extra_rip_blocks = "\n".join(
        _runtime_instance_properties_block(extra_api, extra_rv, extra_name)
        for extra_api, extra_rv, extra_name in (additional_runtime_instance_properties or [])
    )
    runtime_instance_properties = (
        textwrap.dedent(
            f"""\
              <cts:runtimeInstanceProperties>
                <cts:apiVersion major="{api_major}" minor="{api_minor}" patch="{api_patch}"/>
                <cts:runtimeVersion major="{runtime_major}" minor="{runtime_minor}" patch="{runtime_patch}"/>
                <cts:runtimeName>{runtime_name}</cts:runtimeName>
              </cts:runtimeInstanceProperties>"""
        )
        if include_runtime_instance_properties
        else ""
    )
    tag_attr = f' cts:tags="{tags}"' if tags is not None else ""
    return textwrap.dedent(
        f"""\
        <?xml version="1.0" encoding="UTF-8"?>
        <testsuites xmlns:cts="https://github.com/KhronosGroup/OpenXR-CTS">
          <testsuite errors="0" failures="0" skipped="0" tests="1" hostname="tbd" time="1.000" timestamp="2026-01-01T00:00:00Z">
            <properties>
              <property name="filters" value="{filters}"/>
            </properties>
            <cts:ctsTestEnvironment>
              <cts:revisionData revision="{revision}" localChanges="false"/>
        {runtime_instance_properties}
        {extra_rip_blocks}
              <cts:testOptions>
                <cts:graphicsPlugin value="{graphics_plugin}"/>
                <cts:minApiVersion string="1.0" value="0x0001000000000000"/>
                <cts:formFactor string="Hmd" value="XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY"/>
        {test_options_extra}
              </cts:testOptions>
              <cts:availableApiLayers>
                <cts:apiLayerProperties layerName="XR_APILAYER_KHRONOS_runtime_conformance" layerVersion="1" specVersionMajor="1" specVersionMinor="0" specVersionPatch="0"/>
              </cts:availableApiLayers>
              <cts:availableInstanceExtensions>
        {extension_xml}
              </cts:availableInstanceExtensions>
            </cts:ctsTestEnvironment>
            <cts:ctsConformanceReport>
              <cts:results testSuccessCount="1" testFailureCount="0"/>
            </cts:ctsConformanceReport>
            <testcase classname="global" name="SyntheticCase" time="0.000" status="run"{tag_attr}>
        {testcase_body}
            </testcase>
          </testsuite>
        </testsuites>
        """
    ).lstrip()


def _runtime_instance_properties_block(api_version: str, runtime_version: str, runtime_name: str) -> str:
    api_major, api_minor, api_patch = api_version.split(".")
    rv_major, rv_minor, rv_patch = runtime_version.split(".")
    return textwrap.dedent(
        f"""\
              <cts:runtimeInstanceProperties>
                <cts:apiVersion major="{api_major}" minor="{api_minor}" patch="{api_patch}"/>
                <cts:runtimeVersion major="{rv_major}" minor="{rv_minor}" patch="{rv_patch}"/>
                <cts:runtimeName>{runtime_name}</cts:runtimeName>
              </cts:runtimeInstanceProperties>"""
    )


def _version(version: str):
    major, minor, patch = version.split(".")
    return model.Version(int(major), int(minor), int(patch))


def runtime_props(api_version="1.0.0", runtime_version="1.2.3", runtime_name="Synthetic Runtime"):
    return model.RuntimeInstanceProperties(
        _version(api_version),
        _version(runtime_version),
        runtime_name,
    )


def parsed_run(path="x.xml", **overrides) -> model.ParsedRun:
    defaults: dict[str, Any] = dict(
        path=Path(path),
        filters=model.FilterExpression(""),
        run_step=model.RunStep.AUTOMATED,
        graphics_plugin="",
        revision="",
        runtime_instance_properties=frozenset(),
        test_options="",
        available_api_layers=(),
        available_instance_extensions=frozenset(),
        available_form_factors=(),
        counts=model.RunCounts(),
        testcases=[],
        has_tag_data=False,
    )
    defaults.update(overrides)
    return model.ParsedRun(**defaults)


def write_xml(directory: Path, name: str, text: str) -> Path:
    path = directory / name
    path.write_text(text, encoding="utf-8")
    return path


def summarize_inputs(inputs):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for name, text in inputs.items():
            write_xml(root, name, text)

        summary = aggregate.summarize(parse.collect_runs([root]))
        rendered = render.render_markdown(summary)

    return summary, rendered


def _outcome_xml(element: str, outcome_type: str, header: str, message: str, location: str) -> str:
    r"""Build a testcase result block.

    Example:
      <cts:warning type="WARN">
    WARNING:
    heads up
    at C:\openxr-cts\src\conformance\conformance_test\test_File.cpp(20)
      </cts:warning>
    """
    return textwrap.dedent(
        f"""\
              <{element} type="{outcome_type}">
    {header}
    {message}
    at {location}
              </{element}>
        """
    )


def failure_xml(message: str, location: str) -> str:
    return _outcome_xml("failure", "CHECK", "FAILED:", message, location)


def warning_xml(message: str, location: str) -> str:
    return _outcome_xml("cts:warning", "WARN", "WARNING:", message, location)


def skipped_xml(message: str, location: str) -> str:
    return _outcome_xml("skipped", "SKIP", "SKIPPED", message, location)


def testcase_with_outcome(
    kind: model.OutcomeKind,
    message: str,
    *,
    tags: str,
    name="SyntheticCase",
    path="x.xml",
    location="/tmp/openxr-cts/src/conformance/conformance_test/test_File.cpp:10",
) -> model.TestCaseResult:
    testcase_tags = parse.parse_testcase_tags(tags)
    body = "\n".join(
        [
            {
                model.OutcomeKind.FAILURE: "FAILED:",
                model.OutcomeKind.ERROR: "FAILED:",
                model.OutcomeKind.SKIPPED: "SKIPPED",
                model.OutcomeKind.WARNING: "WARNING:",
            }[kind],
            message,
            f"at {location}",
        ]
    )
    return model.TestCaseResult(
        classname="global",
        name=name,
        tags=testcase_tags,
        outcomes=[
            model.Outcome(
                kind=kind,
                run_path=Path(path),
                testcase_name=name,
                testcase_classname="global",
                testcase_tags=testcase_tags,
                body=body,
                source_location=parse._parse_source_location(body),
            )
        ],
    )


def skipped_case(
    message: str,
    tags: str,
    location="/tmp/openxr-cts/src/conformance/conformance_test/test_File.cpp:10",
) -> model.TestCaseResult:
    return testcase_with_outcome(
        model.OutcomeKind.SKIPPED,
        message,
        tags=tags,
        location=location,
    )


def _check_golden(testcase: unittest.TestCase, actual: str, expected_path: Path, label: str) -> None:
    expected = expected_path.read_text(encoding="utf-8")
    if actual != expected:
        diff = "".join(
            difflib.unified_diff(
                expected.splitlines(keepends=True),
                actual.splitlines(keepends=True),
                fromfile=str(expected_path),
                tofile=f"actual ({label})",
            )
        )
        testcase.fail(f"Golden mismatch for {label}:\n{diff}")


class GoldenTests(unittest.TestCase):
    """End-to-end snapshot tests: feed a committed inputs/ directory through
    the full pipeline and diff the rendered output against expected.*.
    Complements the unit-level FocusedTests.
    """

    def test_markdown(self):
        inputs_dir = GOLDEN_DIR / "inputs"
        summary = aggregate.summarize(parse.collect_runs([inputs_dir]))
        actual = render.render_markdown(summary)
        _check_golden(self, actual, GOLDEN_DIR / "expected.md", "markdown")

    def test_markdown_checkboxes(self):
        inputs_dir = GOLDEN_DIR / "inputs"
        summary = aggregate.summarize(parse.collect_runs([inputs_dir]))
        actual = render.render_markdown(summary, checkboxes=True)
        _check_golden(self, actual, GOLDEN_DIR / "expected.checkboxes.md", "checkboxes markdown")


class FocusedTests(unittest.TestCase):
    def test_collects_outcomes_and_renders_review_counts(self):
        body = "".join(
            [
                failure_xml(
                    "boom",
                    "/tmp/openxr-cts/src/conformance/conformance_test/test_File.cpp:10",
                ),
                warning_xml(
                    "heads up",
                    r"C:\openxr-cts\src\conformance\conformance_test\test_File.cpp(20)",
                ),
                skipped_xml(
                    "not today",
                    "/tmp/openxr-cts/src/conformance/conformance_test/test_File.cpp:30",
                ),
            ]
        )
        summary, rendered = summarize_inputs(
            {"one.xml": xml_text(testcase_body=body, tags="[XR_EXT_foo]")}
        )

        self.assertEqual(len(summary.failures), 1)
        self.assertEqual(summary.skip_count, 1)
        self.assertEqual(sum(group.count for group in summary.warning_groups), 1)
        warning = summary.warning_groups[0]
        self.assertEqual(len(warning.source_locations), 1)
        warning_location = next(iter(warning.source_locations))
        self.assertEqual(
            warning_location.original,
            r"C:\openxr-cts\src\conformance\conformance_test\test_File.cpp(20)",
        )
        self.assertEqual(warning_location.short, r"test_File.cpp(20)")
        self.assertEqual(
            warning_location.normalized,
            "src/conformance/conformance_test/test_File.cpp:20",
        )
        self.assertIn("- Failures: 1", rendered)
        self.assertIn("`test_File.cpp(20)`", rendered)

    def test_collect_xml_files_raises_on_nonexistent_path(self):
        with self.assertRaises(FileNotFoundError) as cm:
            parse.collect_xml_files([Path("does-not-exist")])
        self.assertIn("does-not-exist", str(cm.exception))

    def test_collect_xml_files_raises_when_no_xml_found(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(FileNotFoundError) as cm:
                parse.collect_xml_files([Path(tmp)])
            self.assertIn(tmp, str(cm.exception))

    def test_metadata_checks_flag_runtime_and_instance_extension_differences(self):
        runs = [
            parsed_run(
                path="vulkan.xml",
                runtime_instance_properties=frozenset([runtime_props(runtime_version="1.2.3")]),
                available_instance_extensions=frozenset(["XR_EXT_same", "XR_KHR_vulkan_enable"]),
            ),
            parsed_run(
                path="vulkan2.xml",
                runtime_instance_properties=frozenset([runtime_props(runtime_version="1.2.4")]),
                available_instance_extensions=frozenset([
                    "XR_EXT_same",
                    "XR_KHR_vulkan_enable",
                    "XR_KHR_vulkan_enable2",
                ]),
            ),
        ]
        summary = aggregate.summarize(runs)

        fields = [item.field for item in summary.metadata_inconsistencies]
        self.assertIn("Runtime instance properties", fields)
        self.assertIn("Available instance extensions", fields)

    def test_parses_multiple_runtime_instance_properties(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = write_xml(
                root,
                "one.xml",
                xml_text(
                    api_version="1.0.0",
                    runtime_version="1.2.3",
                    additional_runtime_instance_properties=[
                        ("1.1.0", "1.2.4", "Synthetic Runtime")
                    ],
                ),
            )

            run = parse.parse_cts_xml(path)

        self.assertEqual(
            run.runtime_instance_properties,
            frozenset([
                runtime_props("1.0.0", "1.2.3", "Synthetic Runtime"),
                runtime_props("1.1.0", "1.2.4", "Synthetic Runtime"),
            ]),
        )

    def test_requires_runtime_instance_properties(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_xml(
                Path(tmp),
                "one.xml",
                xml_text(include_runtime_instance_properties=False),
            )

            with self.assertRaises(ValueError) as cm:
                parse.parse_cts_xml(path)
            self.assertIn(str(path), str(cm.exception))

    def test_empty_tags_still_count_as_tag_data(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_xml(Path(tmp), "one.xml", xml_text(tags=""))

            run = parse.parse_cts_xml(path)

        self.assertTrue(run.has_tag_data)
        self.assertEqual(run.testcases[0].tags, model.TestCaseTags())

    def test_warnings_combine_by_normalized_source_location(self):
        unix_warning = warning_xml(
            "first warning",
            "/tmp/openxr-cts/src/conformance/conformance_test/test_Warn.cpp:20",
        )
        windows_warning = warning_xml(
            "second warning",
            r"C:\openxr-cts\src\conformance\conformance_test\test_Warn.cpp(20)",
        )
        summary, rendered = summarize_inputs(
            {
                "unix.xml": xml_text(testcase_body=unix_warning),
                "windows.xml": xml_text(testcase_body=windows_warning),
            }
        )

        self.assertEqual(len(summary.warning_groups), 1)
        warning = summary.warning_groups[0]
        self.assertEqual(warning.count, 2)
        self.assertEqual(warning.messages["first warning"], 1)
        self.assertEqual(warning.messages["second warning"], 1)
        self.assertIn("- first warning (1x)\n    - second warning (1x)", rendered)

    def test_high_volume_warnings_render_one_sample_per_location(self):
        body = "".join(
            warning_xml(
                f"Unsupported config found := {index}",
                "/tmp/openxr-cts/src/conformance/conformance_test/test_Swapchains.cpp:316",
            )
            for index in range(7)
        )
        summary, rendered = summarize_inputs({"one.xml": xml_text(testcase_body=body)})

        self.assertEqual(len(summary.warning_groups), 1)
        self.assertEqual(summary.warning_groups[0].count, 7)
        self.assertIn("7 distinct messages at this location; showing one sample", rendered)
        self.assertEqual(rendered.count("Unsupported config found"), 1)

    def test_warning_detail_limit_zero_disables_collapse(self):
        with tempfile.TemporaryDirectory() as tmp:
            body = "".join(
                warning_xml(
                    f"Unsupported config found := {index}",
                    "/tmp/openxr-cts/src/conformance/conformance_test/test_Swapchains.cpp:316",
                )
                for index in range(7)
            )
            write_xml(Path(tmp), "one.xml", xml_text(testcase_body=body))
            summary = aggregate.summarize(parse.collect_runs([Path(tmp)]))
            rendered = render.render_markdown(summary, warning_detail_limit=0)

        self.assertNotIn("distinct messages at this location; showing one sample", rendered)
        self.assertEqual(rendered.count("Unsupported config found"), 7)

    def test_advertised_extension_with_skipped_tagged_test_is_called_out(self):
        testcase = skipped_case(
            "extension test skipped",
            tags="[XR_EXT_eye_gaze_interaction]",
            location="/tmp/openxr-cts/src/conformance/conformance_test/test_Ext.cpp:42",
        )
        summary = aggregate.summarize([parsed_run(
            available_instance_extensions=frozenset(["XR_EXT_eye_gaze_interaction"]),
            testcases=[testcase],
            has_tag_data=True,
        )])
        rendered = render.render_markdown(summary)

        eye_gaze = next(
            item for item in summary.extension_summaries if item.name == "XR_EXT_eye_gaze_interaction"
        )
        self.assertEqual(eye_gaze.readable_status(), "advertised, all tagged tests skipped")
        # The skip is notable because the runtime advertises the extension --
        # without a tag/feature mismatch we can't classify it as trivial.
        self.assertEqual(len(summary.notable_skips), 1)
        self.assertEqual(len(summary.likely_trivial_skips), 0)
        self.assertIn("1 notable skip(s)", rendered)

    def test_skip_classification_by_feature_tags(self):
        cases = [
            (
                "unmet extension",
                "[XR_EXT_eye_gaze_interaction]",
                dict(available_instance_extensions=frozenset(["XR_EXT_palm_pose"])),
                (0, 1),
            ),
            (
                "unmet api version",
                "[XR_VERSION_1_1]",
                dict(runtime_instance_properties=frozenset([runtime_props(api_version="1.0.0")])),
                (0, 1),
            ),
            ("no feature tags", "[self_test]", {}, (1, 0)),
        ]
        for name, tags, run_overrides, expected_counts in cases:
            with self.subTest(name):
                testcase = skipped_case("skip", tags=tags)
                summary = aggregate.summarize([
                    parsed_run(testcases=[testcase], has_tag_data=True, **run_overrides)
                ])
                self.assertEqual(
                    (len(summary.notable_skips), len(summary.likely_trivial_skips)),
                    expected_counts,
                )

    def test_skip_groups_sort_by_feature_class(self):
        cases = [
            ("a vendor skip", "[XR_META_headset_id]", "test_Vendor.cpp:40"),
            ("b ext skip", "[XR_EXT_eye_gaze_interaction]", "test_Ext.cpp:40"),
            ("aa mixed ext skip", "[XR_VERSION_1_1][XR_EXT_eye_gaze_interaction]", "test_Ext.cpp:50"),
            ("c khr skip", "[XR_KHR_binding_modification]", "test_Khr.cpp:40"),
            ("y version skip", "[XR_VERSION_1_1]", "test_Version.cpp:40"),
            ("z core skip", "[scenario]", "test_Core.cpp:40"),
        ]
        summary = aggregate.summarize([parsed_run(
            runtime_instance_properties=frozenset([runtime_props(api_version="1.1.0")]),
            available_instance_extensions=frozenset([
                "XR_KHR_binding_modification",
                "XR_EXT_eye_gaze_interaction",
                "XR_META_headset_id",
            ]),
            testcases=[
                skipped_case(
                    message,
                    tags=tags,
                    location=f"/tmp/openxr-cts/src/conformance/conformance_test/{location}",
                )
                for message, tags, location in cases
            ],
            has_tag_data=True,
        )])
        rendered = render.render_markdown(summary)

        expected_order = [
            "z core skip",
            "y version skip",
            "c khr skip",
            "aa mixed ext skip",
            "b ext skip",
            "a vendor skip",
        ]
        for before, after in zip(expected_order, expected_order[1:]):
            self.assertLess(rendered.index(before), rendered.index(after))

    def test_submission_completeness_requires_graphics_runs_per_advertised_binding(self):
        available_extensions = frozenset(["XR_KHR_vulkan_enable", "XR_KHR_D3D11_enable"])
        runs = [
            parsed_run(
                path="automated.xml",
                run_step=model.RunStep.AUTOMATED,
                graphics_plugin="Vulkan",
                available_instance_extensions=available_extensions,
            ),
            parsed_run(
                path="composition.xml",
                run_step=model.RunStep.COMPOSITION,
                graphics_plugin="Vulkan",
                available_instance_extensions=available_extensions,
            ),
        ]
        summary = aggregate.summarize(runs)
        rendered = render.render_markdown(summary)

        graphics_issues = {
            (issue.extension, issue.run_step)
            for issue in summary.submission_issues
            if isinstance(issue, model.MissingGraphicsCoverageIssue)
        }
        self.assertNotIn(
            ("XR_KHR_vulkan_enable", model.RunStep.AUTOMATED),
            graphics_issues,
        )
        self.assertIn(
            ("XR_KHR_D3D11_enable", model.RunStep.AUTOMATED),
            graphics_issues,
        )
        self.assertIn(
            ("XR_KHR_D3D11_enable", model.RunStep.COMPOSITION),
            graphics_issues,
        )
        self.assertIn("`XR_KHR_D3D11_enable` is advertised, but no automated run used graphicsPlugin `D3D11`.", rendered)
        self.assertIn(
            "`XR_KHR_D3D11_enable` is advertised, but no `[composition][interactive]` run used graphicsPlugin `D3D11`.",
            rendered,
        )

    def test_submission_completeness_reports_missing_run_step_once(self):
        cases = [
            (model.RunStep.AUTOMATED, model.RunStep.COMPOSITION),
            (model.RunStep.COMPOSITION, model.RunStep.AUTOMATED),
        ]
        for missing_step, present_step in cases:
            with self.subTest(missing_step):
                summary = aggregate.summarize([
                    parsed_run(
                        run_step=present_step,
                        graphics_plugin="Vulkan",
                        available_instance_extensions=frozenset(["XR_KHR_vulkan_enable"]),
                    )
                ])
                rendered = render.render_markdown(summary)

                graphics_issues = [
                    issue for issue in summary.submission_issues
                    if isinstance(issue, model.MissingGraphicsCoverageIssue)
                ]
                missing_steps = [
                    issue.run_step for issue in summary.submission_issues
                    if isinstance(issue, model.MissingRunStepIssue)
                ]
                self.assertNotIn(missing_step, [issue.run_step for issue in graphics_issues])
                self.assertIn(missing_step, missing_steps)
                self.assertNotIn(
                    render._submission_issue_message(model.MissingRunStepIssue(present_step)),
                    rendered,
                )
                self.assertIn(
                    render._submission_issue_message(model.MissingRunStepIssue(missing_step)),
                    rendered,
                )
                self.assertNotIn(
                    render._submission_issue_message(model.MissingGraphicsCoverageIssue(
                        extension="XR_KHR_vulkan_enable",
                        graphics_plugin="Vulkan",
                        run_step=missing_step,
                    )),
                    rendered,
                )

    def test_run_features_includes_a_tag_per_advertised_apiversion(self):
        run = parsed_run(runtime_instance_properties=frozenset([
            runtime_props("1.0.0", "1.2.3", "Synthetic Runtime"),
            runtime_props("1.1.0", "1.2.3", "Synthetic Runtime"),
        ]))
        features = aggregate._run_features(run)
        self.assertIn("XR_VERSION_1_0", features)
        self.assertIn("XR_VERSION_1_1", features)

    def test_metadata_check_flags_form_factor_differences(self):
        with_hmd = model.AvailableFormFactor(
            type="XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY",
            view_configurations=(
                model.AvailableViewConfiguration(
                    type="XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO",
                    environment_blend_modes=("XR_ENVIRONMENT_BLEND_MODE_OPAQUE",),
                ),
            ),
        )
        runs = [
            parsed_run(path="a.xml", available_form_factors=(with_hmd,)),
            parsed_run(path="b.xml", available_form_factors=()),
        ]
        summary = aggregate.summarize(runs)
        fields = [item.field for item in summary.metadata_inconsistencies]
        self.assertIn("Available form factors", fields)

    def test_runtime_instance_properties_inconsistency_catches_criss_cross(self):
        # Same names and versions across runs, but the (api, rv) pairing differs.
        # Set-equality on the (api, rv, name) triples catches this; per-dim
        # projection would not.
        runs = [
            parsed_run(path="automated.xml", runtime_instance_properties=frozenset([
                runtime_props("1.0.0", "1.2.3", "Synthetic Runtime"),
                runtime_props("1.1.0", "1.2.4", "Synthetic Runtime"),
            ])),
            parsed_run(path="composition.xml", runtime_instance_properties=frozenset([
                runtime_props("1.0.0", "1.2.4", "Synthetic Runtime"),
                runtime_props("1.1.0", "1.2.3", "Synthetic Runtime"),
            ])),
        ]
        summary = aggregate.summarize(runs)
        fields = [item.field for item in summary.metadata_inconsistencies]
        self.assertIn("Runtime instance properties", fields)

    def test_submission_completeness_requires_scenario_and_actions_runs(self):
        runs = [
            parsed_run(path="automated.xml", run_step=model.RunStep.AUTOMATED),
            parsed_run(path="composition.xml", run_step=model.RunStep.COMPOSITION),
        ]
        summary = aggregate.summarize(runs)
        rendered = render.render_markdown(summary)

        missing_steps = [
            issue.run_step
            for issue in summary.submission_issues
            if isinstance(issue, model.MissingRunStepIssue)
        ]
        self.assertIn(model.RunStep.SCENARIO, missing_steps)
        self.assertIn(model.RunStep.ACTIONS, missing_steps)
        self.assertIn("No `[scenario][interactive]` run found in this submission.", rendered)
        self.assertIn("No `[actions][interactive]` run found in this submission.", rendered)


if __name__ == "__main__":
    unittest.main()
