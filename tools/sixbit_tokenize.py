#!/usr/bin/env python3
"""Convert ASCII words into PDP-8 SIXBIT octal tokens."""

from __future__ import annotations

import argparse
import sys
from typing import Iterable, List, Sequence

SIXBIT_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


class SixbitError(Exception):
    """Raised when a word cannot be converted into SIXBIT."""


def char_to_sixbit(value: str) -> int:
    """Return the SIXBIT value for a single uppercase character."""
    if value == " ":
        return 0
    if value in SIXBIT_ALPHABET:
        return ord(value) - ord("A") + 1
    raise SixbitError(f"Unsupported SIXBIT character: {value!r}")


def pack_sixbit_pairs(chars: Sequence[str]) -> List[int]:
    """Pack an even-length sequence of SIXBIT characters into 12-bit words."""
    if len(chars) % 2 != 0:
        raise ValueError("pack_sixbit_pairs requires an even number of characters")
    words: List[int] = []
    for idx in range(0, len(chars), 2):
        high = char_to_sixbit(chars[idx])
        low = char_to_sixbit(chars[idx + 1])
        words.append(((high << 6) | low) & 0x0FFF)
    return words


def extract_letters(word: str) -> str:
    """Return the first four ASCII letters from the word."""
    letters: List[str] = []
    for ch in word:
        if ch.isalpha() and ch.isascii():
            letters.append(ch.upper())
            if len(letters) == 4:
                break
    return "".join(letters)


def encode_word(word: str) -> tuple[str, List[int]]:
    """Encode a word into truncated letters and SIXBIT octal tokens."""
    trimmed = extract_letters(word)
    if not trimmed:
        raise SixbitError(f"Word {word!r} does not contain ASCII letters to encode.")
    padded = list(trimmed)
    if len(padded) % 2 == 1:
        padded.append(" ")
    return trimmed, pack_sixbit_pairs(padded)


def iter_words(chunks: Sequence[str]) -> Iterable[str]:
    """Yield whitespace-delimited words from CLI arguments or stdin."""
    if chunks:
        data = " ".join(chunks)
    else:
        data = sys.stdin.read()
    for word in data.split():
        yield word


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Tokenize ASCII words into SIXBIT octal pairs.",
        epilog="Input can be provided as command arguments or via stdin.",
    )
    parser.add_argument(
        "text",
        nargs="*",
        help="Input text to tokenize (split on whitespace). Reads stdin when omitted.",
    )
    return parser.parse_args(argv)


def format_octal(words: Sequence[int]) -> str:
    return " ".join(f"{word:04o}" for word in words)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    had_error = False
    for word in iter_words(args.text):
        try:
            label, values = encode_word(word)
        except SixbitError as exc:
            print(f"error: {exc}", file=sys.stderr)
            had_error = True
            continue
        print(f"{label}: {format_octal(values)}")
    return 1 if had_error else 0


if __name__ == "__main__":
    raise SystemExit(main())
