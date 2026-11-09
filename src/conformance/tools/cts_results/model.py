#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""Shared types and helpers."""

import re
from collections import Counter
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional, Union

# Source locations are emitted as either `path:line` (Linux/macOS) or `path(line)`
# (Windows/MSVC). Capture path, delimiter, and line. This lets us normalize for
# duplicate analysis, but output the original for greppability.
SOURCE_LOCATION_RE = re.compile(r"\bat\s+(.+?)(:|\()(\d+)\)?\s*$", re.MULTILINE)
# Catch2 prefixes outcome bodies with one of these single-token marker lines; they
# are noise in summary items where the outcome kind is already clear from context.
OUTCOME_HEADERS = frozenset({"WARNING:", "FAILED:", "SKIPPED", "SKIPPED:"})


class _TextEnum(str, Enum):
    """Pre-3.11 enum with StrEnum-like display behavior."""

    def __str__(self) -> str:
        return self.value


class OutcomeKind(_TextEnum):
    FAILURE = "failure"
    ERROR = "error"
    SKIPPED = "skipped"
    WARNING = "warning"


class RunStep(_TextEnum):
    AUTOMATED = "automated"
    COMPOSITION = "composition"
    SCENARIO = "scenario"
    ACTIONS = "actions"
    GAMEPAD = "gamepad"
    UNKNOWN = "unknown"


@dataclass(frozen=True)
class FilterExpression:
    """Catch2 filter expression from the run-level filters property.

    XML: `/testsuites/testsuite/properties/property[@name='filters']/@value`.
    Keep the raw expression because run-step classification depends on
    include/exclude syntax.
    """

    raw: str
    included_tags: frozenset[str] = frozenset()
    excluded_tags: frozenset[str] = frozenset()


@dataclass(frozen=True)
class TestCaseTags:
    """XML: `cts:tags` on `/testsuites/testsuite/testcase`. Plain tag set;
    no include/exclude semantics (unlike FilterExpression).
    """

    tags: frozenset[str] = frozenset()

    @property
    def feature_tags(self) -> frozenset[str]:
        return frozenset(tag for tag in self.tags if _is_feature_tag(tag))

    @property
    def extension_tags(self) -> frozenset[str]:
        return frozenset(tag for tag in self.tags if _is_extension_tag(tag))


@dataclass(frozen=True)
class SourceLocation:
    """Source path parsed from the final `at path:line` line in an outcome body."""

    original: str = ""
    short: str = ""
    normalized: str = ""


@dataclass
class Outcome:
    """One failure/error/skip/warning child with its testcase/run context.

    XML: a `failure`, `error`, `skipped`, or `cts:warning` child of
    `/testsuites/testsuite/testcase`.
    """

    kind: OutcomeKind
    run_path: Path
    testcase_name: str
    testcase_classname: str
    testcase_tags: Optional[TestCaseTags]
    body: str
    source_location: SourceLocation

    @property
    def normalized_message(self) -> str:
        return _normalized_message(self.body)


@dataclass
class TestCaseResult:
    """XML: `/testsuites/testsuite/testcase` and its parsed outcome children."""

    classname: str
    name: str
    tags: Optional[TestCaseTags]
    outcomes: list[Outcome] = field(default_factory=list)

    @property
    def skipped(self) -> bool:
        return any(outcome.kind == OutcomeKind.SKIPPED for outcome in self.outcomes)

    @property
    def failed_or_errored(self) -> bool:
        return any(outcome.kind in {OutcomeKind.FAILURE, OutcomeKind.ERROR} for outcome in self.outcomes)


@dataclass(frozen=True)
class Version:
    """XML version element with `major`, `minor`, and `patch` attributes."""

    major: int
    minor: int
    patch: int

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}"


@dataclass(frozen=True)
class RuntimeInstanceProperties:
    """XML: `cts:runtimeInstanceProperties`."""

    api_version: Version
    runtime_version: Version
    runtime_name: str

    def __str__(self) -> str:
        return f"(api={self.api_version}, rv={self.runtime_version}, name={self.runtime_name!r})"


@dataclass(frozen=True)
class RunCounts:
    """Counts from `cts:results` plus the enclosing `testsuite` attributes."""

    success: int = 0
    failure: int = 0
    skipped: int = 0
    tests: int = 0
    errors: int = 0


@dataclass(frozen=True)
class AvailableViewConfiguration:
    """XML: `cts:availableViewConfigurations/cts:viewConfiguration`."""

    type: str  # full enum, e.g. "XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO"
    environment_blend_modes: tuple[str, ...]  # full enums, e.g. "XR_ENVIRONMENT_BLEND_MODE_OPAQUE"

    def __str__(self) -> str:
        modes = ", ".join(self.environment_blend_modes) or "(none)"
        return f"{self.type} [{modes}]"


@dataclass(frozen=True)
class AvailableFormFactor:
    """XML: `cts:availableFormFactors/cts:formFactor`."""

    type: str  # full enum, e.g. "XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY"
    view_configurations: tuple[AvailableViewConfiguration, ...]

    def __str__(self) -> str:
        if not self.view_configurations:
            return f"{self.type} (no view configurations)"
        body = "; ".join(str(vc) for vc in self.view_configurations)
        return f"{self.type} {{{body}}}"


@dataclass
class ParsedRun:
    """One parsed CTS XML file."""

    path: Path
    filters: FilterExpression
    run_step: RunStep
    graphics_plugin: str
    revision: str
    runtime_instance_properties: frozenset[RuntimeInstanceProperties]
    test_options: str
    available_api_layers: tuple[str, ...]
    available_instance_extensions: frozenset[str]
    available_form_factors: tuple[AvailableFormFactor, ...]
    counts: RunCounts
    testcases: list[TestCaseResult]
    has_tag_data: bool


@dataclass
class ScalarMetadataInconsistency:
    field: str
    by_value: dict[str, list[Path]]


@dataclass
class SetMetadataInconsistency:
    field: str
    by_set: dict[Path, frozenset]


MetadataInconsistency = Union[ScalarMetadataInconsistency, SetMetadataInconsistency]


@dataclass
class WarningGroup:
    # Canonical normalized form of the location for deduplication.
    normalized: str
    count: int = 0
    messages: Counter[str] = field(default_factory=Counter)
    per_input: Counter[Path] = field(default_factory=Counter)
    # Every variant SourceLocation contributing to the group, e.g.
    # Windows `path(line)` and POSIX `path:line` forms of the same location.
    source_locations: set[SourceLocation] = field(default_factory=set)

    def add(self, outcome: Outcome) -> None:
        self.count += 1
        self.messages[outcome.normalized_message] += 1
        self.per_input[outcome.run_path] += 1
        if outcome.source_location.short:
            self.source_locations.add(outcome.source_location)


@dataclass
class ExtensionSummary:
    name: str
    advertised: bool
    tagged_tests: int = 0
    skipped_tests: int = 0
    failed_tests: int = 0

    def record_tagged_testcase(self, testcase: TestCaseResult) -> None:
        self.tagged_tests += 1
        self.skipped_tests += testcase.skipped
        self.failed_tests += testcase.failed_or_errored

    def readable_status(self) -> str:
        if not self.advertised:
            return "not advertised, tagged tests present"
        if self.tagged_tests == 0:
            return "advertised, no tagged tests seen"
        if self.skipped_tests == self.tagged_tests:
            return "advertised, all tagged tests skipped"
        if self.skipped_tests:
            return "advertised, some tagged tests skipped"
        return "advertised, tagged tests ran"


@dataclass(frozen=True)
class MissingGraphicsCoverageIssue:
    extension: str
    graphics_plugin: str
    run_step: RunStep


@dataclass(frozen=True)
class MissingRunStepIssue:
    run_step: RunStep


SubmissionIssue = Union[MissingGraphicsCoverageIssue, MissingRunStepIssue]


@dataclass
class Summary:
    runs: list[ParsedRun]
    input_labels: dict[Path, str]
    failures: list[Outcome]
    errors: list[Outcome]
    # Skipped outcomes split by whether the testcase that produced them was
    # trivially skippable in its run (tagged with an XR_* feature the runtime
    # didn't advertise). Use `skip_count` for the total.
    notable_skips: list[Outcome]
    likely_trivial_skips: list[Outcome]
    warning_groups: list[WarningGroup]
    metadata_inconsistencies: list[MetadataInconsistency]
    extension_summaries: list[ExtensionSummary]
    submission_issues: list[SubmissionIssue]
    tag_data_unavailable: bool

    @property
    def skip_count(self) -> int:
        return len(self.notable_skips) + len(self.likely_trivial_skips)

    @property
    def warning_count(self) -> int:
        return sum(group.count for group in self.warning_groups)


def _message_without_location(body: str) -> str:
    return SOURCE_LOCATION_RE.sub("", body).strip()


def _normalized_message(body: str) -> str:
    body = _message_without_location(body)
    return "\n".join(line for line in body.splitlines() if line.strip() not in OUTCOME_HEADERS).strip()


def _is_feature_tag(tag: str) -> bool:
    """Is this tag an extension tag or version marker?"""
    return tag.startswith("XR_")


def _is_extension_tag(tag: str) -> bool:
    """Feature tag excluding version markers."""
    return _is_feature_tag(tag) and not tag.startswith("XR_VERSION_")


def short_source_path(path: str) -> str:
    """Strip well-known CTS tree prefixes from a forward-slash source path."""
    prefixes = (
        "src/conformance/conformance_test/../framework/",
        "src/conformance/conformance_test/",
        "src/conformance/framework/",
    )
    for prefix in prefixes:
        if path.startswith(prefix):
            return path[len(prefix):]
    return path
