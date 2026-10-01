#!/usr/bin/env python3
"""Fail if a Markdown page under docs/ is not listed in the `nav` of zensical.toml, or the reverse.

A page missing from `nav` is still built but cannot be reached from the menu, so this check keeps the
navigation complete. Run from the repository root: python3 scripts/check_docs_nav.py
"""

import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"


def nav_pages(entries):
    """Yields every page path referenced by a (nested) nav list."""
    for entry in entries:
        if isinstance(entry, str):
            yield entry
        elif isinstance(entry, dict):
            for value in entry.values():
                if isinstance(value, str):
                    if not value.startswith(("http://", "https://")):
                        yield value
                else:
                    yield from nav_pages(value)


def main() -> int:
    config = tomllib.loads((ROOT / "zensical.toml").read_text(encoding="utf-8"))
    listed = set(nav_pages(config["project"].get("nav", [])))
    existing = {p.relative_to(DOCS).as_posix() for p in DOCS.rglob("*.md")}

    missing = sorted(existing - listed)
    dangling = sorted(listed - existing)
    for page in missing:
        print(f"error: docs/{page} is not listed in the nav of zensical.toml")
    for page in dangling:
        print(f"error: zensical.toml nav references docs/{page}, which does not exist")
    if missing or dangling:
        return 1
    print(f"Navigation OK ({len(existing)} pages)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
