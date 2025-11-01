#!/usr/bin/env python3
"""
Interactive curses front panel for the PDP-8 emulator.

This script reuses the factory driver helpers to initialise the emulator and
peripherals, then paints register state and console output in a curses UI.
"""

from __future__ import annotations

import argparse
import curses
import ctypes
import sys
import time
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Deque, Optional

# Ensure the repository root is on sys.path so we can import factory.driver.
REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from factory import driver as factory_driver


OUTPUT_MAX_LINES = 20
DEFAULT_REFRESH_SEC = 0.05


def _stream_isatty(stream: object) -> bool:
    try:
        return bool(getattr(stream, "isatty")())
    except Exception:
        return False


@dataclass
class PanelState:
    lib: ctypes.CDLL
    cpu: int
    console: Optional[int]
    printer: Optional[int]
    paper_tape: Optional[int]
    watchdog: Optional[int]
    image_path: Path
    total_cycles: int = 0
    paused: bool = True
    exiting: bool = False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Visual PDP-8 front panel.")
    parser.add_argument(
        "image",
        type=Path,
        help="Motorola S-record image (little-endian bytes per 12-bit word).",
    )
    parser.add_argument(
        "--refresh",
        type=float,
        default=DEFAULT_REFRESH_SEC,
        help="UI refresh period in seconds (default: %(default).2f).",
    )
    parser.add_argument(
        "--block-cycles",
        type=int,
        default=factory_driver.RUN_BLOCK_CYCLES,
        help="Number of cycles per emulator run block.",
    )
    return parser.parse_args()


def initialise_emulator(image: Path) -> PanelState:
    lib = factory_driver.load_library()
    factory_driver.configure_api(lib)

    config, _ = factory_driver.load_device_config(Path("pdp8.config"))
    rom_words, start_word = factory_driver.load_srec(image)

    cpu = lib.pdp8_api_create(ctypes.c_size_t(factory_driver.DEFAULT_MEMORY_WORDS))
    if not cpu:
        raise factory_driver.EmulatorError("Failed to create PDP-8 instance.")

    console = None
    printer = None
    paper_tape = None
    watchdog = None

    try:
        if hasattr(lib, "pdp8_interrupt_control_attach"):
            if lib.pdp8_interrupt_control_attach(cpu) != 0:
                raise factory_driver.EmulatorError("Failed to attach interrupt control device.")

        console = lib.pdp8_kl8e_console_create(None, None)
        if not console:
            raise factory_driver.EmulatorError("Failed to create KL8E console.")
        if lib.pdp8_kl8e_console_attach(cpu, console) != 0:
            raise factory_driver.EmulatorError("Failed to attach KL8E console.")

        printer = lib.pdp8_line_printer_create(None)
        if not printer:
            raise factory_driver.EmulatorError("Failed to create line printer peripheral.")
        if lib.pdp8_line_printer_attach(cpu, printer) != 0:
            raise factory_driver.EmulatorError("Failed to attach line printer peripheral.")
        lib.pdp8_line_printer_set_column_limit(
            printer, ctypes.c_uint16(max(1, config.line_printer_column_limit))
        )

        if config.paper_tape_present and config.paper_tape_image:
            paper_tape = lib.pdp8_paper_tape_device_create()
            if paper_tape:
                if lib.pdp8_paper_tape_device_load(
                    paper_tape, str(Path(config.paper_tape_image)).encode("utf-8")
                ) != 0:
                    sys.stderr.write(
                        f"Warning: failed to load paper tape image '{config.paper_tape_image}'.\n"
                    )
                    lib.pdp8_paper_tape_device_destroy(paper_tape)
                    paper_tape = None
                elif lib.pdp8_paper_tape_device_attach(cpu, paper_tape) != 0:
                    sys.stderr.write("Warning: failed to attach paper tape device.\n")
                    lib.pdp8_paper_tape_device_destroy(paper_tape)
                    paper_tape = None
        elif config.paper_tape_present:
            sys.stderr.write("Warning: paper_tape device configured without an image path.\n")

        if (
            config.watchdog_present
            and config.watchdog_enabled
            and hasattr(lib, "pdp8_watchdog_create")
        ):
            try:
                watchdog = lib.pdp8_watchdog_create()
                if not watchdog:
                    sys.stderr.write("Warning: failed to create watchdog device.\n")
                elif lib.pdp8_watchdog_attach(cpu, watchdog) != 0:
                    sys.stderr.write("Warning: failed to attach watchdog device.\n")
                    lib.pdp8_watchdog_destroy(watchdog)
                    watchdog = None
            except Exception as exc:  # pragma: no cover - defensive
                sys.stderr.write(f"Warning: watchdog setup failed: {exc}\n")
                watchdog = None

        lib.pdp8_api_reset(cpu)

        start_address, end_address = factory_driver.load_rom_into_memory(lib, cpu, rom_words)
        entry_address = start_word if start_word is not None else start_address
        factory_driver.install_reset_vector(lib, cpu, entry_address)

        print(f"Loaded {len(rom_words)} word(s) from {start_address:04o} to {end_address:04o}.")
        if start_word is not None and start_word != start_address:
            print(
                "Reset vector set from S-record START: 0000 -> JMP I 20, "
                f"0020 -> {entry_address:04o}."
            )
        else:
            print(f"Reset vector set: 0000 -> JMP I 20, 0020 -> {entry_address:04o}.")

        return PanelState(
            lib=lib,
            cpu=cpu,
            console=console,
            printer=printer,
            paper_tape=paper_tape,
            watchdog=watchdog,
            image_path=image,
        )
    except Exception:
        if console:
            lib.pdp8_kl8e_console_destroy(console)
        if printer:
            lib.pdp8_line_printer_destroy(printer)
        if paper_tape:
            lib.pdp8_paper_tape_device_destroy(paper_tape)
        if watchdog and hasattr(lib, "pdp8_watchdog_destroy"):
            try:
                lib.pdp8_watchdog_destroy(watchdog)
            except Exception:
                pass
        lib.pdp8_api_destroy(cpu)
        raise


def teardown_panel(state: PanelState) -> None:
    if state.console:
        state.lib.pdp8_kl8e_console_destroy(state.console)
    if state.printer:
        state.lib.pdp8_line_printer_destroy(state.printer)
    if state.paper_tape:
        state.lib.pdp8_paper_tape_device_destroy(state.paper_tape)
    if state.watchdog and hasattr(state.lib, "pdp8_watchdog_destroy"):
        try:
            state.lib.pdp8_watchdog_destroy(state.watchdog)
        except Exception:
            pass
    state.lib.pdp8_api_destroy(state.cpu)


def append_output(lines: Deque[str], text: str) -> None:
    for char in text:
        if char in ("\r", "\n"):
            lines.append("")
        else:
            if not lines:
                lines.append("")
            lines[-1] += char
        while len(lines) > OUTPUT_MAX_LINES:
            lines.popleft()
    if not lines:
        lines.append("")


def poll_console_output(state: PanelState, buffer: Deque[str]) -> int:
    emitted = 0
    if not state.console:
        return emitted

    state.lib.pdp8_kl8e_console_flush(state.console)
    while state.lib.pdp8_kl8e_console_output_pending(state.console):
        byte = ctypes.c_uint8(0)
        if state.lib.pdp8_kl8e_console_pop_output(state.console, ctypes.byref(byte)) != 0:
            break
        ch = byte.value & 0x7F
        if ch == 0x0D:
            append_output(buffer, "\n")
        elif ch == 0x0A:
            continue
        elif 0x20 <= ch <= 0x7E:
            append_output(buffer, chr(ch))
        emitted += 1
    return emitted


def queue_console_input(state: PanelState, char_code: int) -> None:
    if not state.console:
        return
    if char_code in (curses.KEY_BACKSPACE,):
        value = 0x08
    elif char_code in (10, 13):
        value = 0x0D
    elif 0x20 <= char_code <= 0x7E:
        value = char_code & 0x7F
    else:
        return
    result = state.lib.pdp8_kl8e_console_queue_input(state.console, ctypes.c_uint8(value))
    if result != 0:
        raise factory_driver.EmulatorError("Failed to queue console input.")


def draw_panel(stdscr: curses.window, state: PanelState, output_buffer: Deque[str]) -> None:
    stdscr.erase()
    height, width = stdscr.getmaxyx()

    status = "PAUSED" if state.paused else "RUNNING"
    halted = bool(state.lib.pdp8_api_is_halted(state.cpu))
    pc = state.lib.pdp8_api_get_pc(state.cpu) & 0x0FFF
    ac = state.lib.pdp8_api_get_ac(state.cpu) & 0x0FFF
    link = state.lib.pdp8_api_get_link(state.cpu) & 0x1

    title = f"PDP-8 Front Panel — {status}"
    stdscr.addnstr(0, 0, title.ljust(width), width)

    stdscr.addnstr(
        1,
        0,
        f"Image: {state.image_path} ",
        width,
    )
    stdscr.addnstr(
        2,
        0,
        (
            f"Cycles: {state.total_cycles}    PC: {pc:04o}    AC: {ac:04o}    "
            f"LINK: {link}    HALT: {'yes' if halted else 'no'}"
        ),
        width,
    )
    stdscr.addnstr(
        3,
        0,
        (
            "Controls (starts paused): space=pause/resume, s=step (1 instr), q=quit; "
            "printable keys send console input"
        ),
        width,
    )
    stdscr.hline(4, 0, "-", width)

    available_rows = height - 6
    output_lines = list(output_buffer)[-available_rows:] if available_rows > 0 else []
    for idx, line in enumerate(output_lines):
        stdscr.addnstr(5 + idx, 0, line, width)

    stdscr.refresh()


def panel_loop(
    stdscr: curses.window,
    state: PanelState,
    refresh_period: float,
    block_cycles: int,
) -> None:
    curses.curs_set(0)
    stdscr.nodelay(True)
    stdscr.timeout(0)

    output_buffer: Deque[str] = deque([""], maxlen=OUTPUT_MAX_LINES)

    try:
        while not state.exiting:
            if not state.paused and not state.lib.pdp8_api_is_halted(state.cpu):
                executed = state.lib.pdp8_api_run(state.cpu, ctypes.c_size_t(block_cycles))
                if executed < 0:
                    raise factory_driver.EmulatorError(
                        "Emulator reported an error during execution."
                    )
                state.total_cycles += executed
                if executed == 0:
                    time.sleep(refresh_period)

            emitted = poll_console_output(state, output_buffer)
            if emitted and factory_driver.KL8E_CHAR_PERIOD > 0.0:
                time.sleep(emitted * factory_driver.KL8E_CHAR_PERIOD)

            draw_panel(stdscr, state, output_buffer)

            key = stdscr.getch()
            handled = False
            while key != curses.ERR:
                if key == ord("q"):
                    state.exiting = True
                    handled = True
                elif key == ord(" "):
                    state.paused = not state.paused
                    handled = True
                elif key == ord("s"):
                    handled = True
                    if state.paused and not state.lib.pdp8_api_is_halted(state.cpu):
                        stepped = state.lib.pdp8_api_step(state.cpu)
                        if stepped < 0:
                            raise factory_driver.EmulatorError(
                                "Emulator reported an error during step execution."
                            )
                        state.total_cycles += stepped
                else:
                    queue_console_input(state, key)
                key = stdscr.getch()
            if not handled:
                time.sleep(refresh_period)

            if state.lib.pdp8_api_is_halted(state.cpu) and not state.paused:
                state.paused = True
    finally:
        draw_panel(stdscr, state, output_buffer)
        stdscr.nodelay(False)
        stdscr.timeout(-1)


def main() -> int:
    args = parse_args()
    if not (_stream_isatty(sys.stdin) and _stream_isatty(sys.stdout)):
        raise factory_driver.EmulatorError("Visual front panel requires an interactive terminal.")

    state = initialise_emulator(args.image)
    try:
        curses.wrapper(panel_loop, state, args.refresh, args.block_cycles)
    finally:
        try:
            factory_driver.report_state(state.lib, state.cpu, state.total_cycles)
        finally:
            teardown_panel(state)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except factory_driver.EmulatorError as exc:
        print(f"visual: {exc}", file=sys.stderr)
        sys.exit(1)
