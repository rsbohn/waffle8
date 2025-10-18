"""
Pure Python PDP-8 emulator with a KL8E console peripheral.

This module mirrors the behaviour of the C implementation under ``src/emulator``.
It models a 12-bit PDP-8 CPU with 4K words of memory and supports the KL8E
keyboard/teleprinter device. The entry point accepts Motorola S-record images
so the Python model can be benchmarked against the native emulator.
"""

from __future__ import annotations

import argparse
import sys
from collections import deque
from pathlib import Path
from typing import Callable, Deque, Iterable, List, Optional, Sequence, Tuple

WORD_MASK = 0x0FFF
LINK_MASK = 0x01
OPCODE_MASK = 0x0E00
INDIRECT_MASK = 0x0100
PAGE_MASK = 0x0080
OFFSET_MASK = 0x007F

AUTO_INCREMENT_START = 0o0010
AUTO_INCREMENT_END = 0o0017
IOT_DEVICE_COUNT = 64

KL8E_KEYBOARD_DEVICE_CODE = 0o003
KL8E_TELEPRINTER_DEVICE_CODE = 0o004

KL8E_KEYBOARD_BIT_SKIP = 0x1
KL8E_KEYBOARD_BIT_CLEAR = 0x2
KL8E_KEYBOARD_BIT_READ = 0x4

KL8E_TELEPRINTER_BIT_SKIP = 0x1
KL8E_TELEPRINTER_BIT_CLEAR = 0x2
KL8E_TELEPRINTER_BIT_LOAD = 0x4

DEFAULT_MEMORY_WORDS = 4096
RUN_BLOCK_CYCLES = 10_000

IOTHandler = Callable[["PDP8", int, Optional[object]], None]


def _mask_word(value: int) -> int:
    return value & WORD_MASK


def _normalise_address(memory_words: int, address: int) -> int:
    if memory_words == 0:
        return 0
    return address % memory_words


class PDP8:
    def __init__(self) -> None:
        self.memory_words = DEFAULT_MEMORY_WORDS
        self.memory: List[int] = [0] * self.memory_words
        self.pc = 0
        self.ac = 0
        self.link = 0
        self.switch_register = 0
        self.halted = False
        self.skip_pending = False
        self._iot_handlers: List[Optional[IOTHandler]] = [None] * IOT_DEVICE_COUNT
        self._iot_contexts: List[Optional[object]] = [None] * IOT_DEVICE_COUNT

    # Core register helpers -------------------------------------------------
    def reset(self, clear_memory: bool = True) -> None:
        self.pc = 0
        self.ac = 0
        self.link = 0
        self.switch_register = 0
        self.halted = False
        self.skip_pending = False
        if clear_memory:
            for idx in range(self.memory_words):
                self.memory[idx] = 0

    def clear_halt(self) -> None:
        self.halted = False

    def is_halted(self) -> bool:
        return self.halted

    def set_pc(self, value: int) -> None:
        self.pc = value & WORD_MASK

    def get_pc(self) -> int:
        return self.pc

    def set_ac(self, value: int) -> None:
        self.ac = _mask_word(value)

    def get_ac(self) -> int:
        return self.ac

    def set_link(self, value: int) -> None:
        self.link = value & LINK_MASK

    def get_link(self) -> int:
        return self.link

    def set_switch_register(self, value: int) -> None:
        self.switch_register = _mask_word(value)

    def get_switch_register(self) -> int:
        return self.switch_register

    # Memory helpers --------------------------------------------------------
    def read_mem(self, address: int) -> int:
        idx = address & WORD_MASK
        return self.memory[idx] & WORD_MASK

    def write_mem(self, address: int, value: int) -> None:
        idx = address & WORD_MASK
        self.memory[idx] = _mask_word(value)

    def load_words(self, words: Iterable[Tuple[int, int]]) -> None:
        for address, value in words:
            self.write_mem(address, value)

    # IOT helpers -----------------------------------------------------------
    def register_iot(self, device_code: int, handler: Optional[IOTHandler], context: Optional[object]) -> None:
        if not (0 <= device_code < IOT_DEVICE_COUNT):
            raise ValueError("device_code out of range")
        self._iot_handlers[device_code] = handler
        self._iot_contexts[device_code] = context

    def request_skip(self) -> None:
        self.skip_pending = True

    # Execution -------------------------------------------------------------
    def step(self) -> bool:
        if self.halted or self.memory_words == 0:
            return False

        instruction = self.read_mem(self.pc)
        self.pc = (self.pc + 1) & WORD_MASK
        opcode = instruction & OPCODE_MASK

        if opcode in (0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0A00):
            self._execute_memory_reference(instruction)
        elif opcode == 0x0C00:
            self._execute_iot(instruction)
        elif opcode == 0x0E00:
            self._execute_operate(instruction)

        if self.skip_pending:
            self.pc = (self.pc + 1) & WORD_MASK
            self.skip_pending = False

        return True

    def run(self, max_cycles: int) -> int:
        executed = 0
        while executed < max_cycles:
            if self.halted:
                break
            if not self.step():
                break
            executed += 1
        return executed

    # Instruction groups ----------------------------------------------------
    def _fetch_effective_address(self, instruction: int) -> int:
        if instruction & PAGE_MASK:
            page_base = self.pc & ~OFFSET_MASK
        else:
            page_base = 0
        offset = instruction & OFFSET_MASK
        address = (page_base | offset) & WORD_MASK

        if instruction & INDIRECT_MASK:
            if AUTO_INCREMENT_START <= address <= AUTO_INCREMENT_END:
                self.memory[address] = _mask_word(self.memory[address] + 1)
            address = self.memory[address] & WORD_MASK

        return address

    def _execute_memory_reference(self, instruction: int) -> None:
        opcode = instruction & OPCODE_MASK
        address = self._fetch_effective_address(instruction)

        if opcode == 0x0000:  # AND
            self.ac = _mask_word(self.ac & self.memory[address])
        elif opcode == 0x0200:  # TAD
            value = self.memory[address]
            total = self.ac + value
            if total & 0x1000:
                self.link ^= 1
            self.ac = _mask_word(total)
        elif opcode == 0x0400:  # ISZ
            self.memory[address] = _mask_word(self.memory[address] + 1)
            if self.memory[address] == 0:
                self.skip_pending = True
        elif opcode == 0x0600:  # DCA
            self.memory[address] = self.ac
            self.ac = 0
        elif opcode == 0x0800:  # JMS
            self.memory[address] = self.pc
            self.pc = (address + 1) & WORD_MASK
        elif opcode == 0x0A00:  # JMP
            self.pc = address & WORD_MASK

    def _execute_iot(self, instruction: int) -> None:
        device = (instruction >> 3) & 0x3F
        handler = self._iot_handlers[device]
        if handler is not None:
            handler(self, instruction, self._iot_contexts[device])

    def _execute_operate(self, instruction: int) -> None:
        if (instruction & 0x0100) == 0:
            self._operate_group1(instruction)
        else:
            self._operate_group2(instruction)

    def _operate_group1(self, instruction: int) -> None:
        if instruction & 0x0080:  # CLA
            self.ac = 0
        if instruction & 0x0040:  # CLL
            self.link = 0
        if instruction & 0x0020:  # CMA
            self.ac = _mask_word(~self.ac)
        if instruction & 0x0010:  # CML
            self.link ^= 1

        rotate_twice = False
        rotate_right = False
        rotate_left = False

        if instruction & 0x0002:
            if (instruction & (0x0008 | 0x0004)) == 0:
                high = (self.ac & 0x003F) << 6
                low = (self.ac >> 6) & 0x003F
                self.ac = high | low
            else:
                rotate_twice = True
        if instruction & 0x0008:
            rotate_right = True
        if instruction & 0x0004:
            rotate_left = True
        if rotate_right and rotate_left:
            rotate_right = False
            rotate_left = False

        if rotate_right or rotate_left:
            rotations = 2 if rotate_twice else 1
            combined = ((self.link & 0x1) << 12) | self.ac
            for _ in range(rotations):
                if rotate_right:
                    carry = combined & 0x1
                    combined = (combined >> 1) | (carry << 12)
                else:
                    carry = (combined >> 12) & 0x1
                    combined = ((combined << 1) & 0x1FFF) | carry
            self.link = (combined >> 12) & 0x1
            self.ac = _mask_word(combined)

        if instruction & 0x0001:  # IAC
            combined = ((self.link & 0x1) << 12) | self.ac
            combined = (combined + 1) & 0x1FFF
            self.link = (combined >> 12) & 0x1
            self.ac = _mask_word(combined)

    def _operate_group2(self, instruction: int) -> None:
        if instruction & 0x0080:  # CLA
            self.ac = 0

        sense = (instruction & 0x0008) != 0
        conditions = (
            (instruction & 0x0040) and (self.ac & 0x0800),  # SMA
            (instruction & 0x0020) and (self.ac == 0),  # SZA
            (instruction & 0x0010) and (self.link != 0),  # SNL
        )
        any_true = any(conditions)

        if instruction & 0x0004:  # OSR
            self.ac = _mask_word(self.ac | self.switch_register)
        if instruction & 0x0002:  # HLT
            self.halted = True

        skip = not any_true if sense else any_true
        if skip:
            self.skip_pending = True


class KL8EConsole:
    def __init__(self, output_stream: Optional[object] = sys.stdout) -> None:
        self.keyboard_buffer = 0
        self.keyboard_flag = False
        self.teleprinter_flag = True
        self.pending_input: Deque[int] = deque()
        self.output_log: Deque[int] = deque()
        self.output_stream = output_stream

    def attach(self, cpu: PDP8) -> None:
        cpu.register_iot(KL8E_KEYBOARD_DEVICE_CODE, self._keyboard_iot, self)
        cpu.register_iot(KL8E_TELEPRINTER_DEVICE_CODE, self._teleprinter_iot, self)

    def queue_input(self, value: int) -> None:
        ch = value & 0x7F
        if not self.keyboard_flag:
            self.keyboard_buffer = ch
            self.keyboard_flag = True
            return
        self.pending_input.append(ch)

    def queue_input_string(self, text: str) -> None:
        for raw in text:
            byte = ord(raw) & 0x7F
            if byte == 0x0A:
                byte = 0x0D
            self.queue_input(byte)

    def input_pending(self) -> int:
        return len(self.pending_input) + (1 if self.keyboard_flag else 0)

    def output_pending(self) -> int:
        return len(self.output_log)

    def pop_output(self) -> Optional[int]:
        if not self.output_log:
            return None
        return self.output_log.popleft()

    def drain_output(self) -> str:
        chars: List[str] = []
        while self.output_log:
            chars.append(chr(self.output_log.popleft()))
        return "".join(chars)

    def _keyboard_promote_pending(self) -> None:
        if self.keyboard_flag:
            return
        if not self.pending_input:
            return
        self.keyboard_buffer = self.pending_input.popleft()
        self.keyboard_flag = True

    def _keyboard_consume_current(self) -> None:
        self.keyboard_flag = False
        self.keyboard_buffer = 0
        self._keyboard_promote_pending()

    def _record_output(self, value: int) -> None:
        ch = value & 0x7F
        self.output_log.append(ch)
        if self.output_stream is not None:
            try:
                self.output_stream.write(chr(ch))
                self.output_stream.flush()
            except Exception:
                pass

    @staticmethod
    def _keyboard_iot(cpu: PDP8, instruction: int, context: Optional[object]) -> None:
        console = context if isinstance(context, KL8EConsole) else None
        if console is None:
            return

        microcode = instruction & 0x7
        if microcode == 0:
            console._keyboard_consume_current()
            return

        if (microcode & KL8E_KEYBOARD_BIT_SKIP) and console.keyboard_flag:
            cpu.request_skip()

        clear_flag = bool(microcode & KL8E_KEYBOARD_BIT_CLEAR)
        read_buffer = bool(microcode & KL8E_KEYBOARD_BIT_READ)
        had_char = console.keyboard_flag
        current = console.keyboard_buffer

        if clear_flag:
            cpu.set_ac(0)

        if read_buffer and had_char:
            cpu.set_ac(cpu.get_ac() | current)

        if clear_flag:
            console._keyboard_consume_current()

    @staticmethod
    def _teleprinter_iot(cpu: PDP8, instruction: int, context: Optional[object]) -> None:
        console = context if isinstance(context, KL8EConsole) else None
        if console is None:
            return

        microcode = instruction & 0x7
        if (microcode & KL8E_TELEPRINTER_BIT_SKIP) and console.teleprinter_flag:
            cpu.request_skip()

        if microcode & KL8E_TELEPRINTER_BIT_CLEAR:
            console.teleprinter_flag = False

        if microcode & KL8E_TELEPRINTER_BIT_LOAD:
            ch = cpu.get_ac() & 0x7F
            console.teleprinter_flag = False
            console._record_output(ch)
            console.teleprinter_flag = True


# CLI ------------------------------------------------------------------------

def _auto_int(value: str) -> int:
    base = 10
    if value.startswith("0o"):
        base = 8
    elif value.startswith("0x"):
        base = 16
    elif value.startswith("0b"):
        base = 2
    return int(value, base)


def load_srec(path: Path) -> Tuple[List[Tuple[int, int]], Optional[int]]:
    try:
        lines = path.read_text().splitlines()
    except OSError as exc:
        raise RuntimeError(f"Unable to read {path}: {exc}") from exc

    byte_map = {}
    start_word: Optional[int] = None
    for raw in lines:
        line = raw.strip()
        if not line or not line.startswith("S"):
            continue
        record_type = line[1]
        if record_type in "123":
            try:
                count = int(line[2:4], 16)
            except ValueError as exc:
                raise RuntimeError(f"Invalid count in record {line}") from exc

            addr_len = {"1": 4, "2": 6, "3": 8}[record_type]
            addr_field = line[4 : 4 + addr_len]
            data_field = line[4 + addr_len : -2]
            try:
                base_address = int(addr_field, 16)
            except ValueError as exc:
                raise RuntimeError(f"Invalid address in record {line}") from exc

            data_bytes: List[int] = []
            for idx in range(0, len(data_field), 2):
                try:
                    data_bytes.append(int(data_field[idx : idx + 2], 16))
                except ValueError as exc:
                    raise RuntimeError(f"Invalid data byte in record {line}") from exc

            expected_payload = count - (addr_len // 2) - 1
            if expected_payload != len(data_bytes):
                raise RuntimeError(f"Count mismatch in record {line}")

            for offset, value in enumerate(data_bytes):
                byte_map[base_address + offset] = value
            continue

        if record_type in "789":
            addr_len = {"7": 8, "8": 6, "9": 4}[record_type]
            addr_field = line[4 : 4 + addr_len]
            try:
                start_byte = int(addr_field, 16)
            except ValueError as exc:
                raise RuntimeError(f"Invalid start address in record {line}") from exc
            start_word = (start_byte // 2) & WORD_MASK
            continue

    words = {}
    for byte_addr in sorted(byte_map):
        if byte_addr % 2 != 0:
            continue
        lo = byte_map.get(byte_addr)
        hi = byte_map.get(byte_addr + 1)
        if lo is None or hi is None:
            raise RuntimeError(f"Incomplete word at byte address 0x{byte_addr:04X}")
        word_addr = byte_addr // 2
        words[word_addr] = ((hi << 8) | lo) & WORD_MASK

    if not words:
        raise RuntimeError(f"No data records found in {path}")

    return sorted(words.items()), start_word


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the Python PDP-8 emulator.")
    parser.add_argument("image", type=Path, help="Motorola S-record image to load.")
    parser.add_argument(
        "--cycles",
        type=int,
        default=1_000_000,
        help="Maximum cycles to execute (default: %(default)s).",
    )
    parser.add_argument(
        "--start",
        type=_auto_int,
        default=None,
        help="Override start address (accepts 0o, 0x prefixes). Defaults to S-record START if present, otherwise the lowest word.",
    )
    parser.add_argument(
        "--input",
        default=None,
        help="String queued to the console before execution (LF converted to CR).",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress live console output; still captured for summary.",
    )
    return parser.parse_args(argv)


def run_program(
    cpu: PDP8,
    console: KL8EConsole,
    rom_words: Sequence[Tuple[int, int]],
    start_address: Optional[int],
    max_cycles: int,
) -> Tuple[int, int, bool]:
    cpu.reset(clear_memory=True)
    cpu.load_words(rom_words)

    entry = start_address if start_address is not None else rom_words[0][0]
    cpu.set_pc(entry)

    executed = 0
    while executed < max_cycles and not cpu.is_halted():
        executed += cpu.run(min(RUN_BLOCK_CYCLES, max_cycles - executed))
        if executed >= max_cycles or cpu.is_halted():
            break

    return cpu.get_pc(), cpu.get_ac(), cpu.is_halted()


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    rom_words, start_from_image = load_srec(args.image)

    output_stream = None if args.quiet else sys.stdout
    console = KL8EConsole(output_stream=output_stream)
    cpu = PDP8()
    console.attach(cpu)

    if args.input:
        console.queue_input_string(args.input)

    effective_start = args.start if args.start is not None else start_from_image
    pc, ac, halted = run_program(cpu, console, rom_words, effective_start, args.cycles)
    captured_output = console.drain_output()

    if args.quiet and captured_output:
        print(captured_output, end="")

    print()
    print("Python PDP-8 run complete.")
    print(f"  Cycles budget: {args.cycles}")
    print(f"  PC: {pc:04o}")
    print(f"  AC: {ac:04o}")
    print(f"  HALT: {'yes' if halted else 'no'}")
    if captured_output:
        print("  Console output:")
        print(captured_output)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
