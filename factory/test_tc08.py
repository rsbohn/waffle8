#!/usr/bin/env python3
"""
Minimal sanity checks for the TC08 device (PDP-8 magtape, device code 074).

Usage:
    python3 factory/test_tc08.py [status|go|data|skip]

Each subcommand exercises the real IOT interface for the TC08 device.
"""

import argparse
import ctypes
import sys
from pathlib import Path

TC08_INSTR_DCR = 0o7401  # Device Clear/Reset
TC08_INSTR_DSR = 0o7402  # Device Status Register (to AC)
TC08_INSTR_DLR = 0o7404  # Data Register Load (from AC)
TC08_INSTR_DSRD = 0o7405 # Data Register Store (to AC)
TC08_INSTR_SKS = 0o7406  # Skip on Status
TC08_INSTR_GO  = 0o7407  # Start operation (GO)

TC08_STATUS_READY = 0x0001


def parse_args():
    parser = argparse.ArgumentParser(description="Test TC08 device functionality.")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    parser.set_defaults(command="status")

    subparsers.add_parser("status", help="Test IOT 7402 (DSR) status read")
    subparsers.add_parser("go", help="Test IOT 7407 (GO) operation")
    subparsers.add_parser("data", help="Test IOT 7404/7405 data register load/store")
    subparsers.add_parser("skip", help="Test IOT 7406 (SKS) skip on ready")
    return parser.parse_args()


def load_library() -> ctypes.CDLL:
    factory_dir = Path(__file__).resolve().parent
    lib_path = factory_dir / "libpdp8.so"
    if not lib_path.exists():
        print(f"{lib_path} not found. Build it with cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o libpdp8.so", file=sys.stderr)
        raise SystemExit(1)
    try:
        return ctypes.CDLL(str(lib_path))
    except OSError as exc:
        print(f"Failed to load {lib_path}: {exc}", file=sys.stderr)
        raise SystemExit(1)


def configure_signatures(lib: ctypes.CDLL) -> None:
    lib.pdp8_api_create.argtypes = [ctypes.c_size_t]
    lib.pdp8_api_create.restype = ctypes.c_void_p
    lib.pdp8_api_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_destroy.restype = None
    lib.pdp8_api_reset.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_reset.restype = None
    lib.pdp8_api_clear_halt.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_clear_halt.restype = None
    lib.pdp8_api_set_pc.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_pc.restype = None
    lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
    lib.pdp8_api_write_mem.restype = ctypes.c_int
    lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_step.restype = ctypes.c_int
    lib.pdp8_api_set_ac.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_ac.restype = None
    lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_ac.restype = ctypes.c_uint16


def execute_iot(lib: ctypes.CDLL, cpu: int, instruction: int, ac: int | None = None) -> int:
    if ac is not None:
        lib.pdp8_api_set_ac(cpu, ctypes.c_uint16(ac & 0x0FFF))
    lib.pdp8_api_clear_halt(cpu)
    if lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(instruction & 0x0FFF)) != 0:
        raise RuntimeError("Failed to write IOT instruction to memory.")
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute IOT instruction.")
    return lib.pdp8_api_get_ac(cpu) & 0x0FFF


def main() -> int:
    args = parse_args()
    lib = load_library()
    configure_signatures(lib)
    cpu = lib.pdp8_api_create(4096)
    if not cpu:
        print("Failed to create PDP-8 CPU instance.", file=sys.stderr)
        return 1
    try:
        lib.pdp8_api_reset(cpu)
        if args.command == "status":
            ac = execute_iot(lib, cpu, TC08_INSTR_DSR)
            print(f"TC08 status (IOT 7402): {ac:04o} (READY={'yes' if ac & TC08_STATUS_READY else 'no'})")
        elif args.command == "go":
            ac = execute_iot(lib, cpu, TC08_INSTR_GO)
            print(f"TC08 GO (IOT 7407) executed, AC={ac:04o}")
        elif args.command == "data":
            execute_iot(lib, cpu, TC08_INSTR_DLR, ac=0o1234)
            ac = execute_iot(lib, cpu, TC08_INSTR_DSRD)
            print(f"TC08 data register roundtrip: {ac:04o}")
        elif args.command == "skip":
            # Should skip if ready (status bit 0 set)
            before = execute_iot(lib, cpu, TC08_INSTR_DSR)
            execute_iot(lib, cpu, TC08_INSTR_SKS)
            print(f"TC08 SKS (IOT 7406): status before={before:04o} (READY={'yes' if before & TC08_STATUS_READY else 'no'})")
        else:
            print(f"Unknown command: {args.command}", file=sys.stderr)
            return 2
    finally:
        lib.pdp8_api_destroy(cpu)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
