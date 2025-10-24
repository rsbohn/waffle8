#!/usr/bin/env python3
"""
Generate PDP-8 paper tape data for the `demo/ptprint.asm` program.

This script builds a text calendar (entire year or a single month),
encodes it using the DEC SIXBIT scheme consumed by `ptprint.asm`, and
writes the result as a paper tape image that the emulator can load.
"""

from __future__ import annotations

import argparse
import calendar
from datetime import date
from pathlib import Path
from typing import Iterable, List


SIXBIT_CR = 30
SIXBIT_LF = 31


def build_calendar_text(year: int, month: int | None) -> str:
    """Return an uppercase text calendar for the requested period."""
    cal = calendar.TextCalendar(firstweekday=6)  # Sunday-first layout
    if month is not None:
        header = f"CALENDAR {calendar.month_name[month].upper()} {year}\n"
        body = cal.formatmonth(year, month)
        return header + body.upper()

    header = f"CALENDAR YEAR {year}\n"
    body = cal.formatyear(year, 2, 1, 4, 3)  # width=2, blank lines=1, padding tuned for printer
    return header + body.upper()


def ascii_to_sixbit(char: str) -> int:
    """Translate a single character to its SIXBIT value."""
    if char == " ":
        return 0
    if char == "\r":
        return SIXBIT_CR
    if char == "\n":
        return SIXBIT_LF

    if "A" <= char <= "Z":
        return ord(char) - 64  # A=1 .. Z=26
    if "0" <= char <= "9":
        return ord(char) - 48 + 32  # 0=32 .. 9=41

    lookup = {
        "!": 42,
        ",": 43,
        "-": 44,
        ".": 45,
        "'": 46,
        ":": 47,
        ";": 48,
        "?": 49,
    }
    if char in lookup:
        return lookup[char]

    raise ValueError(f"Unsupported character for SIXBIT encoding: {repr(char)}")


def text_to_sixbit_chunks(text: str) -> List[str]:
    """Convert calendar text to a list of six-bit binary strings."""
    chunks: List[str] = []
    for line in text.splitlines():
        if line:
            for ch in line:
                chunks.append(format(ascii_to_sixbit(ch), "06b"))
        else:
            # Preserve intentionally blank lines with CR/LF only.
            pass
        chunks.append(format(SIXBIT_CR, "06b"))
        chunks.append(format(SIXBIT_LF, "06b"))

    if not chunks:
        return chunks

    # Ensure an even number of six-bit values (pairs map to 12-bit words).
    if len(chunks) % 2:
        chunks.append(format(0, "06b"))

    return chunks


def chunk_lines(bits: Iterable[str], label: str, width: int = 64) -> List[str]:
    """Group six-bit chunks into labelled tape records."""
    lines: List[str] = []
    group: List[str] = []
    counter = 1

    for bit in bits:
        group.append(bit)
        if len(group) == width:
            lines.append(f"{label}{counter:03o}: " + " ".join(group))
            counter += 1
            group = []

    if group:
        lines.append(f"{label}{counter:03o}: " + " ".join(group))

    return lines


def write_tape(lines: Iterable[str], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="ascii")


def parse_args() -> argparse.Namespace:
    today = date.today()
    parser = argparse.ArgumentParser(
        description="Generate PDP-8 paper tape calendar for demo/ptprint.asm",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("year", nargs="?", type=int, default=today.year, help="Calendar year to emit")
    parser.add_argument(
        "--month",
        type=int,
        choices=range(1, 13),
        help="Optional month (1-12). If omitted the full year is generated.",
    )
    parser.add_argument(
        "--label",
        default="CA",
        help="Two-character paper tape label to use in the generated file",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output tape path (defaults to tapes/calendar-<year>.tape or tapes/calendar-<year>-<month>.tape)",
    )
    parser.add_argument(
        "--line-width",
        type=int,
        default=64,
        help="Number of six-bit chunks per tape line",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if len(args.label) != 2 or not args.label.isalpha() or not args.label.isupper():
        raise SystemExit("Paper tape label must be exactly two uppercase letters (e.g., 'CA').")

    output_path = args.output
    if output_path is None:
        if args.month is None:
            output_path = Path("tapes") / f"calendar-{args.year}.tape"
        else:
            output_path = Path("tapes") / f"calendar-{args.year:04d}-{args.month:02d}.tape"

    text = build_calendar_text(args.year, args.month)
    chunks = text_to_sixbit_chunks(text)
    tape_lines = chunk_lines(chunks, args.label, args.line_width)

    write_tape(tape_lines, output_path)

    summary = f"Generated calendar tape with {len(chunks)} six-bit values across {len(tape_lines)} records."
    info = f"  Output: {output_path}"
    sample = "\n".join(tape_lines[:3])
    print(summary)
    print(info)
    if tape_lines:
        print("\nFirst few records:\n" + sample)


if __name__ == "__main__":
    main()
