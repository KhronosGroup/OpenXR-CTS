#!/usr/bin/env python3
#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""Generate review Markdown from OpenXR CTS XML results."""

import argparse
import sys
from pathlib import Path
from typing import Optional

from cts_results.aggregate import summarize
from cts_results.parse import collect_runs
from cts_results.render import DEFAULT_WARNING_DETAIL_LIMIT, render_markdown


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    review_md = subparsers.add_parser(
        "review-md", help="Render a human-readable Markdown review summary."
    )
    review_md.add_argument("paths", nargs="*", type=Path, default=[Path(".")])
    review_md.add_argument(
        "--output", "-o",
        type=Path,
        help="Output path. Defaults to stdout. Use '-' for stdout explicitly.",
    )
    review_md.add_argument(
        "--checkboxes",
        action="store_true",
        help="Use GFM checkboxes for review-action lists.",
    )
    review_md.add_argument(
        "--warning-detail-limit",
        type=int,
        default=DEFAULT_WARNING_DETAIL_LIMIT,
        help=(
            "Per-location warning sample cap. Above this many distinct messages, only "
            f"the most common one is shown. Pass 0 to disable. Default: {DEFAULT_WARNING_DETAIL_LIMIT}."
        ),
    )

    args = parser.parse_args(argv)
    summary = summarize(collect_runs(args.paths or [Path(".")]))

    match args.command:
        case "review-md":
            text = render_markdown(
                summary,
                checkboxes=args.checkboxes,
                warning_detail_limit=args.warning_detail_limit,
            )
        case _:
            parser.error(f"unknown command: {args.command}")
    _emit(text, args.output)
    return 0


def _emit(text: str, destination: Optional[Path]) -> None:
    if destination is None or str(destination) == "-":
        if hasattr(sys.stdout, "reconfigure"):
            sys.stdout.reconfigure(encoding="utf-8")  # pyright: ignore[reportAttributeAccessIssue]
        print(text, end="")
    else:
        destination.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
