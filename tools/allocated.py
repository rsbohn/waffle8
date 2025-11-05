#!/usr/bin/env python3
"""
Display page-by-page memory utilisation for a PDP-8 assembly source file.

Each PDP-8 page contains 128 words (0o200). The report prints two rows per
page: the first row covers addresses base..base+0o077, and the second row
covers base+0o100..base+0o177. A hash (#) marks an assembled word, while a dot
(.) marks an unused location. An exclamation mark (!) marks an address that 
has been written more than once (overallocation).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from collections import defaultdict
from typing import Dict, Iterable, List, Set, Tuple

try:
    from pdp8_asm import AsmError, PDP8Assembler
except ImportError as exc:  # pragma: no cover - defensive path handling
    print(f"Unable to import assembler module: {exc}", file=sys.stderr)
    sys.exit(1)


WORDS_PER_PAGE = 0o200
HALF_PAGE = 0o100


def assemble_memory(source: Path) -> Tuple[Dict[int, int], Set[int], Dict[int, List[str]]]:
    """Assemble source and return memory dict, duplicate addresses, and duplicate details."""
    try:
        lines = source.read_text().splitlines()
    except OSError as exc:
        raise RuntimeError(f"Unable to read {source}: {exc}") from exc

    assembler = PDP8Assembler(lines)
    assembler.first_pass()
    
    # Track how many times each address is written and what source lines
    address_counts: Dict[int, int] = defaultdict(int)
    address_sources: Dict[int, List[str]] = defaultdict(list)
    memory: Dict[int, int] = {}
    
    for stmt in assembler.statements:
        address_counts[stmt.address] += 1
        address_sources[stmt.address].append(stmt.raw.strip())
    
    memory = assembler.second_pass()
    
    # Find addresses written more than once
    duplicates = {addr for addr, count in address_counts.items() if count > 1}
    
    # Keep only duplicate sources
    duplicate_sources = {addr: sources for addr, sources in address_sources.items() if addr in duplicates}
    
    return memory, duplicates, duplicate_sources


def make_page_rows(memory: Dict[int, int], duplicates: Set[int], page: int) -> List[Tuple[int, str]]:
    base_address = page * WORDS_PER_PAGE
    rows: List[Tuple[int, str]] = []
    for offset in (0, HALF_PAGE):
        row_base = base_address + offset
        chars = []
        for idx in range(HALF_PAGE):
            address = row_base + idx
            if address in duplicates:
                chars.append("!")
            elif address in memory:
                chars.append("#")
            else:
                chars.append(".")
        rows.append((row_base, "".join(chars)))
    return rows


def iter_pages(memory: Dict[int, int], duplicates: Set[int], show_all: bool) -> Iterable[Tuple[int, List[Tuple[int, str]]]]:
    if not memory:
        return []

    max_address = max(memory)
    max_page = max_address // WORDS_PER_PAGE
    pages: List[Tuple[int, List[Tuple[int, str]]]] = []

    for page in range(max_page + 1):
        rows = make_page_rows(memory, duplicates, page)
        if not show_all and all("#" not in row_text and "!" not in row_text for _, row_text in rows):
            continue
        pages.append((page, rows))
    return pages


def render(memory: Dict[int, int], duplicates: Set[int], duplicate_sources: Dict[int, List[str]], show_all: bool, show_details: bool, source_file: Path) -> str:
    pages = list(iter_pages(memory, duplicates, show_all))
    if not pages:
        return "No assembled output."

    lines: List[str] = []
    for page, rows in pages:
        lines.append(f"Page {page:02o}")
        for base, row_text in rows:
            lines.append(f"{base:04o} {row_text}")
        lines.append("")
    
    # Add summary of duplicate addresses if any found
    if duplicates:
        lines.append("=" * 40)
        lines.append(f"WARNING: {len(duplicates)} memory location(s) written multiple times.")
        
        if show_details:
            lines.append("")
            # Show detailed source information for each duplicate
            for addr in sorted(duplicates):
                sources = duplicate_sources.get(addr, [])
                lines.append(f"  {addr:04o} ({len(sources)} writes):")
                for i, source in enumerate(sources, 1):
                    lines.append(f"    {i}. {source}")
        else:
            lines.append(f"# python3 tools/allocated.py {source_file} --show-duplicates")
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
    parser.add_argument(
        "--show-duplicates",
        action="store_true",
        help="Show source lines for each duplicate address.",
    )
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    try:
        memory, duplicates, duplicate_sources = assemble_memory(args.source)
    except (RuntimeError, AsmError) as exc:
        print(exc, file=sys.stderr)
        return 1

    output = render(memory, duplicates, duplicate_sources, args.all_pages, args.show_duplicates, args.source)
    print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

