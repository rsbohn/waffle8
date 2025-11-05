#!/usr/bin/env python3
"""
Convert a plain text file into a PDP-8 magtape record (.tap).

The generated record uses the standard six-word SIXBIT header followed by an ASCII payload
stored in 12-bit words (character byte in bits 0-7). The file layout matches the format
consumed by the emulator's magtape device and inspected by tools/magtape_tool.py.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import List

WORD_MASK = 0x0FFF
SENTINEL_WORD = 0xFFFF
HEADER_FIELD_LENGTH = 6  # Label and data format header fields are six characters each
SIXBIT_MASK = 0x3F


class Txt2TapeError(Exception):
    """Raised when inputs cannot be encoded into a magtape record."""


def char_to_sixbit(value: str) -> int:
    """Convert a single character to its SIXBIT value."""
    if value == " ":
        return 0
    if "A" <= value <= "Z":
        return ord(value) - 64
    if "0" <= value <= "9":
        return ord(value) - ord("0") + 32
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
    if value in lookup:
        return lookup[value]
    raise Txt2TapeError(f"Unsupported SIXBIT character: {value!r}")


def normalize_field(text: str) -> str:
    """Uppercase, truncate, and pad a header field to exactly six characters."""
    return text.upper()[:HEADER_FIELD_LENGTH].ljust(HEADER_FIELD_LENGTH)


def encode_sixbit_field(text: str) -> List[int]:
    """Encode up to six characters of text as three SIXBIT words."""
    processed = normalize_field(text)
    codes = [char_to_sixbit(ch) for ch in processed]
    # Pair characters into 12-bit words (two SIXBIT chars per word)
    words: List[int] = []
    for idx in range(0, HEADER_FIELD_LENGTH, 2):
        high = codes[idx] & SIXBIT_MASK
        low = codes[idx + 1] & SIXBIT_MASK
        words.append((high << 6) | low)
    return words


def encode_ascii_payload(text: str) -> List[int]:
    """Encode text into 12-bit words carrying ASCII bytes in the low eight bits."""
    payload: List[int] = []
    for ch in text:
        code_point = ord(ch)
        if code_point > 0xFF:
            raise Txt2TapeError(f"Character {ch!r} (U+{code_point:04X}) exceeds 8-bit ASCII range")
        payload.append(code_point & 0xFF)
    return payload


def compute_length_word(total: int) -> int:
    """Validate that the record length fits into the 12-bit header field."""
    if total > WORD_MASK:
        raise Txt2TapeError(f"Record exceeds 12-bit length limit ({WORD_MASK} words)")
    return total


def derive_default_label(input_path: Path) -> str:
    """Create a reasonable default label from the input filename stem."""
    stem = input_path.stem.upper()
    if not stem:
        return "TXT2TP"
    return stem[:HEADER_FIELD_LENGTH] or "TXT2TP"


def write_record(path: Path, words: List[int]) -> None:
    """Write the magtape record to disk with the appropriate sentinel."""
    length_word = compute_length_word(len(words))
    with path.open("wb") as fp:
        fp.write((length_word & WORD_MASK).to_bytes(2, "little"))
        for word in words:
            fp.write(int(word & WORD_MASK).to_bytes(2, "little"))
        fp.write(SENTINEL_WORD.to_bytes(2, "little"))


def build_record_words(label: str, data_format: str, payload: List[int]) -> List[int]:
    """Construct the sequence of 12-bit words for the record."""
    header_words = encode_sixbit_field(label) + encode_sixbit_field(data_format)
    return header_words + payload


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert a text file into a PDP-8 magtape record (.tap).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("input", type=Path, help="Input text file to encode")
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        help="Destination .tap file (default: magtape/<label>.tap)",
    )
    parser.add_argument("--label", "-l", help="Six-character tape label (truncated/padded as needed)")
    parser.add_argument(
        "--format",
        "-f",
        default="ASCII",
        help="Six-character data format value stored in the header",
    )
    parser.add_argument(
        "--encoding",
        default="utf-8",
        help="Text encoding used to read the input file",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    if not args.input.exists():
        print(f"error: input file {args.input} does not exist", file=sys.stderr)
        return 1

    label = args.label or derive_default_label(args.input)
    data_format = args.format or "ASCII"
    normalized_label = normalize_field(label)
    normalized_format = normalize_field(data_format)

    try:
        text = args.input.read_text(encoding=args.encoding)
    except OSError as exc:
        print(f"error: failed to read {args.input}: {exc}", file=sys.stderr)
        return 1

    try:
        payload_words = encode_ascii_payload(text)
        record_words = build_record_words(normalized_label, normalized_format, payload_words)
        output_path = args.output
        if output_path is None:
            output_dir = Path("magtape")
            output_dir.mkdir(parents=True, exist_ok=True)
            default_name = normalized_label.strip() or "record"
            output_path = output_dir / f"{default_name}.tap"
        else:
            output_path.parent.mkdir(parents=True, exist_ok=True)

        write_record(output_path, record_words)
    except Txt2TapeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: failed to write output: {exc}", file=sys.stderr)
        return 1

    total_words = len(record_words)
    header_words = HEADER_FIELD_LENGTH  # six total header words
    payload_count = total_words - header_words
    print(f"Wrote {output_path} ({total_words} words: header={header_words}, payload={payload_count})")
    print(f"Label={normalized_label!r} DataFormat={normalized_format!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
