#!/usr/bin/env python3
"""Utility for inspecting PDP-8 magtape record files."""

from __future__ import annotations

import argparse
import datetime as _dt
import os
import sys
import textwrap
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List

SENTINEL_WORD = 0xFFFF
WORD_MASK = 0x0FFF


@dataclass
class RecordInfo:
    path: Path
    length: int
    words: List[int]
    partial: bool
    timestamp: float

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def mtime(self) -> _dt.datetime:
        return _dt.datetime.fromtimestamp(self.timestamp)


def read_record(path: Path) -> RecordInfo:
    data = path.read_bytes()
    if len(data) < 2:
        raise ValueError(f"{path} is too short to contain a record header")

    declared_length = int.from_bytes(data[:2], "little") & WORD_MASK
    words: List[int] = []
    offset = 2
    partial = False

    for _ in range(declared_length):
        if offset + 2 > len(data):
            partial = True
            break
        word = int.from_bytes(data[offset : offset + 2], "little") & WORD_MASK
        words.append(word)
        offset += 2

    if offset + 2 > len(data):
        partial = True
    else:
        sentinel = int.from_bytes(data[offset : offset + 2], "little")
        if sentinel != SENTINEL_WORD:
            partial = True

    return RecordInfo(path=path, length=declared_length, words=words, partial=partial, timestamp=path.stat().st_mtime)


def iter_records(directory: Path) -> Iterable[RecordInfo]:
    if not directory.exists():
        raise FileNotFoundError(f"Directory {directory} does not exist")
    if not directory.is_dir():
        raise NotADirectoryError(f"{directory} is not a directory")

    for entry in sorted(directory.iterdir()):
        if entry.is_file() and entry.suffix.lower() == ".tap":
            try:
                yield read_record(entry)
            except Exception as exc:  # pragma: no cover - defensive
                print(f"warning: unable to parse {entry}: {exc}", file=sys.stderr)
                continue


def command_list(args: argparse.Namespace) -> int:
    directory = Path(args.directory)
    records = list(iter_records(directory))
    if not records:
        print("No magtape records found.")
        return 0

    print(f"Records in {directory}:")
    print(f"{'Name':<40} {'Words':>8} {'Header':>8} {'Partial':>8} {'Modified':>20}")
    print("-" * 90)
    for record in records:
        mod = record.mtime.strftime("%Y-%m-%d %H:%M:%S")
        print(
            f"{record.name:<40} {len(record.words):>8} {record.length:>8} "
            f"{('yes' if record.partial else 'no'):>8} {mod:>20}"
        )
    return 0


def write_raw_words(words: Iterable[int], output: Path) -> None:
    with output.open("wb") as fp:
        for word in words:
            fp.write(int(word & WORD_MASK).to_bytes(2, "little"))


def command_extract(args: argparse.Namespace) -> int:
    source = Path(args.record)
    record = read_record(source)
    if record.partial and not args.force:
        print(f"Record {record.name} is partial; use --force to extract anyway.", file=sys.stderr)
        return 1

    output = Path(args.output)
    if output.is_dir():
        output = output / f"{record.path.stem}.bin"

    write_raw_words(record.words, output)
    print(f"Wrote {len(record.words)} word(s) to {output}.")
    if record.partial:
        print("Warning: input record was partial; trailing data may be missing.", file=sys.stderr)
    return 0


def select_records_for_bundle(records: List[RecordInfo], latest_only: bool) -> List[RecordInfo]:
    if not records:
        return []
    if not latest_only:
        return records
    latest = max(records, key=lambda r: r.timestamp)
    same_session = [r for r in records if abs(r.timestamp - latest.timestamp) < 5.0]
    return same_session or [latest]


def command_bundle(args: argparse.Namespace) -> int:
    directory = Path(args.directory)
    records = list(iter_records(directory))
    if not records:
        print("No magtape records to bundle.")
        return 0

    chosen = select_records_for_bundle(records, args.latest)
    archive_path = Path(args.output)
    archive_path.parent.mkdir(parents=True, exist_ok=True)

    mode = "w"
    with zipfile.ZipFile(archive_path, mode, compression=zipfile.ZIP_DEFLATED) as zf:
        for record in chosen:
            zf.write(record.path, arcname=record.path.name)
    print(f"Bundled {len(chosen)} record(s) into {archive_path}.")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Inspect and extract PDP-8 magtape records.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent(
            """
            Examples:
              magtape_tool.py list magtape
              magtape_tool.py extract magtape/record-20251019-120000.tap dumps/
              magtape_tool.py bundle magtape --output sessions.zip --latest
            """
        ),
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list", help="List records in a directory")
    list_parser.add_argument("directory", help="Directory containing .tap records")
    list_parser.set_defaults(func=command_list)

    extract_parser = subparsers.add_parser("extract", help="Extract record contents")
    extract_parser.add_argument("record", help="Path to a .tap record file")
    extract_parser.add_argument("output", help="Output file or directory for extracted words")
    extract_parser.add_argument("--force", action="store_true", help="Extract even if record is partial")
    extract_parser.set_defaults(func=command_extract)

    bundle_parser = subparsers.add_parser("bundle", help="Bundle records into a zip archive")
    bundle_parser.add_argument("directory", help="Directory containing .tap records")
    bundle_parser.add_argument(
        "--output",
        default="magtape-session.zip",
        help="Destination zip archive (default: magtape-session.zip)",
    )
    bundle_parser.add_argument(
        "--latest",
        action="store_true",
        help="Only include the most recent session (records with matching timestamps)",
    )
    bundle_parser.set_defaults(func=command_bundle)

    return parser


def main(argv: List[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
