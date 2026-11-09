#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""Render CTS submission summaries as Markdown."""

from enum import IntEnum
from pathlib import Path
from typing import Optional, assert_never

from .model import (MissingGraphicsCoverageIssue, MissingRunStepIssue, Outcome,
                    RunStep, ScalarMetadataInconsistency,
                    SetMetadataInconsistency, SubmissionIssue, Summary,
                    WarningGroup)

DEFAULT_WARNING_DETAIL_LIMIT = 5


def render_markdown(
    summary: Summary,
    *,
    checkboxes: bool = False,
    warning_detail_limit: int = DEFAULT_WARNING_DETAIL_LIMIT,
) -> str:
    lines: list[str] = []
    lines.append("# CTS Submission Summary")
    lines.append("")
    lines.append(f"- XML files parsed: {len(summary.runs)}")
    lines.append(f"- Failures: {len(summary.failures)}")
    lines.append(f"- Errors: {len(summary.errors)}")
    lines.append(f"- Skips: {summary.skip_count}")
    lines.append(f"- Warnings: {summary.warning_count}")
    lines.append("")

    _append_review_notes(lines, summary)
    _append_submission_issues(lines, summary, checkboxes=checkboxes)
    _append_runs(lines, summary)
    _append_outcomes(lines, summary, "Failures", summary.failures, checkboxes=checkboxes)
    _append_outcomes(lines, summary, "Errors", summary.errors, checkboxes=checkboxes)
    _append_skip_summary(lines, summary, checkboxes=checkboxes)
    _append_warning_summary(lines, summary, checkboxes=checkboxes, warning_detail_limit=warning_detail_limit)
    _append_extension_summary(lines, summary)
    _append_metadata_inconsistencies(lines, summary, checkboxes=checkboxes)
    return "\n".join(lines).rstrip() + "\n"


def _append_review_notes(lines: list[str], summary: Summary) -> None:
    lines.append("## Review Notes")
    lines.append("")
    if summary.submission_issues:
        lines.append(f"- **Submission completeness: {len(summary.submission_issues)} issue(s) found.**")
    else:
        lines.append("- Submission completeness: no issues found.")
    if summary.failures or summary.errors:
        lines.append("- **Failures or errors are present**; inspect the detail sections.")
    else:
        lines.append("- No failures or errors found.")
    if summary.metadata_inconsistencies:
        fields = ", ".join(item.field for item in summary.metadata_inconsistencies)
        lines.append(f"- Metadata differs across XML files: {fields}.")
    if summary.notable_skips:
        trivial_note = (
            f" (and {len(summary.likely_trivial_skips)} likely-trivial)"
            if summary.likely_trivial_skips
            else ""
        )
        lines.append(
            f"- {len(summary.notable_skips)} notable skip(s){trivial_note} -- see Skips."
        )
    if summary.tag_data_unavailable:
        lines.append("- Some XML files lack `cts:tags`, so extension test correlation may be incomplete.")
    lines.append("")


def _append_runs(lines: list[str], summary: Summary) -> None:
    lines.append("## Runs")
    lines.append("")
    lines.append("| File | Step | Graphics | Filters | Success | Failure | Skipped | Errors |")
    lines.append("| --- | --- | --- | --- | ---: | ---: | ---: | ---: |")
    for run in summary.runs:
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} | {} |".format(
                _input_label(summary, run.path),
                run.run_step,
                run.graphics_plugin,
                _md_escape_table_cell(run.filters.raw),
                run.counts.success,
                run.counts.failure,
                run.counts.skipped,
                run.counts.errors,
            )
        )
    lines.append("")


def _append_outcomes(
    lines: list[str], summary: Summary, title: str, outcomes: list[Outcome],
    *, checkboxes: bool = False,
) -> None:
    lines.append(f"## {title}")
    lines.append("")
    if not outcomes:
        lines.append(f"No {title.lower()} found.")
        lines.append("")
        return
    bullet = _bullet(checkboxes)
    for outcome in outcomes:
        location = f" at `{outcome.source_location.short}`" if outcome.source_location.short else ""
        lines.append(f"{bullet} `{_input_label(summary, outcome.run_path)}` `{outcome.testcase_name}`{location}")
        lines.append("")
        lines.append("```text")
        lines.extend(outcome.body.splitlines())
        lines.append("```")
        lines.append("")


def _append_skip_summary(lines: list[str], summary: Summary, *, checkboxes: bool = False) -> None:
    lines.append("## Skips")
    lines.append("")
    if not summary.notable_skips and not summary.likely_trivial_skips:
        lines.append("No skips found.")
        lines.append("")
        return
    # Both subsections get checkboxes when enabled: the classification only
    # explains *why* the test was skipped, not whether the skip *message* is
    # what the reviewer expected -- worth allowing a quick tick-through.
    _append_skip_subsection(
        lines, "Notable", summary.notable_skips,
        empty_text="No notable skips.",
        checkboxes=checkboxes,
    )
    _append_skip_subsection(
        lines,
        "Likely-trivial",
        summary.likely_trivial_skips,
        intro=(
            "Skips of tests tagged with at least one `XR_*` feature the runtime "
            "doesn't advertise (extension absent, or `XR_VERSION_M_N` higher than "
            "any queried apiVersion)."
        ),
        empty_text="No likely-trivial skips.",
        checkboxes=checkboxes,
    )


def _append_skip_subsection(
    lines: list[str],
    heading: str,
    outcomes: list[Outcome],
    *,
    intro: Optional[str] = None,
    empty_text: str,
    checkboxes: bool = False,
) -> None:
    lines.append(f"### {heading}")
    lines.append("")
    if intro:
        lines.append(intro)
        lines.append("")
    if not outcomes:
        lines.append(empty_text)
        lines.append("")
        return
    bullet = _bullet(checkboxes)
    for (_location, message), grouped in _group_outcomes_by_location_and_message(outcomes):
        names = sorted({outcome.testcase_name for outcome in grouped})
        locations = sorted({outcome.source_location.short for outcome in grouped})
        lines.append(f"{bullet} **{len(grouped)} skip(s)** at {_inline_code_list(locations)}")
        _append_message_field(lines, "  ", "Message", message)
        lines.append("  - Tests:")
        for name in names:
            lines.append(f"    - `{name}`")
    lines.append("")


def _append_warning_summary(
    lines: list[str], summary: Summary, *, checkboxes: bool = False, warning_detail_limit: int,
) -> None:
    lines.append("## Warnings")
    lines.append("")
    if not summary.warning_groups:
        lines.append("No warnings found.")
        lines.append("")
        return
    bullet = _bullet(checkboxes)
    for group in summary.warning_groups:
        locations = sorted(location.short for location in group.source_locations)
        lines.append(f"{bullet} **{group.count} warning(s)** at {_inline_code_list(locations)}")
        lines.append(f"  - Inputs: {_warning_input_list(summary, group)}")
        _append_warning_messages(lines, group, warning_detail_limit)
    lines.append("")


def _append_extension_summary(lines: list[str], summary: Summary) -> None:
    lines.append("## Extensions")
    lines.append("")
    if not summary.extension_summaries:
        lines.append("No available or tagged extensions found.")
        lines.append("")
        return
    lines.append("| Extension | Status | Tagged tests | Skipped | Failed |")
    lines.append("| --- | --- | ---: | ---: | ---: |")
    for item in summary.extension_summaries:
        lines.append(
            f"| `{item.name}` | {item.readable_status()} | {item.tagged_tests} | {item.skipped_tests} | {item.failed_tests} |"
        )
    lines.append("")


def _append_metadata_inconsistencies(
    lines: list[str], summary: Summary, *, checkboxes: bool = False,
) -> None:
    if not summary.metadata_inconsistencies:
        return
    lines.append("## Metadata Inconsistencies")
    lines.append("")
    bullet = _bullet(checkboxes)
    for item in summary.metadata_inconsistencies:
        lines.append(f"{bullet} **{item.field}**")
        match item:
            case SetMetadataInconsistency():
                _append_set_inconsistency(lines, summary, item.by_set)
            case ScalarMetadataInconsistency():
                for value, paths in item.by_value.items():
                    files = ", ".join(_input_label(summary, path) for path in paths)
                    lines.append(f"  - `{value or '(empty)'}`: {files}")
            case _:
                assert_never(item)
    lines.append("")


def _append_set_inconsistency(
    lines: list[str], summary: Summary, by_set: dict[Path, frozenset]
) -> None:
    baseline = frozenset.intersection(*by_set.values())
    baseline_items = sorted(str(item) for item in baseline)
    if baseline_items:
        lines.append("  - Shared by all inputs:")
        for item in baseline_items:
            lines.append(f"    - `{item}`")
    else:
        lines.append("  - Shared by all inputs: (none)")
    for path, items in by_set.items():
        extras = sorted(str(item) for item in (items - baseline))
        label = _input_label(summary, path)
        if extras:
            lines.append(f"  - {label} adds:")
            for item in extras:
                lines.append(f"    - `{item}`")
        else:
            lines.append(f"  - {label} adds: (nothing beyond baseline)")


def _append_submission_issues(
    lines: list[str], summary: Summary, *, checkboxes: bool = False,
) -> None:
    if not summary.submission_issues:
        return
    lines.append("## Submission Completeness")
    lines.append("")
    bullet = _bullet(checkboxes)
    for issue in summary.submission_issues:
        lines.append(f"{bullet} {_submission_issue_message(issue)}")
    lines.append("")


def _submission_issue_message(issue: SubmissionIssue) -> str:
    match issue:
        case MissingGraphicsCoverageIssue(run_step=RunStep.AUTOMATED):
            return (
                f"`{issue.extension}` is advertised, but no automated run used "
                f"graphicsPlugin `{issue.graphics_plugin}`."
            )
        case MissingGraphicsCoverageIssue(run_step=RunStep.COMPOSITION):
            return (
                f"`{issue.extension}` is advertised, but no `[composition][interactive]` "
                f"run used graphicsPlugin `{issue.graphics_plugin}`."
            )
        case MissingRunStepIssue(run_step=RunStep.AUTOMATED):
            return "No automated run found in this submission."
        case MissingRunStepIssue(run_step=RunStep.COMPOSITION):
            return "No `[composition][interactive]` run found in this submission."
        case MissingRunStepIssue(run_step=RunStep.SCENARIO):
            return "No `[scenario][interactive]` run found in this submission."
        case MissingRunStepIssue(run_step=RunStep.ACTIONS):
            return "No `[actions][interactive]` run found in this submission."
        case _:
            raise ValueError(f"unhandled submission issue: {issue!r}")


def _bullet(checkboxes: bool) -> str:
    """Outer bullet token for review-action lists; GFM task-list when enabled."""
    return "- [ ]" if checkboxes else "-"


# Feature classes, ordered broadest first: tests not gated by any XR_* feature
# come first, then version-gated, then progressively less-core extension scopes.
class FeatureClass(IntEnum):
    CORE = 0
    VERSION = 1
    KHR = 2
    EXT = 3
    VENDOR = 4


def _feature_tag_sort_key(tag: str) -> tuple[FeatureClass, str]:
    if tag.startswith("XR_VERSION_"):
        return (FeatureClass.VERSION, tag)
    if tag.startswith("XR_KHR_"):
        return (FeatureClass.KHR, tag)
    if tag.startswith("XR_EXT_"):
        return (FeatureClass.EXT, tag)
    return (FeatureClass.VENDOR, tag)


def _outcome_tag_sort_key(outcome: Outcome) -> tuple[FeatureClass, str]:
    if outcome.testcase_tags is None:
        return (FeatureClass.CORE, "")
    tag_keys = [
        _feature_tag_sort_key(tag)
        for tag in outcome.testcase_tags.feature_tags
    ]
    # Among an outcome's own feature tags, the most-niche one gates it: a
    # test tagged [XR_VERSION_1_1][XR_EXT_eye_gaze_interaction] is extension-
    # gated more than version-gated.
    return max(tag_keys) if tag_keys else (FeatureClass.CORE, "")


def _group_outcomes_by_location_and_message(outcomes: list[Outcome]) -> list[tuple[tuple[str, str], list[Outcome]]]:
    def group_tag_sort_key(grouped_outcomes: list[Outcome]) -> tuple[FeatureClass, str]:
        # A group can span testcases with different tag sets (e.g. several tests
        # skipping at the same helper line). Sort by the least-niche participant.
        return min((_outcome_tag_sort_key(outcome) for outcome in grouped_outcomes), default=(FeatureClass.CORE, ""))

    def group_sort_key(item: tuple[tuple[str, str], list[Outcome]]) -> tuple[tuple[FeatureClass, str], int, str, str]:
        (location, message), grouped_outcomes = item
        return (group_tag_sort_key(grouped_outcomes), -len(grouped_outcomes), message, location)

    grouped: dict[tuple[str, str], list[Outcome]] = {}
    for outcome in outcomes:
        grouped.setdefault((outcome.source_location.normalized, outcome.normalized_message), []).append(outcome)
    return sorted(grouped.items(), key=group_sort_key)


def _input_label(summary: Summary, path: Path) -> str:
    return summary.input_labels.get(path, path.name)


def _warning_input_list(summary: Summary, group: WarningGroup) -> str:
    return ", ".join(
        f"`{_md_escape_table_cell(_input_label(summary, path))}` ({count}x)"
        for path, count in sorted(group.per_input.items(), key=lambda item: (_input_label(summary, item[0]), item[1]))
    )


def _append_warning_messages(lines: list[str], group: WarningGroup, detail_limit: int) -> None:
    messages = sorted(group.messages.items(), key=lambda item: (-item[1], item[0]))
    if not messages:
        lines.append("  - Messages: _(none)_")
        return

    over_limit = detail_limit > 0 and len(messages) > detail_limit
    shown = messages[:1] if over_limit else messages
    if not over_limit and len(shown) == 1 and "\n" not in shown[0][0]:
        message, count = shown[0]
        lines.append(f"  - Messages: {message} ({count}x)")
        return

    lines.append("  - Messages:")
    for message, count in shown:
        if "\n" in message:
            lines.append(f"    - ({count}x)")
            _append_code_block(lines, "      ", message)
        else:
            lines.append(f"    - {message} ({count}x)")
    if over_limit:
        lines.append(
            f"    - _{len(messages)} distinct messages at this location; showing one sample._"
        )


def _append_message_field(lines: list[str], indent: str, label: str, message: str) -> None:
    """Render `indent- label: message`; multi-line bodies get a fenced code block."""
    bullet_prefix = f"{indent}- "
    body_indent = indent + "  "
    if not message:
        lines.append(f"{bullet_prefix}{label}: _(empty)_")
    elif "\n" in message:
        lines.append(f"{bullet_prefix}{label}:")
        _append_code_block(lines, body_indent, message)
    else:
        lines.append(f"{bullet_prefix}{label}: {message}")


def _append_code_block(lines: list[str], indent: str, body: str) -> None:
    lines.append(f"{indent}```text")
    for body_line in body.splitlines():
        lines.append(f"{indent}{body_line}")
    lines.append(f"{indent}```")


def _inline_code_list(items: list[str]) -> str:
    return ", ".join(f"`{item}`" for item in items)


def _md_escape_table_cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", "<br>")
