#!/usr/bin/env python3
"""
Lightweight PDP-8 assembler for a constrained PAL-like syntax.

Supported features:
  * Origins via `*<octal>`
  * Labels in the form `LABEL,`
  * Memory-reference ops: AND, TAD, ISZ, DCA, JMS, JMP (with optional `I`)
  * IOT with explicit numeric operand
  * Group 1 operate micro-ops (CLA, CLL, CMA, CML, RAR, RAL, RTR, RTL, BSW, IAC)
  * Group 2 operate micro-ops (SMA, SZA, SNL, SPA, SNA, SZL, CLA, OSR, HLT)
  * Data words specified as octal literals or quoted characters
  * Multiple statements per line separated by semicolons

The assembler emits Motorola S-records (S1) with byte addresses storing
each 12-bit word as two bytes: low byte first, high nibble in the next byte.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


class AsmError(Exception):
    """Assembly error with optional line context."""

    def __init__(self, message: str, line_no: Optional[int] = None, text: Optional[str] = None):
        prefix = f"Line {line_no}: " if line_no is not None else ""
        if text:
            message = f"{message} [source: {text}]"
        super().__init__(prefix + message)


MEMREF_OPS = {
    "AND": 0o0000,
    "TAD": 0o1000,
    "ISZ": 0o2000,
    "DCA": 0o3000,
    "JMS": 0o4000,
    "JMP": 0o5000,
}

GROUP1_BITS = {
    "CLA": 0o0200,
    "CLL": 0o0100,
    "CMA": 0o0040,
    "CML": 0o0020,
    "RAR": 0o0010,
    "RAL": 0o0004,
    "RTR": 0o0012,
    "RTL": 0o0006,
    "BSW": 0o0002,
    "IAC": 0o0001,
}

GROUP2_BITS = {
    "SMA": 0o0020,
    "SZA": 0o0040,
    "SNL": 0o0100,
    "CLA": 0o0200,
    "OSR": 0o0004,
    "HLT": 0o0002,
}

SENSE_BIT = 0o0010  # Complements skip sense in group 2 (producing e.g. SNA/SZL/SPA)


@dataclass
class Statement:
    kind: str
    address: int
    args: Tuple
    line_no: int
    text: str


class PDP8Assembler:
    def __init__(self, lines: Sequence[str]):
        self.lines = list(lines)
        self.symbols: Dict[str, int] = {}
        self.statements: List[Statement] = []

    @staticmethod
    def _strip_comment(text: str) -> str:
        if "/" in text:
            idx = text.index("/")
            return text[:idx]
        return text

    @staticmethod
    def _parse_number(token: str) -> int:
        token = token.strip()
        if token.startswith("0x") or token.startswith("0X"):
            return int(token[2:], 16) & 0x0FFF
        if token.startswith("#"):
            return int(token[1:], 10) & 0x0FFF
        if token.startswith("-"):
            value = -int(token[1:], 8)
            return value & 0x0FFF
        return int(token, 8) & 0x0FFF

    def first_pass(self) -> None:
        location = 0
        label_pattern = re.compile(r"\s*([A-Za-z0-9_]+),\s*(.*)")

        for line_no, raw in enumerate(self.lines, start=1):
            stripped = self._strip_comment(raw).strip()
            if not stripped:
                continue

            if stripped.startswith("*"):
                try:
                    location = int(stripped[1:], 8)
                except ValueError as exc:
                    raise AsmError(f"Invalid origin directive: {stripped}", line_no) from exc
                continue

            label = None
            rest = stripped
            match = label_pattern.match(stripped)
            if match:
                label, rest = match.group(1).upper(), match.group(2)
                if label in self.symbols:
                    raise AsmError(f"Duplicate label '{label}'", line_no)
                self.symbols[label] = location

            rest = rest.strip()
            if not rest:
                continue

            if rest == "$":
                break

            parts = [part.strip() for part in rest.split(";") if part.strip()]
            for part in parts:
                if part.startswith('"') and part.endswith('"') and len(part) >= 2:
                    value = ord(part[1:-1]) & 0x7F
                    self.statements.append(
                        Statement("data", location, (value,), line_no, part)
                    )
                    location += 1
                    continue

                tokens = part.split()
                if not tokens:
                    continue
                upper_tokens = [tok.upper() for tok in tokens]
                op = upper_tokens[0]

                if op in MEMREF_OPS:
                    indirect = False
                    operand_token: Optional[str] = None
                    if len(upper_tokens) >= 2 and upper_tokens[1] == "I":
                        indirect = True
                        operand_token = tokens[2] if len(tokens) >= 3 else None
                    else:
                        operand_token = tokens[1] if len(tokens) >= 2 else None
                    if operand_token is None:
                        raise AsmError(f"Missing operand for {op}", line_no, part)
                    self.statements.append(
                        Statement(
                            "mem",
                            location,
                            (op, indirect, operand_token),
                            line_no,
                            part,
                        )
                    )
                    location += 1
                    continue

                if op == "IOT":
                    if len(tokens) != 2:
                        raise AsmError("IOT requires a single numeric operand", line_no, part)
                    self.statements.append(
                        Statement("iot", location, (tokens[1],), line_no, part)
                    )
                    location += 1
                    continue

                if op in GROUP1_BITS or op in GROUP2_BITS or op in {"SNA", "SPA", "SZL"}:
                    group_tokens = upper_tokens
                    self.statements.append(
                        Statement("operate", location, tuple(group_tokens), line_no, part)
                    )
                    location += 1
                    continue

                # Plain numeric literal?
                try:
                    value = self._parse_number(tokens[0])
                except ValueError:
                    symbol = tokens[0]
                    self.statements.append(
                        Statement("data_symbol", location, (symbol,), line_no, part)
                    )
                else:
                    self.statements.append(
                        Statement("data", location, (value,), line_no, part)
                    )
                location += 1

    def _resolve_symbol(self, token: str, line_no: int, text: str) -> int:
        try:
            return self._parse_number(token)
        except ValueError:
            name = token.upper()
            if name not in self.symbols:
                raise AsmError(f"Unknown symbol '{token}'", line_no, text)
            return self.symbols[name]

    def second_pass(self) -> Dict[int, int]:
        memory: Dict[int, int] = {}
        for stmt in self.statements:
            if stmt.kind == "data":
                value = stmt.args[0] & 0x0FFF
                memory[stmt.address] = value
                continue

            if stmt.kind == "data_symbol":
                symbol_token = stmt.args[0]
                value = self._resolve_symbol(symbol_token, stmt.line_no, stmt.text) & 0x0FFF
                memory[stmt.address] = value
                continue

            if stmt.kind == "iot":
                value = self._resolve_symbol(stmt.args[0], stmt.line_no, stmt.text)
                memory[stmt.address] = value & 0x0FFF
                continue

            if stmt.kind == "mem":
                op, indirect, operand_token = stmt.args
                opcode = MEMREF_OPS[op]
                operand_addr = self._resolve_symbol(operand_token, stmt.line_no, stmt.text)

                current_page = stmt.address // 0o200
                if operand_addr < 0o200:
                    page_bit = 0
                    offset = operand_addr
                else:
                    operand_page = operand_addr // 0o200
                    if operand_page != current_page:
                        raise AsmError(
                            f"Operand '{operand_token}' out of range for direct addressing",
                            stmt.line_no,
                            stmt.text,
                        )
                    page_bit = 1
                    offset = operand_addr & 0o177
                word = opcode | (0o400 if indirect else 0) | (page_bit << 7) | offset
                memory[stmt.address] = word & 0x0FFF
                continue

            if stmt.kind == "operate":
                tokens = list(stmt.args)
                group1_possible = all(tok in GROUP1_BITS for tok in tokens)
                group2_candidate = any(tok in GROUP2_BITS or tok in {"SNA", "SPA", "SZL"} for tok in tokens)

                if group2_candidate and not group1_possible:
                    # Group 2
                    bits = 0
                    for tok in tokens:
                        if tok == "SNA":
                            bits |= GROUP2_BITS["SZA"] | SENSE_BIT
                        elif tok == "SPA":
                            bits |= GROUP2_BITS["SMA"] | SENSE_BIT
                        elif tok == "SZL":
                            bits |= GROUP2_BITS["SNL"] | SENSE_BIT
                        elif tok == "CLA":
                            bits |= GROUP2_BITS["CLA"]
                        elif tok == "OSR":
                            bits |= GROUP2_BITS["OSR"]
                        elif tok == "HLT":
                            bits |= GROUP2_BITS["HLT"]
                        elif tok in GROUP2_BITS:
                            bits |= GROUP2_BITS[tok]
                        elif tok == "SNS":
                            bits |= SENSE_BIT
                        else:
                            raise AsmError(f"Unsupported group 2 op '{tok}'", stmt.line_no, stmt.text)
                    word = 0o7400 | bits
                    memory[stmt.address] = word & 0x0FFF
                    continue

                # Group 1 (default)
                bits = 0
                for tok in tokens:
                    if tok not in GROUP1_BITS:
                        raise AsmError(f"Unsupported group 1 op '{tok}'", stmt.line_no, stmt.text)
                    bits |= GROUP1_BITS[tok]
                word = 0o7000 | bits
                memory[stmt.address] = word & 0x0FFF
                continue

            raise AsmError(f"Unhandled statement type '{stmt.kind}'", stmt.line_no, stmt.text)

        return memory


def words_to_srec(memory: Dict[int, int], start_address: int) -> List[str]:
    if not memory:
        return []

    byte_map: Dict[int, int] = {}
    for addr, word in memory.items():
        word &= 0x0FFF
        byte_addr = addr * 2
        byte_map[byte_addr] = word & 0xFF
        byte_map[byte_addr + 1] = (word >> 8) & 0x0F

    records: List[str] = []
    max_bytes_per_record = 32
    sorted_bytes = sorted(byte_map.items())

    current_start = None
    current_bytes: List[int] = []
    previous_addr = None

    def emit():
        nonlocal current_start, current_bytes
        if current_start is None or not current_bytes:
            return
        address = current_start
        count = len(current_bytes) + 3  # count + addr(2) + checksum
        record_bytes = [count, (address >> 8) & 0xFF, address & 0xFF] + current_bytes
        checksum = (~sum(record_bytes) & 0xFF)
        data_field = "".join(f"{b:02X}" for b in record_bytes[3:])
        record = f"S1{count:02X}{address:04X}{data_field}{checksum:02X}"
        records.append(record)
        current_start = None
        current_bytes = []

    for addr, value in sorted_bytes:
        if current_start is None:
            current_start = addr
        contiguous = previous_addr is not None and addr == previous_addr + 1
        exceeds = len(current_bytes) >= max_bytes_per_record
        if not contiguous or exceeds:
            emit()
            current_start = addr
        current_bytes.append(value & 0xFF)
        previous_addr = addr
    emit()

    start_byte_addr = (start_address & 0x0FFF) * 2
    count = 3
    checksum = (~(count + (start_byte_addr >> 8) + (start_byte_addr & 0xFF)) & 0xFF)
    records.append(f"S903{start_byte_addr:04X}{checksum:02X}")
    return records


def assemble(path: Path, output: Path) -> None:
    try:
        lines = path.read_text().splitlines()
    except OSError as exc:
        raise AsmError(f"Unable to read {path}: {exc}")

    assembler = PDP8Assembler(lines)
    assembler.first_pass()
    memory = assembler.second_pass()
    if not memory:
        raise AsmError("No output generated; empty program?")
    start_addr = min(memory)
    records = words_to_srec(memory, start_addr)
    try:
        output.write_text("\n".join(records) + "\n")
    except OSError as exc:
        raise AsmError(f"Unable to write {output}: {exc}")


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description="Assemble PDP-8 PAL-style source to S-records.")
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args(argv)

    try:
        assemble(args.source, args.output)
    except AsmError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
