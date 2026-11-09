#!/usr/bin/env python3
#
# Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# findLegacyExtensionSections.py - search extension chapters for the old
# bold-text section layout that the style guide has replaced.
#
# Usage: scripts/findLegacyExtensionSections.py [sources/chapters/extensions/...]...
#
# If one or more adoc file paths (relative to specification dir) are provided,
# only those paths are checked; otherwise all extension chapters are checked.
#
# Existing extensions that still use the old layout are listed in
# legacy_extension_sections_allowlist.txt. Remove a file's entries once it
# has been converted.

import os
import sys
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

import findBareNormatives

LEGACY_SECTION_CHECKS: List[Tuple[str, str]] = [
    (
        r"^\*New [A-Z][A-Za-z ]*\*\s*$",
        "hand-written interface list - replace with "
        "include::{generated}/interfaces/<extension>.adoc[leveloffset=1]",
    ),
    (
        r"^\*(Overview|Issues|Version History)\*\s*$",
        "bold pseudo-heading - use a ==== section heading",
    ),
]


def check_legacy_sections(
    sources: Sequence[str],
    output: Optional[Callable[[str], None]] = None,
    allowlist: Optional[dict] = None,
) -> bool:
    """Return True if any source uses the legacy section layout."""
    return findBareNormatives.check_sources(
        sources, LEGACY_SECTION_CHECKS, output=output, allowlist=allowlist
    )


def _list_default_sources() -> List[str]:
    script_dir = Path(__file__).resolve().parent
    spec_dir = script_dir.parent
    extensions_dir = spec_dir / "sources" / "chapters" / "extensions"
    return sorted(str(p.relative_to(spec_dir)) for p in extensions_dir.rglob("*.adoc"))


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    spec_dir = script_dir.parent
    os.chdir(spec_dir)

    allowlist_path = script_dir / "legacy_extension_sections_allowlist.txt"
    allowlist = findBareNormatives.load_allowlist(str(allowlist_path))

    if len(sys.argv) > 1:
        sources = sys.argv[1:]
        print(f"Only checking {' '.join(sources)}")
        print()
    else:
        sources = _list_default_sources()

    if check_legacy_sections(sources, allowlist=allowlist):
        print("Failures detected")
        sys.exit(1)


if __name__ == "__main__":
    main()
