#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""Combine parsed CTS runs into review-oriented summary data."""

import os
from pathlib import Path
from typing import assert_never

from .model import (ExtensionSummary, MetadataInconsistency,
                    MissingGraphicsCoverageIssue, MissingRunStepIssue, Outcome,
                    OutcomeKind, ParsedRun, RunStep,
                    ScalarMetadataInconsistency, SetMetadataInconsistency,
                    SubmissionIssue, Summary, TestCaseResult, WarningGroup)

# Map graphics binding extension to the graphics plugin name defined in
# graphics_plugin_factory.cpp. The factory uses IgnoreCaseStringLess,
# so use .lower() when comparing / keying on plugin name.
GRAPHICS_BINDING_EXTENSIONS = {
    "XR_KHR_vulkan_enable": "Vulkan",
    "XR_KHR_vulkan_enable2": "Vulkan2",
    "XR_KHR_opengl_enable": "OpenGL",
    "XR_KHR_opengl_es_enable": "OpenGLES",
    "XR_KHR_D3D11_enable": "D3D11",
    "XR_KHR_D3D12_enable": "D3D12",
    "XR_KHR_metal_enable": "Metal",
}
REQUIRED_RUN_STEPS = (
    RunStep.AUTOMATED,
    RunStep.COMPOSITION,
    RunStep.SCENARIO,
    RunStep.ACTIONS,
)
GRAPHICS_COVERAGE_RUN_STEPS = (
    RunStep.AUTOMATED,
    RunStep.COMPOSITION,
)


def summarize(runs: list[ParsedRun]) -> Summary:
    failures: list[Outcome] = []
    errors: list[Outcome] = []
    notable_skips: list[Outcome] = []
    likely_trivial_skips: list[Outcome] = []
    warnings: dict[str, WarningGroup] = {}

    for run in runs:
        features = _run_features(run)
        for testcase in run.testcases:
            trivially_skippable = _is_trivially_skippable(testcase, features)
            for outcome in testcase.outcomes:
                match outcome.kind:
                    case OutcomeKind.FAILURE:
                        failures.append(outcome)
                    case OutcomeKind.ERROR:
                        errors.append(outcome)
                    case OutcomeKind.SKIPPED:
                        (likely_trivial_skips if trivially_skippable else notable_skips).append(outcome)
                    case OutcomeKind.WARNING:
                        key = outcome.source_location.normalized
                        group = warnings.setdefault(key, WarningGroup(normalized=key))
                        group.add(outcome)
                    case _:
                        assert_never(outcome.kind)

    warning_groups = sorted(warnings.values(), key=lambda group: (-group.count, group.normalized))
    return Summary(
        runs=runs,
        input_labels=_make_input_labels([run.path for run in runs]),
        failures=failures,
        errors=errors,
        notable_skips=notable_skips,
        likely_trivial_skips=likely_trivial_skips,
        warning_groups=warning_groups,
        metadata_inconsistencies=_find_metadata_inconsistencies(runs),
        extension_summaries=_summarize_extensions(runs),
        submission_issues=_find_submission_issues(runs),
        tag_data_unavailable=any(not run.has_tag_data for run in runs),
    )


def _find_metadata_inconsistencies(runs: list[ParsedRun]) -> list[MetadataInconsistency]:
    """Per-field inconsistency check across runs in one submission.

    The fields checked shouldn't generally differ within a CTS submission.
    Surface any divergences to let the reviewer judge whether they matter.
    """
    if len(runs) < 2:
        return []

    scalar_fields = [
        ("CTS revision", lambda run: run.revision),
        ("Stable test options", lambda run: run.test_options),
    ]
    # Set-valued fields render as shared baseline + per-XML additions
    set_fields = [
        ("Runtime instance properties", lambda run: run.runtime_instance_properties),
        ("Available API layers", lambda run: frozenset(run.available_api_layers)),
        (
            "Available instance extensions",
            lambda run: frozenset(run.available_instance_extensions),
        ),
        ("Available form factors", lambda run: frozenset(run.available_form_factors)),
    ]

    inconsistencies: list[MetadataInconsistency] = []
    for field_name, extractor in scalar_fields:
        by_value: dict[str, list[Path]] = {}
        for run in runs:
            value = extractor(run)
            by_value.setdefault(value, []).append(run.path)
        if len(by_value) > 1:
            inconsistencies.append(ScalarMetadataInconsistency(field_name, by_value))
    for field_name, extractor in set_fields:
        by_set = {run.path: extractor(run) for run in runs}
        if len(set(by_set.values())) > 1:
            inconsistencies.append(SetMetadataInconsistency(field_name, by_set))
    return inconsistencies


def _summarize_extensions(runs: list[ParsedRun]) -> list[ExtensionSummary]:
    advertised_extensions = {
        extension
        for run in runs
        for extension in run.available_instance_extensions
    }
    tagged_extensions = {
        tag
        for run in runs
        for testcase in run.testcases
        if testcase.tags is not None
        for tag in testcase.tags.extension_tags
    }

    by_extension = {
        extension: ExtensionSummary(extension, extension in advertised_extensions)
        for extension in sorted(advertised_extensions | tagged_extensions)
    }
    for run in runs:
        for testcase in run.testcases:
            if testcase.tags is None:
                continue
            for extension in testcase.tags.tags:
                if extension not in by_extension:
                    continue
                summary = by_extension[extension]
                summary.record_tagged_testcase(testcase)
    return list(by_extension.values())


def _run_features(run: ParsedRun) -> frozenset[str]:
    """The XR_* feature tags the runtime advertises for this run."""
    features = set(run.available_instance_extensions)
    for props in run.runtime_instance_properties:
        features.add(f"XR_VERSION_{props.api_version.major}_{props.api_version.minor}")
    return frozenset(features)


def _is_trivially_skippable(testcase: TestCaseResult, features: frozenset[str]) -> bool:
    """True when any XR_* feature tag on `testcase` is absent from `features`.
    """
    if testcase.tags is None:
        return False
    return any(tag not in features for tag in testcase.tags.feature_tags)


def _find_submission_issues(runs: list[ParsedRun]) -> list[SubmissionIssue]:
    advertised_extensions = {
        extension
        for run in runs
        for extension in run.available_instance_extensions
    }
    steps_by_plugin: dict[str, set[RunStep]] = {}
    for run in runs:
        if run.graphics_plugin:
            steps_by_plugin.setdefault(run.graphics_plugin.lower(), set()).add(run.run_step)
    run_steps = {run.run_step for run in runs}

    issues: list[SubmissionIssue] = []
    for extension, plugin in GRAPHICS_BINDING_EXTENSIONS.items():
        if extension not in advertised_extensions:
            continue
        plugin_steps = steps_by_plugin.get(plugin.lower(), set())
        issues.extend(
            MissingGraphicsCoverageIssue(
                extension=extension,
                graphics_plugin=plugin,
                run_step=step,
            )
            for step in GRAPHICS_COVERAGE_RUN_STEPS
            if step in run_steps and step not in plugin_steps
        )

    issues.extend(
        MissingRunStepIssue(step)
        for step in REQUIRED_RUN_STEPS
        if step not in run_steps
    )
    return issues


def _make_input_labels(paths: list[Path]) -> dict[Path, str]:
    """Return each path relative to the nearest common ancestor directory."""
    if not paths:
        return {}
    try:
        common_parent = Path(os.path.commonpath(path.parent for path in paths))
    except ValueError:
        return {path: path.as_posix() for path in paths}
    return {path: path.relative_to(common_parent).as_posix() for path in paths}
