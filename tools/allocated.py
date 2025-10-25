#!/usr/bin/env python3
"""
Display page-by-page memory utilisation for a PDP-8 assembly source file.

Each PDP-8 page contains 128 words (0o200). The report prints two rows per
page: the first row covers addresses base..base+0o077, and the second row
covers base+0o100..base+0o177. A hash (#) marks an assembled word, while a dot
(.) marks an unused location.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

try:
    from pdp8_asm import AsmError, PDP8Assembler
except ImportError as exc:  # pragma: no cover - defensive path handling
    print(f"Unable to import assembler module: {exc}", file=sys.stderr)
    sys.exit(1)


WORDS_PER_PAGE = 0o200
HALF_PAGE = 0o100


def assemble_memory(source: Path) -> Dict[int, int]:
    try:
        lines = source.read_text().splitlines()
    except OSError as exc:
        raise RuntimeError(f"Unable to read {source}: {exc}") from exc

    assembler = PDP8Assembler(lines)
    assembler.first_pass()
    return assembler.second_pass()


def make_page_rows(memory: Dict[int, int], page: int) -> List[Tuple[int, str]]:
    base_address = page * WORDS_PER_PAGE
    rows: List[Tuple[int, str]] = []
    for offset in (0, HALF_PAGE):
        row_base = base_address + offset
        chars = []
        for idx in range(HALF_PAGE):
            address = row_base + idx
            chars.append("#" if address in memory else ".")
        rows.append((row_base, "".join(chars)))
    return rows


def iter_pages(memory: Dict[int, int], show_all: bool) -> Iterable[Tuple[int, List[Tuple[int, str]]]]:
    if not memory:
        return []

    max_address = max(memory)
    max_page = max_address // WORDS_PER_PAGE
    pages: List[Tuple[int, List[Tuple[int, str]]]] = []

    for page in range(max_page + 1):
        rows = make_page_rows(memory, page)
        if not show_all and all("#" not in row_text for _, row_text in rows):
            continue
        pages.append((page, rows))
    return pages


def render(memory: Dict[int, int], show_all: bool) -> str:
    pages = list(iter_pages(memory, show_all))
    if not pages:
        return "No assembled output."

    lines: List[str] = []
    for page, rows in pages:
        lines.append(f"Page {page:02o}")
        for base, row_text in rows:
            lines.append(f"{base:04o} {row_text}")
        lines.append("")
    return "\n".join(lines).rstrip()


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Show PDP-8 page allocations from assembly source.")
    parser.add_argument("source", type=Path, help="PAL-style PDP-8 assembly source file")
    parser.add_argument(
        "--all-pages",
        action="store_true",
        help="Include pages without any assembled words.",
    )
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    try:
        memory = assemble_memory(args.source)
    except (RuntimeError, AsmError) as exc:
        print(exc, file=sys.stderr)
        return 1

    output = render(memory, args.all_pages)
    print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

