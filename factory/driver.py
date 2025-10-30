#!/usr/bin/env python3
"""
factory — load a PDP-8 ROM S-record, install a reset vector, and run the emulator.
"""

import argparse
import errno
import os
import select
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, IO, List, Optional, Tuple

import ctypes


RESET_VECTOR_ADDR = 0o0000
RESET_POINTER_ADDR = 0o0020
JMP_INDIRECT_20 = 0o5420  # JMP I 20, used as the reset vector
DEFAULT_MEMORY_WORDS = 4096
RUN_BLOCK_CYCLES = 10_000
KL8E_BAUD = 110
KL8E_BITS_PER_CHAR = 10
KL8E_CHAR_PERIOD = KL8E_BITS_PER_CHAR / KL8E_BAUD if KL8E_BAUD > 0 else 0.0

# Watchdog device constants (match src/emulator/watchdog.h)
PDP8_WATCHDOG_DEVICE_CODE = 0o55
PDP8_WATCHDOG_IOT_BASE = 0o6000 | ((PDP8_WATCHDOG_DEVICE_CODE & 0x3F) << 3)
PDP8_WATCHDOG_BIT_WRITE = 0x1
PDP8_WATCHDOG_BIT_READ = 0x2
PDP8_WATCHDOG_BIT_RESTART = 0x4


@dataclass
class DeviceConfig:
    kl8e_present: bool = True
    kl8e_keyboard_input: str = "stdin"
    kl8e_teleprinter_output: str = "stdout"

    line_printer_present: bool = True
    line_printer_output: str = "stdout"
    line_printer_column_limit: int = 132

    paper_tape_present: bool = False
    paper_tape_image: Optional[str] = None
    # Watchdog device config
    watchdog_present: bool = False
    watchdog_enabled: bool = False
    watchdog_mode: Optional[str] = None
    watchdog_periodic: bool = False
    watchdog_default_count: int = 0
    watchdog_pause_on_halt: bool = False


class EmulatorError(Exception):
    """Raised when the emulator encounters an unrecoverable condition."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Load a PDP-8 ROM and start the factory.")
    parser.add_argument(
        "image",
        type=Path,
        help="Motorola S-record image (little-endian bytes per 12-bit word).",
    )
    parser.add_argument(
        "-r",
        "--run",
        action="store_true",
        help="Start execution immediately without prompting for 'go'.",
    )
    return parser.parse_args()


def load_srec(path: Path) -> Tuple[List[Tuple[int, int]], Optional[int]]:
    """
    Parse a little-endian Motorola S-record file into (word_address, word_value) pairs.

    The PDP-8 uses 12-bit words. This loader combines consecutive byte pairs where
    the low byte appears at the even address and the high byte follows.
    """
    try:
        lines = path.read_text().splitlines()
    except OSError as exc:
        raise EmulatorError(f"Unable to read {path}: {exc}") from exc

    byte_map: Dict[int, int] = {}
    start_word: Optional[int] = None
    for raw_line in lines:
        line = raw_line.strip()
        if not line or not line.startswith("S"):
            continue

        record_type = line[1]
        if record_type not in "123":  # Data-bearing record types
            if record_type in "789":
                addr_digits = {"7": 8, "8": 6, "9": 4}[record_type]
                addr_field = line[4 : 4 + addr_digits]
                try:
                    start_byte = int(addr_field, 16)
                except ValueError as exc:
                    raise EmulatorError(f"Invalid start address in record: {line}") from exc
                start_word = (start_byte // 2) & 0x0FFF
            continue

        try:
            count = int(line[2:4], 16)
        except ValueError as exc:
            raise EmulatorError(f"Invalid byte count in record: {line}") from exc

        addr_field_len = {"1": 4, "2": 6, "3": 8}[record_type]
        addr_field = line[4 : 4 + addr_field_len]
        data_field = line[4 + addr_field_len : -2]

        try:
            base_address = int(addr_field, 16)
        except ValueError as exc:
            raise EmulatorError(f"Invalid address field in record: {line}") from exc

        if len(data_field) % 2 != 0:
            raise EmulatorError(f"Odd number of data nybbles in record: {line}")

        data_bytes = []
        for idx in range(0, len(data_field), 2):
            try:
                data_bytes.append(int(data_field[idx : idx + 2], 16))
            except ValueError as exc:
                raise EmulatorError(f"Invalid data byte in record: {line}") from exc

        expected_payload = count - (addr_field_len // 2) - 1
        if expected_payload != len(data_bytes):
            raise EmulatorError(f"Count mismatch in record: {line}")

        for offset, value in enumerate(data_bytes):
            byte_map[base_address + offset] = value

    if not byte_map:
        raise EmulatorError(f"No data records found in {path}")

    words: Dict[int, int] = {}
    for byte_address in sorted(byte_map):
        if byte_address % 2 != 0:
            continue  # wait for the even address
        lo = byte_map.get(byte_address)
        hi = byte_map.get(byte_address + 1)
        if lo is None or hi is None:
            raise EmulatorError(f"Incomplete word at byte address 0x{byte_address:04X}")
        word_address = byte_address // 2
        word_value = ((hi << 8) | lo) & 0x0FFF
        words[word_address] = word_value

    if not words:
        raise EmulatorError("No complete words decoded from S-record.")

    return sorted(words.items()), start_word


def load_library() -> ctypes.CDLL:
    root = Path(__file__).resolve().parent
    lib_path = root / "libpdp8.so"
    if not lib_path.exists():
        raise EmulatorError(
            f"{lib_path} not found. Build it with "
            "'cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o libpdp8.so'."
        )
    try:
        return ctypes.CDLL(str(lib_path))
    except OSError as exc:
        raise EmulatorError(f"Failed to load {lib_path}: {exc}") from exc


def configure_api(lib: ctypes.CDLL) -> None:
    lib.pdp8_api_create.argtypes = [ctypes.c_size_t]
    lib.pdp8_api_create.restype = ctypes.c_void_p

    lib.pdp8_api_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_destroy.restype = None

    lib.pdp8_api_reset.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_reset.restype = None

    lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
    lib.pdp8_api_write_mem.restype = ctypes.c_int

    lib.pdp8_api_read_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_read_mem.restype = ctypes.c_uint16

    lib.pdp8_api_set_pc.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_pc.restype = None

    lib.pdp8_api_get_pc.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_pc.restype = ctypes.c_uint16

    lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_ac.restype = ctypes.c_uint16

    lib.pdp8_api_get_link.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_link.restype = ctypes.c_uint8

    lib.pdp8_api_is_halted.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_is_halted.restype = ctypes.c_int

    # low-level step and AC access useful for issuing IOTs from the driver
    lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_step.restype = ctypes.c_int

    lib.pdp8_api_set_ac.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_ac.restype = None

    lib.pdp8_api_run.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.pdp8_api_run.restype = ctypes.c_int

    lib.pdp8_api_set_switch_register.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_switch_register.restype = None

    lib.pdp8_kl8e_console_create.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_kl8e_console_create.restype = ctypes.c_void_p

    lib.pdp8_kl8e_console_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_destroy.restype = None

    lib.pdp8_kl8e_console_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_kl8e_console_attach.restype = ctypes.c_int

    lib.pdp8_kl8e_console_queue_input.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
    lib.pdp8_kl8e_console_queue_input.restype = ctypes.c_int

    lib.pdp8_kl8e_console_input_pending.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_input_pending.restype = ctypes.c_size_t

    lib.pdp8_kl8e_console_output_pending.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_output_pending.restype = ctypes.c_size_t

    lib.pdp8_kl8e_console_pop_output.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8)]
    lib.pdp8_kl8e_console_pop_output.restype = ctypes.c_int

    lib.pdp8_kl8e_console_flush.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_flush.restype = ctypes.c_int

    lib.pdp8_line_printer_create.argtypes = [ctypes.c_void_p]
    lib.pdp8_line_printer_create.restype = ctypes.c_void_p

    lib.pdp8_line_printer_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_line_printer_destroy.restype = None

    lib.pdp8_line_printer_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_line_printer_attach.restype = ctypes.c_int

    lib.pdp8_line_printer_set_column_limit.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_line_printer_set_column_limit.restype = ctypes.c_int

    lib.pdp8_paper_tape_device_create.argtypes = []
    lib.pdp8_paper_tape_device_create.restype = ctypes.c_void_p

    lib.pdp8_paper_tape_device_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_paper_tape_device_destroy.restype = None

    lib.pdp8_paper_tape_device_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_paper_tape_device_attach.restype = ctypes.c_int

    lib.pdp8_paper_tape_device_load.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.pdp8_paper_tape_device_load.restype = ctypes.c_int

    lib.pdp8_paper_tape_device_label.argtypes = [ctypes.c_void_p]
    lib.pdp8_paper_tape_device_label.restype = ctypes.c_char_p

    # Watchdog API (optional; may not be present in older builds)
    try:
        lib.pdp8_watchdog_create.argtypes = []
        lib.pdp8_watchdog_create.restype = ctypes.c_void_p

        lib.pdp8_watchdog_destroy.argtypes = [ctypes.c_void_p]
        lib.pdp8_watchdog_destroy.restype = None

        lib.pdp8_watchdog_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        lib.pdp8_watchdog_attach.restype = ctypes.c_int
    except AttributeError:
        # older builds may not export these symbols; caller should handle gracefully
        pass


def write_word(lib: ctypes.CDLL, cpu: int, address: int, value: int) -> None:
    result = lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(address), ctypes.c_uint16(value & 0x0FFF))
    if result != 0:
        raise EmulatorError(f"Failed to write memory at {address:04o}")


def execute_iot(lib: ctypes.CDLL, cpu: int, instruction: int, ac: int | None = None) -> int:
    """Write an IOT instruction into address 0 and execute a single step.
    Returns resulting AC value (12-bit).
    """
    if ac is not None:
        lib.pdp8_api_set_ac(cpu, ctypes.c_uint16(ac & 0x0FFF))
    # write instruction at address 0 and set PC to 0, then step
    if lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(instruction & 0x0FFF)) != 0:
        raise EmulatorError("Failed to write IOT instruction to memory.")
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    if lib.pdp8_api_step(cpu) == 0:
        raise EmulatorError("Failed to execute IOT instruction.")
    return lib.pdp8_api_get_ac(cpu) & 0x0FFF


def load_rom_into_memory(lib: ctypes.CDLL, cpu: int, rom_words: List[Tuple[int, int]]) -> Tuple[int, int]:
    min_addr = min(addr for addr, _ in rom_words)
    max_addr = max(addr for addr, _ in rom_words)

    for address, value in rom_words:
        if not (0 <= address < DEFAULT_MEMORY_WORDS):
            raise EmulatorError(f"ROM word at {address:04o} exceeds available memory.")
        write_word(lib, cpu, address, value)

    start_address = min_addr
    return start_address, max_addr


def install_reset_vector(lib: ctypes.CDLL, cpu: int, start_address: int) -> None:
    write_word(lib, cpu, RESET_VECTOR_ADDR, JMP_INDIRECT_20)
    write_word(lib, cpu, RESET_POINTER_ADDR, start_address & 0x0FFF)
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(RESET_VECTOR_ADDR))


def pump_console_input(
    lib: ctypes.CDLL, console: int, stdin_fd: int, echo_stream: Optional[IO[str]] = None
) -> bool:
    if not console or stdin_fd < 0:
        return True

    ready, _, _ = select.select([stdin_fd], [], [], 0)
    if not ready:
        return True

    try:
        data = os.read(stdin_fd, 1024)
    except OSError as exc:
        if exc.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
            return True
        raise EmulatorError(f"Console read failed: {exc}") from exc

    if not data:
        return False

    if echo_stream is not None and data:
        echo_stream.write(data.decode("utf-8", errors="ignore"))
        echo_stream.flush()

    for byte in data:
        value = 0x0D if byte == 0x0A else (byte & 0x7F)
        if lib.pdp8_kl8e_console_queue_input(console, ctypes.c_uint8(value)) != 0:
            raise EmulatorError("Failed to queue console input.")

    return True


def run_factory(
    lib: ctypes.CDLL,
    cpu: int,
    console: int,
    stdin_fd: int,
    echo_stream: Optional[IO[str]] = None,
) -> int:
    total_cycles = 0
    while not lib.pdp8_api_is_halted(cpu):
        if not pump_console_input(lib, console, stdin_fd, echo_stream):
            break

        executed = lib.pdp8_api_run(cpu, ctypes.c_size_t(RUN_BLOCK_CYCLES))
        if executed < 0:
            raise EmulatorError("Emulator reported an error during execution.")
        if executed == 0:
            break
        total_cycles += executed

        if console:
            lib.pdp8_kl8e_console_flush(console)
            emitted = 0
            while lib.pdp8_kl8e_console_output_pending(console):
                byte = ctypes.c_uint8(0)
                if lib.pdp8_kl8e_console_pop_output(console, ctypes.byref(byte)) != 0:
                    break
                emitted += 1
            if emitted and KL8E_CHAR_PERIOD > 0.0:
                time.sleep(emitted * KL8E_CHAR_PERIOD)
                sys.stdout.flush()

    pump_console_input(lib, console, stdin_fd, echo_stream)
    if console:
        lib.pdp8_kl8e_console_flush(console)
        emitted = 0
        while lib.pdp8_kl8e_console_output_pending(console):
            byte = ctypes.c_uint8(0)
            if lib.pdp8_kl8e_console_pop_output(console, ctypes.byref(byte)) != 0:
                break
            emitted += 1
        if emitted and KL8E_CHAR_PERIOD > 0.0:
            time.sleep(emitted * KL8E_CHAR_PERIOD)
        sys.stdout.flush()
    return total_cycles


def report_state(lib: ctypes.CDLL, cpu: int, total_cycles: int) -> None:
    pc = lib.pdp8_api_get_pc(cpu) & 0x0FFF
    ac = lib.pdp8_api_get_ac(cpu) & 0x0FFF
    link = lib.pdp8_api_get_link(cpu) & 0x1
    halted = bool(lib.pdp8_api_is_halted(cpu))

    print()
    print("Factory run complete.")
    print(f"  Cycles executed: {total_cycles}")
    print(f"  PC: {pc:04o}")
    print(f"  AC: {ac:04o}")
    print(f"  LINK: {link}")
    print(f"  HALT: {'yes' if halted else 'no'}")


def load_device_config(path: Path) -> Tuple[DeviceConfig, bool]:
    config = DeviceConfig()
    if not path.exists():
        return config, False

    try:
        raw_lines = path.read_text().splitlines()
    except OSError as exc:
        raise EmulatorError(f"Failed to read {path}: {exc}")

    current: Optional[str] = None
    for raw in raw_lines:
        line, *_ = raw.split("#", 1)
        line = line.strip()
        if not line:
            continue

        if current is None:
            if not line.lower().startswith("device"):
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            name = parts[1].strip().lower()
            if not line.endswith("{"):
                continue
            current = name
            if current == "paper_tape":
                config.paper_tape_present = True
            elif current == "watchdog":
                config.watchdog_present = True
            elif current == "kl8e_console":
                config.kl8e_present = True
            elif current == "line_printer":
                config.line_printer_present = True
            continue

        if line == "}":
            current = None
            continue

        if "=" not in line or current is None:
            continue

        key, value = (part.strip() for part in line.split("=", 1))
        if not key or not value:
            continue

        if current == "kl8e_console":
            if key == "keyboard_input":
                config.kl8e_keyboard_input = value
            elif key == "teleprinter_output":
                config.kl8e_teleprinter_output = value
        elif current == "line_printer":
            if key == "output":
                config.line_printer_output = value
            elif key == "column_limit":
                try:
                    limit = int(value, 10)
                    if limit > 0:
                        config.line_printer_column_limit = limit
                except ValueError:
                    pass
        elif current == "paper_tape":
            if key == "image":
                config.paper_tape_image = value
        elif current == "watchdog":
            if key == "enabled":
                val = value.lower()
                config.watchdog_enabled = val in ("1", "true", "yes", "on")
            elif key == "mode":
                config.watchdog_mode = value.strip('"')
            elif key == "periodic":
                val = value.lower()
                config.watchdog_periodic = val in ("1", "true", "yes", "on")
            elif key == "default_count":
                try:
                    count = int(value, 10)
                    if 0 <= count <= 0x1FF:
                        config.watchdog_default_count = count
                except ValueError:
                    pass
            elif key == "pause_on_halt":
                val = value.lower()
                config.watchdog_pause_on_halt = val in ("1", "true", "yes", "on")

    return config, True


def main() -> int:
    args = parse_args()
    rom_words, start_word = load_srec(args.image)

    lib = load_library()
    configure_api(lib)
    config, config_loaded = load_device_config(Path("pdp8.config"))

    cpu = lib.pdp8_api_create(ctypes.c_size_t(DEFAULT_MEMORY_WORDS))
    if not cpu:
        raise EmulatorError("Failed to create PDP-8 instance.")

    printer = None
    console = None
    paper_tape_device = None
    wd = None
    stdin_fd = -1

    try:
        console = lib.pdp8_kl8e_console_create(None, None)
        if not console:
            raise EmulatorError("Failed to create KL8E console.")

        if lib.pdp8_kl8e_console_attach(cpu, console) != 0:
            raise EmulatorError("Failed to attach KL8E console.")

        printer = lib.pdp8_line_printer_create(None)
        if not printer:
            raise EmulatorError("Failed to create line printer peripheral.")

        if lib.pdp8_line_printer_attach(cpu, printer) != 0:
            raise EmulatorError("Failed to attach line printer peripheral.")

        lib.pdp8_line_printer_set_column_limit(
            printer, ctypes.c_uint16(max(1, config.line_printer_column_limit))
        )

        if config.paper_tape_present and config.paper_tape_image:
            paper_tape_device = lib.pdp8_paper_tape_device_create()
            if not paper_tape_device:
                print("Warning: failed to create paper tape device.", file=sys.stderr)
            else:
                image_path = Path(config.paper_tape_image)
                if not image_path.is_file():
                    print(
                        f"Warning: paper tape image '{config.paper_tape_image}' not found.",
                        file=sys.stderr,
                    )
                    lib.pdp8_paper_tape_device_destroy(paper_tape_device)
                    paper_tape_device = None
                else:
                    if (
                        lib.pdp8_paper_tape_device_load(
                            paper_tape_device, str(image_path).encode("utf-8")
                        )
                        != 0
                    ):
                        print(
                            f"Warning: failed to load paper tape image '{image_path}'.",
                            file=sys.stderr,
                        )
                        lib.pdp8_paper_tape_device_destroy(paper_tape_device)
                        paper_tape_device = None
                    elif lib.pdp8_paper_tape_device_attach(cpu, paper_tape_device) != 0:
                        print(
                            "Warning: failed to attach paper tape device.",
                            file=sys.stderr,
                        )
                        lib.pdp8_paper_tape_device_destroy(paper_tape_device)
                        paper_tape_device = None
                    else:
                        label_ptr = lib.pdp8_paper_tape_device_label(paper_tape_device)
                        label = label_ptr.decode("utf-8") if label_ptr else "(unknown)"
                        print(f"Paper tape attached: {image_path} (label {label}).")
        elif config.paper_tape_present:
            print("Warning: paper_tape device configured without an image path.", file=sys.stderr)

        # If the watchdog is configured for the factory driver, create and attach it
        wd = None
        if config.watchdog_present and config.watchdog_enabled and hasattr(lib, "pdp8_watchdog_create"):
            try:
                wd = lib.pdp8_watchdog_create()
                if not wd:
                    print("Warning: failed to create watchdog device.", file=sys.stderr)
                    wd = None
                else:
                    if lib.pdp8_watchdog_attach(cpu, wd) != 0:
                        print("Warning: failed to attach watchdog device.", file=sys.stderr)
                        lib.pdp8_watchdog_destroy(wd)
                        wd = None
                    else:
                        # Initialize the watchdog control register via IOT if a default count was provided
                        if config.watchdog_default_count > 0:
                            # Parse watchdog mode: "halt", "reset", or "interrupt"
                            mode = (config.watchdog_mode or "").lower()
                            if "halt" in mode:
                                cmd = 3  # HALT_ONE_SHOT (or 4 for HALT_PERIODIC)
                            elif "interrupt" in mode:
                                cmd = 5  # INTERRUPT_ONE_SHOT (or 6 for INTERRUPT_PERIODIC)
                            else:
                                cmd = 1  # RESET_ONE_SHOT (or 2 for RESET_PERIODIC)
                            
                            # Upgrade to periodic if watchdog_periodic is true
                            if config.watchdog_periodic:
                                cmd += 1
                            
                            control = ((cmd & 0x7) << 9) | (config.watchdog_default_count & 0x1FF)
                            execute_iot(lib, cpu, PDP8_WATCHDOG_IOT_BASE | PDP8_WATCHDOG_BIT_WRITE, ac=control)
            except Exception as exc:
                print(f"Warning: watchdog setup failed: {exc}", file=sys.stderr)

        lib.pdp8_api_reset(cpu)

        start_address, end_address = load_rom_into_memory(lib, cpu, rom_words)
        entry_address = start_word if start_word is not None else start_address
        install_reset_vector(lib, cpu, entry_address)

        print(f"Loaded {len(rom_words)} word(s) from {start_address:04o} to {end_address:04o}.")
        if start_word is not None and start_word != start_address:
            print(f"Reset vector set from S-record START: 0000 -> JMP I 20, 0020 -> {entry_address:04o}.")
        else:
            print(f"Reset vector set: 0000 -> JMP I 20, 0020 -> {entry_address:04o}.")
        if not args.run:
            print("Type 'go' to start the factory, or 'quit' to exit.")

        try:
            stdin_fd = sys.stdin.fileno()
        except (OSError, AttributeError):
            stdin_fd = -1
        echo_stream: Optional[IO[str]] = None
        if stdin_fd >= 0:
            try:
                if os.isatty(stdin_fd):
                    echo_stream = sys.stdout
            except OSError:
                echo_stream = None

        if not args.run:
            while True:
                try:
                    command = input("factory> ").strip().lower()
                except EOFError:
                    command = "quit"
                if command in ("go", "g"):
                    break
                if command in ("quit", "exit", "q"):
                    print("Exiting without running.")
                    return 0
                if not command:
                    continue
                print("Enter 'go' to run or 'quit' to exit.")

        total_cycles = run_factory(lib, cpu, console, stdin_fd, echo_stream)
        report_state(lib, cpu, total_cycles)

    finally:
        if console:
            lib.pdp8_kl8e_console_destroy(console)
        if printer:
            lib.pdp8_line_printer_destroy(printer)
        if paper_tape_device:
            lib.pdp8_paper_tape_device_destroy(paper_tape_device)
        if wd:
            try:
                if hasattr(lib, "pdp8_watchdog_destroy"):
                    lib.pdp8_watchdog_destroy(wd)
            except Exception:
                pass
        lib.pdp8_api_destroy(cpu)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except EmulatorError as exc:
        print(f"factory: {exc}", file=sys.stderr)
        sys.exit(1)
