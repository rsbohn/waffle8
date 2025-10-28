#!/usr/bin/env python3
"""
Simple watchdog device tests.

Tests included:
- write/read control register roundtrip
- one-shot HALT expiry
- one-shot RESET expiry

These use the factory shared library (libpdp8.so) and the C API via ctypes.
"""

from __future__ import annotations

import ctypes
import sys
import time
from pathlib import Path

# Watchdog device constants (match src/emulator/watchdog.h)
PDP8_WATCHDOG_DEVICE_CODE = 0o55
PDP8_WATCHDOG_IOT_BASE = 0o6000 | ((PDP8_WATCHDOG_DEVICE_CODE & 0x3F) << 3)
PDP8_WATCHDOG_BIT_WRITE = 0x1
PDP8_WATCHDOG_BIT_READ = 0x2
PDP8_WATCHDOG_BIT_RESTART = 0x4

# Command encodings (matches watchdog.c enum)
WD_CMD_DISABLE = 0
WD_CMD_RESET_ONE_SHOT = 1
WD_CMD_RESET_PERIODIC = 2
WD_CMD_HALT_ONE_SHOT = 3
WD_CMD_HALT_PERIODIC = 4


def instr(bits: int) -> int:
    return PDP8_WATCHDOG_IOT_BASE | (bits & 0x7)


def load_library() -> ctypes.CDLL:
    factory_dir = Path(__file__).resolve().parent
    lib_path = factory_dir / "libpdp8.so"
    if not lib_path.exists():
        print(f"{lib_path} not found. Build it with the project Makefile.", file=sys.stderr)
        raise SystemExit(1)
    return ctypes.CDLL(str(lib_path))


def configure_signatures(lib: ctypes.CDLL) -> None:
    # Core API
    lib.pdp8_api_create.argtypes = [ctypes.c_size_t]
    lib.pdp8_api_create.restype = ctypes.c_void_p

    lib.pdp8_api_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_destroy.restype = None

    lib.pdp8_api_reset.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_reset.restype = None

    lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
    lib.pdp8_api_write_mem.restype = ctypes.c_int

    lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_step.restype = ctypes.c_int

    lib.pdp8_api_set_ac.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_ac.restype = None

    lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_ac.restype = ctypes.c_uint16

    lib.pdp8_api_get_pc.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_pc.restype = ctypes.c_uint16

    lib.pdp8_api_set_pc.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_pc.restype = None

    lib.pdp8_api_is_halted.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_is_halted.restype = ctypes.c_int

    # Watchdog API
    lib.pdp8_watchdog_create.argtypes = []
    lib.pdp8_watchdog_create.restype = ctypes.c_void_p

    lib.pdp8_watchdog_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_watchdog_destroy.restype = None

    lib.pdp8_watchdog_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_watchdog_attach.restype = ctypes.c_int


def execute_iot(lib: ctypes.CDLL, cpu: int, instruction: int, ac: int | None = None) -> int:
    """Write an IOT to memory and execute it; return resulting AC."""
    if ac is not None:
        lib.pdp8_api_set_ac(cpu, ctypes.c_uint16(ac & 0x0FFF))
    # ensure CPU is not halted
    try:
        lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    except Exception:
        pass
    if lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(instruction & 0x0FFF)) != 0:
        raise RuntimeError("Failed to write IOT instruction to memory.")
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute IOT instruction.")
    return lib.pdp8_api_get_ac(cpu) & 0x0FFF


def test_read_write_roundtrip(lib: ctypes.CDLL) -> None:
    cpu = lib.pdp8_api_create(4096)
    try:
        wd = lib.pdp8_watchdog_create()
        assert wd != 0
        assert lib.pdp8_watchdog_attach(cpu, wd) == 0

        # write control (cmd=HALT_ONE_SHOT, count=5)
        ac_val = ((WD_CMD_HALT_ONE_SHOT & 0x7) << 9) | (5 & 0x1FF)
        res = execute_iot(lib, cpu, instr(PDP8_WATCHDOG_BIT_WRITE), ac=ac_val)
        # read back
        read = execute_iot(lib, cpu, instr(PDP8_WATCHDOG_BIT_READ))
        assert read == ac_val, f"Read back {read:o} != wrote {ac_val:o}"
        print("watchdog read/write roundtrip OK")
    finally:
        if 'wd' in locals() and wd:
            lib.pdp8_watchdog_destroy(wd)
        lib.pdp8_api_destroy(cpu)


def wait_for_halt(lib: ctypes.CDLL, cpu: int, timeout: float = 2.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        # step the CPU to allow ticks to run
        lib.pdp8_api_step(cpu)
        if lib.pdp8_api_is_halted(cpu):
            return True
        time.sleep(0.02)
    return False


def wait_for_pc(lib: ctypes.CDLL, cpu: int, target_pc: int, timeout: float = 2.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        lib.pdp8_api_step(cpu)
        pc = lib.pdp8_api_get_pc(cpu)
        if pc == target_pc:
            return True
        time.sleep(0.02)
    return False


def test_one_shot_halt(lib: ctypes.CDLL) -> None:
    cpu = lib.pdp8_api_create(4096)
    try:
        wd = lib.pdp8_watchdog_create()
        assert wd != 0
        assert lib.pdp8_watchdog_attach(cpu, wd) == 0

        # ensure CPU running and not halted
        lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(10))

        # write control: HALT one-shot, count = 2 deciseconds
        ac_val = ((WD_CMD_HALT_ONE_SHOT & 0x7) << 9) | (2 & 0x1FF)
        execute_iot(lib, cpu, instr(PDP8_WATCHDOG_BIT_WRITE), ac=ac_val)

        halted = wait_for_halt(lib, cpu, timeout=3.0)
        assert halted, "watchdog did not HALT within timeout"
        print("watchdog one-shot HALT OK")
    finally:
        if 'wd' in locals() and wd:
            lib.pdp8_watchdog_destroy(wd)
        lib.pdp8_api_destroy(cpu)


def test_one_shot_reset(lib: ctypes.CDLL) -> None:
    cpu = lib.pdp8_api_create(4096)
    try:
        wd = lib.pdp8_watchdog_create()
        assert wd != 0
        assert lib.pdp8_watchdog_attach(cpu, wd) == 0

        # set PC to non-zero so reset effect is visible
        lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(123))

        # write control: RESET one-shot, count = 2 deciseconds
        ac_val = ((WD_CMD_RESET_ONE_SHOT & 0x7) << 9) | (2 & 0x1FF)
        execute_iot(lib, cpu, instr(PDP8_WATCHDOG_BIT_WRITE), ac=ac_val)

        reset = wait_for_pc(lib, cpu, 0, timeout=3.0)
        assert reset, "watchdog did not RESET (PC->0) within timeout"
        print("watchdog one-shot RESET OK")
    finally:
        if 'wd' in locals() and wd:
            lib.pdp8_watchdog_destroy(wd)
        lib.pdp8_api_destroy(cpu)


if __name__ == '__main__':
    lib = load_library()
    configure_signatures(lib)

    test_read_write_roundtrip(lib)
    test_one_shot_halt(lib)
    test_one_shot_reset(lib)

    print('watchdog tests completed')
