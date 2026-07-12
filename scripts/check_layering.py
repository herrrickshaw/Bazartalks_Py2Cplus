#!/usr/bin/env python3
"""
check_layering.py
------------------
Governance check (spiritual port of the original BazaarTalks repo's
`architecture/togaf.py govern` check, per the migration plan's CI section).

Enforces two structural rules by scanning CMakeLists.txt target_link_libraries
calls and #include directives, since a bad dependency here would silently
violate the migration plan's phase ordering / Python-boundary decision:

  1. libs/quant_core (and anything under libs/) must never link against
     services/* -- compute libraries are consumed by services, never the
     reverse. A violation here means a lower-level phase started depending
     on a higher-level one, breaking the plan's incremental build-up.
  2. No C++ target (anywhere in libs/ or services/) may reference
     python-sidecar/ in its CMakeLists.txt or #include a header from it.
     The contract with the sidecar is file-based (parquet/SQLite/Cassandra
     artifacts), never a build-time or header dependency -- a violation
     here means the hybrid-architecture boundary from the migration plan
     has been breached.

Exit code is non-zero on any violation, so this is wired as a required CI
check, not just a linter suggestion.
"""
from __future__ import annotations

import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
LIBS = REPO_ROOT / "libs"
SERVICES = REPO_ROOT / "services"
SIDECAR = REPO_ROOT / "python-sidecar"


def cmake_files() -> list[pathlib.Path]:
    return list((LIBS).rglob("CMakeLists.txt")) if LIBS.exists() else []


def violations() -> list[str]:
    errs: list[str] = []

    # Rule 1: libs/* CMakeLists must not reference services/*
    for f in cmake_files():
        text = f.read_text()
        if "services/" in text or re.search(r"\bservices\b", text):
            errs.append(f"{f.relative_to(REPO_ROOT)}: libs/ target references services/ "
                        f"(compute libraries must not depend on serving/orchestration layers)")

    # Rule 2: no C++ target (libs/ or services/) may reference python-sidecar/
    for base in (LIBS, SERVICES):
        if not base.exists():
            continue
        for f in list(base.rglob("CMakeLists.txt")) + list(base.rglob("*.cpp")) + list(
            base.rglob("*.hpp")
        ):
            text = f.read_text(errors="ignore")
            if "python-sidecar" in text:
                errs.append(f"{f.relative_to(REPO_ROOT)}: references python-sidecar/ "
                            f"(the sidecar contract is file-based artifacts only, never a "
                            f"build-time or #include dependency)")

    return errs


def main() -> int:
    errs = violations()
    if errs:
        print("Layering violations found:", file=sys.stderr)
        for e in errs:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print("Layering check passed: no libs/ -> services/ or C++ -> python-sidecar/ references.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
