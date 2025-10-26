#!/usr/bin/env python3
"""Utility for inspecting PDP-8 magtape record files."""

from __future__ import annotations

import argparse
import datetime as _dt
import sys
import textwrap
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Sequence

SENTINEL_WORD = 0xFFFF
WORD_MASK = 0x0FFF
SIXBIT_MASK = 0x3F


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


def sixbit_to_char(value: int) -> str:
    lookup = {
        0: " ",
        30: "\r",
        31: "\n",
        42: "!",
        43: ",",
        44: "-",
        45: ".",
        46: "'",
        47: ":",
        48: ";",
        49: "?",
    }
    if 1 <= value <= 26:
        return chr(value + 64)
    if 32 <= value <= 41:
        return chr(value - 32 + ord("0"))
    return lookup.get(value, "?")


def decode_sixbit_word(word: int) -> tuple[str, str]:
    high = (word >> 6) & SIXBIT_MASK
    low = word & SIXBIT_MASK
    return sixbit_to_char(high), sixbit_to_char(low)


def decode_header_words(words: Sequence[int]) -> tuple[str, str]:
    if len(words) < 6:
        raise ValueError("Header requires at least six words")
    label_chars: List[str] = []
    format_chars: List[str] = []
    for idx in range(3):
        label_chars.extend(decode_sixbit_word(words[idx]))
    for idx in range(3, 6):
        format_chars.extend(decode_sixbit_word(words[idx]))
    return "".join(label_chars).rstrip(), "".join(format_chars).rstrip()


def decode_sixbit_payload(words: Sequence[int], start_index: int = 6) -> tuple[List[str], str]:
    rendered: List[str] = []
    chars: List[str] = []
    for idx, word in enumerate(words, start=start_index):
        high, low = decode_sixbit_word(word)
        pair = f"{(high + low)!r}"
        rendered.append(f"{idx:04}: {pair}")
        chars.extend((high, low))
    return rendered, "".join(chars)


def decode_ascii_payload(words: Sequence[int], start_index: int = 6) -> tuple[List[str], str]:
    rendered: List[str] = []
    chars: List[str] = []
    for idx, word in enumerate(words, start=start_index):
        char = chr(word & 0xFF)
        rendered.append(f"{idx:04}: {char!r}")
        chars.append(char)
    return rendered, "".join(chars)


def print_raw_words(words: Sequence[int], per_line: int = 8) -> None:
    for start in range(0, len(words), per_line):
        chunk = words[start : start + per_line]
        octal_words = " ".join(f"{word:04o}" for word in chunk)
        print(f"{start:04}: {octal_words}")


def command_inspect(args: argparse.Namespace) -> int:
    record = read_record(Path(args.record))
    if not args.raw and not args.head and not args.decode:
        print("Nothing to do: specify --raw, --head, and/or --decode.", file=sys.stderr)
        return 1

    if record.partial:
        print("Warning: record is partial; data may be truncated.", file=sys.stderr)

    header: tuple[str, str] | None = None
    if args.head or args.decode:
        try:
            header = decode_header_words(record.words)
        except ValueError as exc:
            print(f"Unable to decode header: {exc}", file=sys.stderr)
            header = None

    header_printed = False
    if args.head and header:
        label, data_format = header
        print("Header:")
        print(f"  Label     : {label}")
        print(f"  Data format: {data_format}")
        header_printed = True

    if args.decode:
        if not header:
            print("Unable to decode payload without header information.", file=sys.stderr)
        else:
            label, data_format = header
            if not header_printed:
                print("Header:")
                print(f"  Label     : {label}")
                print(f"  Data format: {data_format}")
                header_printed = True
            payload = record.words[6:]
            format_key = data_format.strip().upper()
            print(f"Decoded data ({format_key or 'UNKNOWN'}):")
            if not payload:
                print("  (no payload words)")
            elif format_key == "SIXBIT":
                rendered, text = decode_sixbit_payload(payload)
                for line in rendered:
                    print(line)
                print(f"Combined: {text!r}")
            elif format_key == "ASCII":
                rendered, text = decode_ascii_payload(payload)
                for line in rendered:
                    print(line)
                print(f"Combined: {text!r}")
            else:
                print(f"  Unsupported data format '{data_format}'.")

    if args.raw:
        print("Words (octal):")
        print_raw_words(record.words)

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

    inspect_parser = subparsers.add_parser("inspect", help="Inspect record contents")
    inspect_parser.add_argument("record", help="Path to a .tap record file")
    inspect_parser.add_argument(
        "--raw",
        action="store_true",
        help="Print all 12-bit words in octal (eight per line)",
    )
    inspect_parser.add_argument(
        "--head",
        action="store_true",
        help="Decode and print the six-word magtape header (label + data format)",
    )
    inspect_parser.add_argument(
        "--decode",
        action="store_true",
        help="Decode payload according to the header data format",
    )
    inspect_parser.set_defaults(func=command_inspect)

    return parser


def main(argv: List[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
