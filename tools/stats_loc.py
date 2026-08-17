#!/usr/bin/env python3
"""Reproducible, dependency-free line-of-code counter for Nimvlets.

Developer-only tool. Counts physical lines in this repository's own
files, grouped the way AGENTS.md and docs/DECISION_LOG.md describe, and
excludes anything that isn't first-party source: build output,
FetchContent's `_deps` cache, `.git`, and any third-party/vendor tree.

Usage:
    python3 tools/stats_loc.py                  # print counts + delta
    python3 tools/stats_loc.py --save-baseline   # also persist current
                                                  # counts as the new
                                                  # baseline for the next
                                                  # block's delta

No external dependencies (standard library only), so it runs anywhere
Python 3 does, with no venv/pip step.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BASELINE_PATH = REPO_ROOT / "tools" / ".loc_baseline.json"

# Directories excluded outright, wherever they appear in the tree.
EXCLUDED_DIR_NAMES = {
    "build",
    "out",
    "dist",
    "_deps",
    ".git",
    "third_party",
    "third-party",
    "vendor",
    "__pycache__",
    ".cache",
}
# Directory *name prefixes* that are excluded (covers build/build-*,
# cmake-build-* from .gitignore's own patterns).
EXCLUDED_DIR_PREFIXES = ("build-", "cmake-build-")

CODE_EXTENSIONS = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".mm", ".m", ".c"}
TOOLING_EXTENSIONS = {".cmake", ".py", ".sh", ".yml", ".yaml"}
EDITORIAL_EXTENSIONS = {".json", ".csv", ".tsv"}
DOC_EXTENSIONS = {".md", ".txt"}

TOOLING_ROOT_FILES = {"CMakeLists.txt", "CMakePresets.json"}


def is_excluded_dir(name: str) -> bool:
    if name in EXCLUDED_DIR_NAMES:
        return True
    return any(name.startswith(prefix) for prefix in EXCLUDED_DIR_PREFIXES)


def iter_repo_files():
    stack = [REPO_ROOT]
    while stack:
        current = stack.pop()
        for entry in sorted(current.iterdir()):
            if entry.is_dir():
                if is_excluded_dir(entry.name):
                    continue
                stack.append(entry)
            elif entry.is_file():
                yield entry


def count_physical_lines(path: Path) -> int:
    try:
        with path.open("rb") as f:
            data = f.read()
    except OSError:
        return 0
    if not data:
        return 0
    # Physical line count: number of line separators, plus one more if
    # the file doesn't end with a trailing newline. This matches what
    # `wc -l` reports for newline-terminated files while still counting
    # a final unterminated line.
    lines = data.count(b"\n")
    if not data.endswith(b"\n"):
        lines += 1
    return lines


def classify(rel_path: Path) -> str | None:
    """Returns one of "application", "tooling", "tests", "editorial",
    "documentation", or None (not counted), for a path relative to the
    repo root.
    """
    parts = rel_path.parts
    suffix = rel_path.suffix.lower()
    top = parts[0]

    # CMakeLists.txt is "Tooling: CMake" wherever it lives in the tree
    # (root, src/*, tests/, ...) — checked before the directory-based
    # rules below so a CMakeLists.txt under src/ or tests/ isn't
    # silently dropped (it's build plumbing, not application/test C++).
    if rel_path.name == "CMakeLists.txt" or suffix == ".cmake":
        return "tooling"

    if suffix in DOC_EXTENSIONS:
        return "documentation"

    if top == "tests":
        if suffix in CODE_EXTENSIONS:
            return "tests"
        return None

    if top == "src":
        if suffix in CODE_EXTENSIONS:
            return "application"
        return None

    if top in ("cmake", "tools") and (suffix in TOOLING_EXTENSIONS or suffix == ""):
        return "tooling"

    if top == ".github" and len(parts) > 1 and parts[1] == "workflows":
        if suffix in TOOLING_EXTENSIONS:
            return "tooling"
        return None

    if len(parts) == 1 and rel_path.name in TOOLING_ROOT_FILES:
        return "tooling"

    if top == "assets" and suffix in EDITORIAL_EXTENSIONS:
        return "editorial"

    return None


def collect_counts():
    counts = {
        "application": 0,
        "tooling": 0,
        "tests": 0,
        "editorial": 0,
        "documentation": 0,
    }
    for path in iter_repo_files():
        rel_path = path.relative_to(REPO_ROOT)
        category = classify(rel_path)
        if category is None:
            continue
        counts[category] += count_physical_lines(path)
    return counts


def load_baseline() -> dict:
    if not BASELINE_PATH.exists():
        return {"code_total": 0, "relevant_total": 0}
    try:
        with BASELINE_PATH.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return {"code_total": 0, "relevant_total": 0}
    return {
        "code_total": int(data.get("code_total", 0)),
        "relevant_total": int(data.get("relevant_total", 0)),
    }


def save_baseline(code_total: int, relevant_total: int) -> None:
    BASELINE_PATH.parent.mkdir(parents=True, exist_ok=True)
    with BASELINE_PATH.open("w", encoding="utf-8") as f:
        json.dump({"code_total": code_total, "relevant_total": relevant_total}, f, indent=2)
        f.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--save-baseline",
        action="store_true",
        help="Persist this run's CODE TOTAL / RELEVANT TOTAL as the baseline for future delta computation.",
    )
    args = parser.parse_args()

    counts = collect_counts()
    code_total = counts["application"] + counts["tooling"] + counts["tests"]
    relevant_total = code_total + counts["editorial"] + counts["documentation"]

    baseline = load_baseline()
    delta_code_total = code_total - baseline["code_total"]
    delta_relevant_total = relevant_total - baseline["relevant_total"]

    print(f"Application: {counts['application']}")
    print(f"Tooling: {counts['tooling']}")
    print(f"Tests: {counts['tests']}")
    print(f"CODE TOTAL: {code_total}")
    print()
    print(f"Editorial/data: {counts['editorial']}")
    print(f"Documentation: {counts['documentation']}")
    print(f"RELEVANT TOTAL: {relevant_total}")
    print()
    print(f"Delta CODE TOTAL: {delta_code_total:+d}")
    print(f"Delta RELEVANT TOTAL: {delta_relevant_total:+d}")

    if args.save_baseline:
        save_baseline(code_total, relevant_total)
        print(f"\n(baseline saved to {BASELINE_PATH.relative_to(REPO_ROOT)})", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
