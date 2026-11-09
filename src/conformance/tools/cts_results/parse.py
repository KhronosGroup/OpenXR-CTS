#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""Read OpenXR CTS XML result files into parsed run records."""

import re
import xml.etree.ElementTree as ET
from pathlib import Path, PureWindowsPath
from typing import Iterable, Optional

from .model import (SOURCE_LOCATION_RE, AvailableFormFactor,
                    AvailableViewConfiguration, FilterExpression, Outcome,
                    OutcomeKind, ParsedRun, RunCounts, RunStep,
                    RuntimeInstanceProperties, SourceLocation, TestCaseResult,
                    TestCaseTags, Version, short_source_path)

CTS_NS = "https://github.com/KhronosGroup/OpenXR-CTS"
NS = {"cts": CTS_NS}
TAG_RE = re.compile(r"(exclude:)?\[([^\]]+)\]")


def parse_filter_expression(raw: Optional[str]) -> FilterExpression:
    raw = raw or ""
    included: set[str] = set()
    excluded: set[str] = set()
    for match in TAG_RE.finditer(raw):
        target = excluded if match.group(1) else included
        target.add(match.group(2))
    return FilterExpression(raw, frozenset(included), frozenset(excluded))


def parse_testcase_tags(raw: str) -> TestCaseTags:
    return TestCaseTags(frozenset(match.group(2) for match in TAG_RE.finditer(raw)))


# Tag sets used to categorize a given CTS run
_SET_INTERACTIVE = frozenset({"interactive"})
_SET_COMPOSITION_INTERACTIVE = frozenset({"composition", "interactive"})
_SET_SCENARIO_INTERACTIVE = frozenset({"scenario", "interactive"})
_SET_ACTIONS_INTERACTIVE = frozenset({"actions", "interactive"})
_SET_GAMEPAD = frozenset({"gamepad"})


def classify_run_step(filters: FilterExpression) -> RunStep:
    """Classify the run step from the exact filter expression.

    Run steps that do not use a canonical filter (e.g.
    [composition][interactive][foo]) are classed as "unknown"
    and not counted for purposes of completeness analysis.
    """
    included = filters.included_tags
    excluded = filters.excluded_tags
    if not included and excluded == _SET_INTERACTIVE:
        return RunStep.AUTOMATED
    if not excluded and included == _SET_COMPOSITION_INTERACTIVE:
        return RunStep.COMPOSITION
    if not excluded and included == _SET_SCENARIO_INTERACTIVE:
        return RunStep.SCENARIO
    if not excluded and included == _SET_ACTIONS_INTERACTIVE:
        return RunStep.ACTIONS
    if not excluded and included == _SET_GAMEPAD:
        return RunStep.GAMEPAD
    return RunStep.UNKNOWN


def collect_xml_files(paths: Iterable[Path]) -> list[Path]:
    paths = list(paths)
    xml_files: list[Path] = []
    for path in paths:
        if not path.exists():
            raise FileNotFoundError(f"input path does not exist: {path}")
        if path.is_dir():
            xml_files.extend(path.rglob("*.xml"))
        elif path.is_file() and path.suffix.lower() == ".xml":
            xml_files.append(path)
    if not xml_files:
        searched = ", ".join(str(p) for p in paths)
        raise FileNotFoundError(f"no XML files found in: {searched}")
    return sorted(xml_files)


def collect_runs(paths: Iterable[Path]) -> list[ParsedRun]:
    return [parse_cts_xml(path) for path in collect_xml_files(paths)]


def parse_cts_xml(path: Path) -> ParsedRun:
    try:
        return _parse_cts_xml(path)
    except (ValueError, ET.ParseError) as exc:
        raise ValueError(f"{path}: {exc}") from exc


def _parse_cts_xml(path: Path) -> ParsedRun:
    root = ET.parse(path).getroot()
    if root.tag != "testsuites":
        raise ValueError(f"expected <testsuites> root, got <{root.tag}>")
    testsuite = _required_child(root, "./testsuite")

    filters = parse_filter_expression(
        _required_attr(
            _required_child(testsuite, "./properties/property[@name='filters']"),
            "value",
        )
    )
    test_options = _required_child(root, ".//cts:ctsTestEnvironment/cts:testOptions")

    testcases = _parse_testcases(root, path)
    available_extensions = frozenset(
        _required_attr(elem, "extensionName")
        for elem in root.findall(".//cts:availableInstanceExtensions/cts:extensionProperties", NS)
    )

    return ParsedRun(
        path=path,
        filters=filters,
        run_step=classify_run_step(filters),
        graphics_plugin=_required_attr(
            _required_child(test_options, "cts:graphicsPlugin"),
            "value",
        ),
        revision=_required_attr(_required_child(root, ".//cts:revisionData"), "revision"),
        runtime_instance_properties=_parse_runtime_instance_properties(root),
        test_options=_canonical_test_options(test_options),
        available_api_layers=tuple(
            sorted(
                _api_layer_value(elem)
                for elem in root.findall(".//cts:availableApiLayers/cts:apiLayerProperties", NS)
            )
        ),
        available_instance_extensions=available_extensions,
        available_form_factors=_parse_available_form_factors(root),
        counts=_parse_counts(root, testsuite),
        testcases=testcases,
        # TODO: Check the CTS revision before treating missing `cts:tags` as
        # unavailable tag data. For revisions new enough to emit the attribute,
        # no observed tags should mean "there are no tags" rather than "this
        # XML cannot report tags". Alternatively, update the CTS reporter to
        # always write `cts:tags=""` so this distinction is explicit in XML.
        has_tag_data=any(testcase.tags is not None for testcase in testcases),
    )


def _parse_testcases(root: ET.Element, path: Path) -> list[TestCaseResult]:
    testcases: list[TestCaseResult] = []
    for elem in root.findall(".//testcase"):
        tags = _get_cts_attribute(elem, "tags")
        testcase = TestCaseResult(
            classname=_required_attr(elem, "classname"),
            name=_required_attr(elem, "name"),
            tags=parse_testcase_tags(tags) if tags is not None else None,
        )
        for child in list(elem):
            kind = _outcome_kind_from_tag(child.tag)
            if kind is None:
                continue
            body = _all_text(child).strip()
            testcase.outcomes.append(
                Outcome(
                    kind=kind,
                    run_path=path,
                    testcase_name=testcase.name,
                    testcase_classname=testcase.classname,
                    testcase_tags=testcase.tags,
                    body=body,
                    source_location=_parse_source_location(body),
                )
            )
        testcases.append(testcase)
    return testcases


def _parse_available_form_factors(root: ET.Element) -> tuple[AvailableFormFactor, ...]:
    """Parse the `cts:availableFormFactors` block. Older CTS XMLs lack this
    element entirely; return an empty tuple in that case rather than raising,
    so older submissions remain parseable.
    """
    return tuple(
        AvailableFormFactor(
            type=_required_attr(ff_elem, "value"),
            view_configurations=tuple(
                AvailableViewConfiguration(
                    type=_required_attr(vc_elem, "value"),
                    environment_blend_modes=tuple(
                        _required_attr(ebm, "value")
                        for ebm in vc_elem.findall(
                            "./cts:availableEnvironmentBlendModes/cts:environmentBlendMode", NS,
                        )
                    ),
                )
                for vc_elem in ff_elem.findall(
                    "./cts:availableViewConfigurations/cts:viewConfiguration", NS,
                )
            ),
        )
        for ff_elem in root.findall(".//cts:availableFormFactors/cts:formFactor", NS)
    )


def _parse_runtime_instance_properties(root: ET.Element) -> frozenset[RuntimeInstanceProperties]:
    blocks = root.findall(".//cts:runtimeInstanceProperties", NS)
    if not blocks:
        raise ValueError("expected at least one runtimeInstanceProperties block")
    return frozenset(
        RuntimeInstanceProperties(
            api_version=_version_from_element(_required_child(block, "cts:apiVersion")),
            runtime_version=_version_from_element(_required_child(block, "cts:runtimeVersion")),
            runtime_name=_all_text(_required_child(block, "cts:runtimeName")).strip(),
        )
        for block in blocks
    )


def _parse_counts(root: ET.Element, testsuite: ET.Element) -> RunCounts:
    results = _required_child(root, ".//cts:ctsConformanceReport/cts:results")
    return RunCounts(
        success=_int_attr(results, "testSuccessCount"),
        failure=_int_attr(results, "testFailureCount"),
        skipped=_int_attr(testsuite, "skipped"),
        tests=_int_attr(testsuite, "tests"),
        errors=_int_attr(testsuite, "errors"),
    )


_OPTIONS_VARIED_BETWEEN_RUNS = frozenset({
    "graphicsPlugin",
    "enabledInstanceExtensions",
    "enabledInteractionProfiles",
})


def _canonical_test_options(element: ET.Element) -> str:
    """
    Convert a cts:testOptions element into a canonical string.

    This string should match across all runs in a submission.
    """
    parts = []
    for child in element:
        name = _cts_local_name(child.tag)
        # Most test options should be stable across runs; exclude only the
        # options that the submission steps instruct the submitter to vary.
        if name in _OPTIONS_VARIED_BETWEEN_RUNS:
            continue
        # This is a child of testOptions that we want to keep constant.
        # Serialize it to a string in a reproducible way.
        parts.append(_canonical_element(child))
    return "; ".join(sorted(parts))


def _canonical_element(element: ET.Element) -> str:
    """Convert one child element of cts:testOptions into a canonical string."""
    name = _cts_local_name(element.tag)
    # Serialize all attributes of this test option
    attrs = ",".join(
        f"{_cts_local_name(key)}={value}" for key, value in sorted(element.attrib.items())
    )
    # handle children if any (should not exist)
    children = ",".join(_canonical_element(child) for child in element)
    text = _own_text(element)
    return f"{name}({attrs})[{text}]{children}"


_LAYER_ATTRS = (
    "layerName",
    "layerVersion",
    "specVersionMajor",
    "specVersionMinor",
    "specVersionPatch",
)


def _api_layer_value(element: ET.Element) -> str:
    return " ".join(
        f"{name}={_required_attr(element, name)}"
        for name in _LAYER_ATTRS
    )


def _required_attr(element: ET.Element, name: str) -> str:
    """Get an element attribute, or raise an exception if missing."""
    value = element.get(name)
    if value is None:
        raise ValueError(f"expected required attribute {name}")
    return value


def _required_child(parent: ET.Element, child_path: str) -> ET.Element:
    """Find a child node, or raise an exception if missing."""
    child = parent.find(child_path, NS)
    if child is None:
        raise ValueError(f"expected required child {child_path}")
    return child


def _version_from_element(elem: ET.Element) -> Version:
    """Parse a version tag that contains an attribute per component."""
    return Version(
        major=_int_attr(elem, "major"),
        minor=_int_attr(elem, "minor"),
        patch=_int_attr(elem, "patch"),
    )


def _int_attr(element: ET.Element, name: str) -> int:
    """Get an attribute as an integer, or raise an exception if missing/invalid."""
    value = element.get(name)
    if value is None:
        raise ValueError(f"expected required integer attribute {name}")
    try:
        return int(value)
    except ValueError as exc:
        raise ValueError(f"expected integer attribute {name}, got {value!r}") from exc


def _all_text(element: ET.Element) -> str:
    return "".join(element.itertext())


def _own_text(element: ET.Element) -> str:
    return " ".join((element.text or "").split())


def _get_cts_attribute(element: ET.Element, name: str) -> Optional[str]:
    value = element.get(f"{{{CTS_NS}}}{name}")
    if value is None:
        value = element.get(f"cts:{name}")
    return value


def _outcome_kind_from_tag(tag: str) -> Optional[OutcomeKind]:
    """Convert tag name into test outcome."""
    if tag == "failure":
        return OutcomeKind.FAILURE
    if tag == "error":
        return OutcomeKind.ERROR
    if tag == "skipped":
        return OutcomeKind.SKIPPED
    if tag == f"{{{CTS_NS}}}warning":
        return OutcomeKind.WARNING
    return None


def _parse_source_location(body: str) -> SourceLocation:
    """Parse a test message to find the source location in it, if any."""
    matches = SOURCE_LOCATION_RE.findall(body)
    if not matches:
        return SourceLocation()
    path, delimiter, line = matches[-1]
    path = path.strip()
    close = ")" if delimiter == "(" else ""
    original = f"{path}{delimiter}{line}{close}"
    # These are compiler paths embedded in XML text, not local filesystem
    # paths. PureWindowsPath treats "\" and "/" alike (PurePosixPath would
    # not), normalizing Windows, POSIX, and mixed forms to forward slashes.
    path = PureWindowsPath(path).as_posix()
    marker = "src/conformance/"
    index = path.find(marker)
    if index >= 0:
        path = path[index:]
    short = f"{short_source_path(path)}{delimiter}{line}{close}"
    normalized = f"{path}:{line}"
    return SourceLocation(original=original, short=short, normalized=normalized)


def _cts_local_name(name: str) -> str:
    """Convert a namespaced tag name to an undecorated one."""
    return name.removeprefix(f"{{{CTS_NS}}}")
