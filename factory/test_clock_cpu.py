#!/usr/bin/env python3
"""
Pytest that drives demo/clock.srec directly through the emulator API.

The test loads the S-record into a fresh CPU, starts execution at 0200
(the first routine), runs until halt, then inspects the RAM-resident
results. It prints the captured words for visibility and asserts the
computed hour matches the host clock (allowing a boundary grace period).
"""

from __future__ import annotations

import ctypes
from datetime import datetime
from pathlib import Path
from typing import Iterable, Tuple

import pytest

from factory import driver

REPO_ROOT = Path(__file__).resolve().parents[1]
CLOCK_SREC = REPO_ROOT / "demo/clock.srec"
TIME_ADDR = 0o225
HOURS_ADDR = 0o226
MINS_ADDR = 0o227
PROGRAM_START = 0o200
MAX_CYCLES = 6000


@pytest.fixture(scope="module")
def lib() -> ctypes.CDLL:
    shared = driver.load_library()
    driver.configure_api(shared)
    return shared


def load_words(lib: ctypes.CDLL, cpu: int, rom_words: Iterable[Tuple[int, int]]) -> None:
    for addr, value in rom_words:
        rc = lib.pdp8_api_write_mem(
            cpu, ctypes.c_uint16(addr & 0x0FFF), ctypes.c_uint16(value & 0x0FFF)
        )
        assert rc == 0, f"write_mem failed at {addr:04o}"


def run_until_halt(lib: ctypes.CDLL, cpu: int, max_cycles: int) -> int:
    cycles = 0
    while not lib.pdp8_api_is_halted(cpu) and cycles < max_cycles:
        step = lib.pdp8_api_step(cpu)
        assert step >= 0, "emulator reported error during step"
        if step == 0:
            break
        cycles += 1
    return cycles


def test_clock_rom_first_block(lib: ctypes.CDLL, capsys: pytest.CaptureFixture[str]) -> None:
    rom_words, _start_word = driver.load_srec(CLOCK_SREC)
    cpu = lib.pdp8_api_create(driver.DEFAULT_MEMORY_WORDS)
    assert cpu
    try:
        lib.pdp8_api_reset(cpu)
        load_words(lib, cpu, rom_words)

        lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(PROGRAM_START))
        start_time = datetime.now()
        start_minutes = start_time.hour * 60 + start_time.minute

        cycles = run_until_halt(lib, cpu, MAX_CYCLES)
        assert lib.pdp8_api_is_halted(cpu), "clock routine failed to halt"

        time_now = lib.pdp8_api_read_mem(cpu, ctypes.c_uint16(TIME_ADDR)) & 0x0FFF
        hours = lib.pdp8_api_read_mem(cpu, ctypes.c_uint16(HOURS_ADDR)) & 0x0FFF
        mins = lib.pdp8_api_read_mem(cpu, ctypes.c_uint16(MINS_ADDR)) & 0x0FFF

        print(f"clock demo snapshot: TIME={time_now:04o} HOURS={hours:04o} MINS={mins:04o} cycles={cycles}")

        # allow for minute rollover between start and finish
        end_time = datetime.now()
        end_minutes = end_time.hour * 60 + end_time.minute
        expected_hours = {start_minutes // 60, end_minutes // 60}
        assert hours in expected_hours, (
            f"computed hour {hours:02o} not in {expected_hours}; "
            f"memory dump: time={time_now:04o} hours={hours:04o} mins={mins:04o}"
        )
    finally:
        lib.pdp8_api_destroy(cpu)
