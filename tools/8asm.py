#!/usr/bin/env python3
"""
Convenience front-end for assembling PDP-8 source with tools/pdp8_asm.py.

Usage patterns:
  * 8asm.py demo.asm                → assemble and print S-record to stdout
  * 8asm.py --list demo.asm         → print listing followed by S-record
  * 8asm.py demo.asm demo.srec      → assemble and write demo.srec
  * 8asm.py                         → show help
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

from pdp8_asm import AsmError, PDP8Assembler, render_listing, words_to_srec


def _load_source(path: Path) -> List[str]:
    if str(path) == "-":
        return sys.stdin.read().splitlines()
    try:
        return path.read_text().splitlines()
    except OSError as exc:
        raise AsmError(f"Unable to read {path}: {exc}")


def _records_from_memory(assembler: PDP8Assembler, memory: Dict[int, int]) -> List[str]:
    if not memory:
        raise AsmError("No output generated; empty program?")
    start_addr = assembler.symbols.get("START", min(memory))
    records = words_to_srec(memory, start_addr)
    if not records:
        raise AsmError("No S-record data produced.")
    return records


def write_records(path: Path, records: Sequence[str]) -> None:
    try:
        path.write_text("\n".join(records) + "\n")
    except OSError as exc:
        raise AsmError(f"Unable to write {path}: {exc}")


def print_records(records: Sequence[str]) -> None:
    for line in records:
        print(line)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description="Assemble PDP-8 source into Motorola S-records.")
    parser.add_argument("source", type=Path, nargs="?", help="Input assembly file")
    parser.add_argument("output", type=Path, nargs="?", help="Optional S-record output file")
    parser.add_argument("--list", action="store_true", help="Print an assembly listing before the S-record output")

    if not argv:
        parser.print_help()
        return 0

    args = parser.parse_args(argv)

    if args.source is None:
        parser.print_help()
        return 0

    memory: Dict[int, int] = {}
    listing_printed = False

    try:
        lines = _load_source(args.source)
    except AsmError as exc:
        print(exc, file=sys.stderr)
        return 1

    assembler = PDP8Assembler(lines)
    first_pass_error: Optional[AsmError] = None
    try:
        assembler.first_pass()
    except AsmError as exc:
        first_pass_error = exc
        if not args.list:
            print(exc, file=sys.stderr)
            return 1

    if args.list:
        rows: List[Tuple] = []
        errors: List[AsmError] = []
        if first_pass_error is not None:
            errors.append(first_pass_error)
        else:
            try:
                memory, rows, pass_errors = assembler.assemble_listing()
            except AsmError as exc:
                errors.append(exc)
                memory = {}
            else:
                errors.extend(pass_errors)
        listing_text = render_listing(args.source, assembler, rows, errors)
        print(listing_text)
        if errors:
            return 1
        listing_printed = True
    else:
        if first_pass_error is not None:
            print(first_pass_error, file=sys.stderr)
            return 1
        try:
            memory = assembler.second_pass()
        except AsmError as exc:
            print(exc, file=sys.stderr)
            return 1

    try:
        if not memory:
            memory = assembler.second_pass()
        records = _records_from_memory(assembler, memory)
    except AsmError as exc:
        print(exc, file=sys.stderr)
        return 1

    if args.output:
        try:
            write_records(args.output, records)
        except AsmError as exc:
            print(exc, file=sys.stderr)
            return 1
        return 0

    if listing_printed:
        print()
    print_records(records)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
