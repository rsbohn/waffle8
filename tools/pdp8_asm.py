#!/usr/bin/env python3
"""
Lightweight PDP-8 assembler for a constrained PAL-like syntax.

Supported features:
  * Origins via `*<octal>`
  * Labels in the form `LABEL,`
  * Memory-reference ops: AND, TAD, ISZ, DCA, JMS, JMP (with optional `I`)
  * IOT with explicit numeric operand
  * IOT mnemonics: SKON (skip if interrupt on)
  * Group 1 operate micro-ops (CLA, CLL, CMA, CML, RAR, RAL, RTR, RTL, BSW, IAC)
  * Group 2 operate micro-ops (SMA, SZA, SNL, SPA, SNA, SZL, CLA, OSR, HLT, ION, IOFF)
  * Interrupt control: ION (enable), IOFF (disable), SKON (skip if on)
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
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

# --- Temporary pseudo-opcode table for session use ---
# This table is intended for temporary pseudo-ops, e.g. for assembling tc08.pa.
# It is kept separate from the main opcode tables and can be extended or cleared as needed.
PSEUDO_OPS: Dict[str, int] = {}


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
    "SMA": 0o0100,
    "SZA": 0o0040,
    "SNL": 0o0020,
    # Reverse-sense variants include the SENSE bit (0o0010).  Presenting
    # them here simplifies lookups if the assembler or callers use these
    # mnemonics directly.
    "SPA": 0o0110,  # SMA | 0o0010
    "SNA": 0o0050,  # SZA | 0o0010
    "SZL": 0o0030,  # SNL | 0o0010
    "CLA": 0o0200,
    "OSR": 0o0004,
    "HLT": 0o0002,
    "ION": 0o0001,  # Enable interrupts (Group 2 operate with bit 0 set)
    "IOFF": 0o0000, # Disable interrupts (Group 2 operate with no bits set)
}

SENSE_BIT = 0o0010  # Complements skip sense in group 2 (producing e.g. SNA/SZL/SPA)

IOT_MNEMONICS = {
    "SKON": 0o6002,  # Skip if Interrupt ON (device 00, function 2)
}


@dataclass
class Statement:
    kind: str
    address: int
    args: Tuple
    line_no: int
    text: str
    raw: str


class PDP8Assembler:
    def __init__(self, lines: Sequence[str]):
        self.lines = list(lines)
        self.symbols: Dict[str, int] = {}
        self.statements: List[Statement] = []
        self.origins: List[int] = []

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
        pseudo_op_pattern = re.compile(r"^([A-Za-z0-9_]+)\s*=\s*([0-7]+)")
        location = 0
        label_pattern = re.compile(r"\s*([A-Za-z0-9_]+),\s*(.*)")

        for line_no, raw in enumerate(self.lines, start=1):
            if '/#show-table' in raw:
                print("Pseudo-opcode table:")
                if not PSEUDO_OPS:
                    print("  (empty)")
                else:
                    for k, v in PSEUDO_OPS.items():
                        print(f"  {k} = {v:04o}")
            stripped = self._strip_comment(raw).strip()
            if not stripped:
                continue

            # Pseudo-op directive: NAME = VALUE (octal)
            pseudo_match = pseudo_op_pattern.match(stripped)
            if pseudo_match:
                name, value = pseudo_match.group(1).upper(), int(pseudo_match.group(2), 8)
                PSEUDO_OPS[name] = value
                continue

            if stripped.startswith("*"):
                try:
                    location = int(stripped[1:], 8)
                except ValueError as exc:
                    raise AsmError(f"Invalid origin directive: {stripped}", line_no) from exc
                if not self.origins or self.origins[-1] != location:
                    self.origins.append(location)
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
                if not self.origins:
                    self.origins.append(location)
                # DOT: emit current address as a word
                if part == ".":
                    self.statements.append(
                        Statement("emit_addr", location, (location,), line_no, part, raw.rstrip("\n"))
                    )
                    location += 1
                    continue
                if part.startswith('"') and part.endswith('"') and len(part) >= 2:
                    value = ord(part[1:-1]) & 0x7F
                    self.statements.append(
                        Statement("data", location, (value,), line_no, part, raw.rstrip("\n"))
                    )
                    location += 1
                    continue

                tokens = part.split()
                if not tokens:
                    continue
                upper_tokens = [tok.upper() for tok in tokens]
                op = upper_tokens[0]

                # Check pseudo-opcode table first (temporary session ops)
                if op in PSEUDO_OPS:
                    value = PSEUDO_OPS[op]
                    self.statements.append(
                        Statement("data", location, (value,), line_no, part, raw.rstrip("\n"))
                    )
                    location += 1
                    continue

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
                            raw.rstrip("\n"),
                        )
                    )
                    location += 1
                    continue

                if op == "IOT":
                    if len(tokens) != 2:
                        raise AsmError("IOT requires a single numeric operand", line_no, part)
                    self.statements.append(
                        Statement("iot", location, (tokens[1],), line_no, part, raw.rstrip("\n"))
                    )
                    location += 1
                    continue

                if op in IOT_MNEMONICS:
                    # IOT mnemonic like SKON - assemble as a direct IOT value
                    # Format as octal string to preserve the intended value
                    self.statements.append(
                        Statement("iot", location, (f"0o{IOT_MNEMONICS[op]:o}",), line_no, part, raw.rstrip("\n"))
                    )
                    location += 1
                    continue

                if op in GROUP1_BITS or op in GROUP2_BITS or op in {"SNA", "SPA", "SZL"}:
                    group_tokens = upper_tokens
                    self.statements.append(
                        Statement(
                            "operate",
                            location,
                            tuple(group_tokens),
                            line_no,
                            part,
                            raw.rstrip("\n"),
                        )
                    )
                    location += 1
                    continue

                # Plain numeric literal?
                try:
                    value = self._parse_number(tokens[0])
                except ValueError:
                    symbol = tokens[0]
                    self.statements.append(
                        Statement("data_symbol", location, (symbol,), line_no, part, raw.rstrip("\n"))
                    )
                else:
                    self.statements.append(
                        Statement("data", location, (value,), line_no, part, raw.rstrip("\n"))
                    )
                location += 1

    def _resolve_symbol(self, token: str, line_no: int, text: str, current_address: Optional[int] = None) -> int:
        # Support relative addressing with .+N or .-N (octal)
        if token.startswith(".") and current_address is not None:
            relative_match = re.fullmatch(r"\.(?:([+-])([0-7]+))?", token.strip())
            if relative_match:
                sign, digits = relative_match.groups()
                offset = int(digits, 8) if digits is not None else 0
                if sign == "-":
                    offset = -offset
                return (current_address + offset) & 0x0FFF

        # Support label arithmetic: LABEL+1, LABEL-1, LABEL+N, LABEL-N (N octal or decimal)
        label_arith_match = re.fullmatch(r"([A-Za-z0-9_]+)\s*([+-])\s*([0-9]+)", token.strip())
        if label_arith_match:
            label, op, offset_str = label_arith_match.groups()
            label = label.upper()
            if label not in self.symbols:
                raise AsmError(f"Unknown symbol '{label}'", line_no, text)
            base_addr = self.symbols[label]
            # Try octal first, then decimal fallback
            try:
                offset = int(offset_str, 8)
            except ValueError:
                offset = int(offset_str, 10)
            if op == "-":
                offset = -offset
            return (base_addr + offset) & 0x0FFF

        try:
            return self._parse_number(token)
        except ValueError:
            # Check for auto-pointer syntax &SYMBOL
            if token.startswith('&'):
                symbol_name = token[1:].upper()
                if symbol_name not in self.symbols:
                    raise AsmError(f"Unknown symbol '{token[1:]}'", line_no, text)
                return self.symbols[symbol_name]  # Return the address of the symbol

            name = token.upper()
            if name not in self.symbols:
                raise AsmError(f"Unknown symbol '{token}'", line_no, text)
            return self.symbols[name]

    def _assemble_statement(self, stmt: Statement) -> int:
        if stmt.kind == "data":
            return stmt.args[0] & 0x0FFF

        if stmt.kind == "data_symbol":
            symbol_token = stmt.args[0]
            value = self._resolve_symbol(symbol_token, stmt.line_no, stmt.text, stmt.address)
            return value & 0x0FFF

        if stmt.kind == "iot":
            value = self._resolve_symbol(stmt.args[0], stmt.line_no, stmt.text, stmt.address)
            return value & 0x0FFF

        if stmt.kind == "mem":
            op, indirect, operand_token = stmt.args
            opcode = MEMREF_OPS[op]
            operand_addr = self._resolve_symbol(operand_token, stmt.line_no, stmt.text, stmt.address)

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
            return word & 0x0FFF

        if stmt.kind == "operate":
            tokens = list(stmt.args)
            group1_possible = all(tok in GROUP1_BITS for tok in tokens)
            group2_candidate = any(tok in GROUP2_BITS or tok in {"SNA", "SPA", "SZL"} for tok in tokens)

            if group2_candidate and not group1_possible:
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
                return word & 0x0FFF

            bits = 0
            for tok in tokens:
                if tok not in GROUP1_BITS:
                    raise AsmError(f"Unsupported group 1 op '{tok}'", stmt.line_no, stmt.text)
                bits |= GROUP1_BITS[tok]
            word = 0o7000 | bits
            return word & 0x0FFF

        # DOT: emit current address as a word
        if stmt.kind == "emit_addr":
            return stmt.address & 0x0FFF

        raise AsmError(f"Unhandled statement type '{stmt.kind}'", stmt.line_no, stmt.text)

    def second_pass(self) -> Dict[int, int]:
        memory: Dict[int, int] = {}
        for stmt in self.statements:
            word = self._assemble_statement(stmt)
            memory[stmt.address] = word & 0x0FFF

        return memory

    def assemble_listing(self) -> Tuple[Dict[int, int], List[Tuple[Statement, Optional[int], Optional[AsmError]]], List[AsmError]]:
        memory: Dict[int, int] = {}
        rows: List[Tuple[Statement, Optional[int], Optional[AsmError]]] = []
        errors: List[AsmError] = []
        for stmt in self.statements:
            try:
                word = self._assemble_statement(stmt)
            except AsmError as exc:
                errors.append(exc)
                rows.append((stmt, None, exc))
                continue
            word &= 0x0FFF
            memory[stmt.address] = word
            rows.append((stmt, word, None))
        return memory, rows, errors


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


def _write_srec(assembler: PDP8Assembler, memory: Dict[int, int], output: Path) -> None:
    if not memory:
        raise AsmError("No output generated; empty program?")
    start_addr = assembler.symbols.get("START", min(memory))
    records = words_to_srec(memory, start_addr)
    try:
        output.write_text("\n".join(records) + "\n")
    except OSError as exc:
        raise AsmError(f"Unable to write {output}: {exc}")


def render_listing(
    source: Path,
    assembler: PDP8Assembler,
    rows: Sequence[Tuple[Statement, Optional[int], Optional[AsmError]]],
    errors: Sequence[AsmError],
) -> str:
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    origins = assembler.origins if assembler.origins else [0]
    origin_text = ", ".join(f"{origin:04o}" for origin in origins)

    header = [
        "PDP-8 Assembly Listing",
        f"Source: {source}",
        f"Assembled: {timestamp}",
        f"Origins: {origin_text}",
        "",
        "ADDR  WORD  SYMBOL/OPCODE      ; SOURCE",
        "----  ----  ------------------  ----------------------------------------",
    ]

    body: List[str] = []
    for stmt, word, error in rows:
        word_field = f"{word:04o}" if word is not None else "????"
        if stmt.kind == "mem":
            symbol_field = stmt.args[0]
        elif stmt.kind == "operate":
            symbol_field = stmt.args[0] if stmt.args else ""
        elif stmt.kind == "iot":
            symbol_field = "IOT"
        elif stmt.kind == "data_symbol":
            symbol_field = stmt.args[0].upper()
        elif stmt.kind == "data":
            symbol_field = stmt.text.strip() or f"{stmt.args[0]:04o}"
        elif stmt.kind == "emit_addr":
            symbol_field = ". (emit addr)"
        else:
            symbol_field = stmt.text.strip()
        symbol_field = symbol_field[:18]
        source_text = stmt.raw.rstrip() if stmt.raw else stmt.text
        line = f"{stmt.address:04o}  {word_field:>4}  {symbol_field:<18}  ; {source_text.rstrip()}"
        if error is not None:
            line += f"  <<< ERROR: {error}"
        body.append(line)

    totals_line = f"Totals: {sum(1 for _, word, _ in rows if word is not None)} words, {len(errors)} errors"
    footer: List[str] = ["", totals_line]
    if errors:
        footer.append("Errors:")
        for err in errors:
            footer.append(f"  - {err}")

    return "\n".join(header + body + footer)


def assemble(path: Path, output: Path) -> None:
    try:
        lines = path.read_text().splitlines()
    except OSError as exc:
        raise AsmError(f"Unable to read {path}: {exc}")

    assembler = PDP8Assembler(lines)
    assembler.first_pass()
    memory = assembler.second_pass()
    _write_srec(assembler, memory, output)


def main(argv: Sequence[str]) -> int:

    parser = argparse.ArgumentParser(description="Assemble PDP-8 PAL-style source to S-records.")
    parser.add_argument("source", type=Path, help="Input assembly file")
    parser.add_argument("output", type=Path, nargs="?", help="Output S-record file")
    parser.add_argument("-o", "--output", dest="output_path", type=Path, help="Explicit S-record output path")
    parser.add_argument("--list", action="store_true", help="Emit a human-readable listing to STDOUT")
    parser.add_argument("--list-only", action="store_true", help="Generate listing without writing S-records")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print pseudo-opcode table and extra info")
    args = parser.parse_args(argv)
    if args.verbose:
        print("Pseudo-opcode table:")
        if not PSEUDO_OPS:
            print("  (empty)")
        else:
            for k, v in PSEUDO_OPS.items():
                print(f"  {k} = {v:04o}")

    if args.list_only and not args.list:
        parser.error("--list-only requires --list")

    # The positional 'output' and '--output' are mutually exclusive in practice.
    # No need to check for both being set.

    output_path: Optional[Path] = args.output_path or args.output

    if args.list:
        try:
            lines = args.source.read_text().splitlines()
        except OSError as exc:
            print(f"Unable to read {args.source}: {exc}", file=sys.stderr)
            return 1

        assembler = PDP8Assembler(lines)
        listing_rows: List[Tuple[Statement, Optional[int], Optional[AsmError]]] = []
        errors: List[AsmError] = []
        try:
            assembler.first_pass()
        except AsmError as exc:
            errors.append(exc)

        memory: Dict[int, int] = {}
        if not errors:
            memory, listing_rows, pass_errors = assembler.assemble_listing()
            errors.extend(pass_errors)

        listing_text = render_listing(args.source, assembler, listing_rows, errors)
        print(listing_text)

        if errors:
            return 1

        if args.list_only:
            return 0

        if output_path is None:
            if args.source.suffix:
                output_path = args.source.with_suffix(".srec")
            else:
                output_path = Path(str(args.source) + ".srec")

        try:
            _write_srec(assembler, memory, output_path)
        except AsmError as exc:
            print(exc, file=sys.stderr)
            return 1
        except OSError as exc:
            print(f"Unable to write {output_path}: {exc}", file=sys.stderr)
            return 1
        return 0

    if output_path is None:
        parser.error("Output path required unless --list is specified")

    try:
        assemble(args.source, output_path)
    except AsmError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
