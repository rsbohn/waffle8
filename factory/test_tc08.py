#!/usr/bin/env python3
"""
Minimal sanity checks for the TC08 device (PDP-8 magtape, device code 074).

Usage:
    python3 factory/test_tc08.py [skip|write]

This follows the minimal TC08 bootstrap flow: after reset the controller reports
ready; SKS (6746) should therefore skip the next instruction. The write command
targets unit 1 (read/write) and verifies block 0 survives a write/read cycle,
creating the image file if it does not exist.
"""

import argparse
import ctypes
import sys
import tempfile
import os
from pathlib import Path

TC08_INSTR_DCR = 0o6762  # Device Clear/Reset (device 076 func 2)
TC08_INSTR_DTXA = 0o6771 # Load transfer address (device 077 func 1)
TC08_INSTR_DTSF = 0o6764 # Skip-if-ready in our minimal model (device 076 func 4)
TC08_INSTR_DTLB = 0o6766 # Load block number and start transfer (device 076 func 6)
TC08_INSTR_GO  = 0o6767  # Start operation (GO)

TC08_FLAG_UNIT1 = 0o2000  # AC bit 10 selects unit 1
TC08_FLAG_WRITE = 0o4000  # AC bit 11 selects write direction

TC08_STATUS_READY = 0x0001


def parse_args():
    parser = argparse.ArgumentParser(description="Test TC08 device functionality.")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    parser.set_defaults(command="skip")

    subparsers.add_parser("skip", help="Test IOT 7406 (SKS) skip on ready")
    subparsers.add_parser("write", help="Test writing a block to TC08 unit 1 (RW)")
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
    lib.pdp8_board_host_simulator.argtypes = []
    lib.pdp8_board_host_simulator.restype = ctypes.c_void_p
    lib.pdp8_api_create_for_board.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_create_for_board.restype = ctypes.c_void_p
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
    lib.pdp8_api_get_pc.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_pc.restype = ctypes.c_uint16


def execute_iot(lib: ctypes.CDLL, cpu: int, instruction: int, ac: int | None = None) -> tuple[int, int]:
    if ac is not None:
        lib.pdp8_api_set_ac(cpu, ctypes.c_uint16(ac & 0x0FFF))
    lib.pdp8_api_clear_halt(cpu)
    if lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(instruction & 0x0FFF)) != 0:
        raise RuntimeError("Failed to write IOT instruction to memory.")
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute IOT instruction.")
    ac_out = lib.pdp8_api_get_ac(cpu) & 0x0FFF
    pc_out = lib.pdp8_api_get_pc(cpu) & 0x0FFF
    return ac_out, pc_out


def main() -> int:
    args = parse_args()
    lib = load_library()
    configure_signatures(lib)
    tempdir: tempfile.TemporaryDirectory[str] | None = None
    if args.command == "write":
        tempdir = tempfile.TemporaryDirectory()
        os.environ["TC08_IMAGE1"] = str(Path(tempdir.name) / "unit1.tu56")

    cpu = lib.pdp8_api_create_for_board(lib.pdp8_board_host_simulator())
    if not cpu:
        if tempdir:
            tempdir.cleanup()
        print("Failed to create PDP-8 CPU instance.", file=sys.stderr)
        return 1
    try:
        lib.pdp8_api_reset(cpu)
        if args.command == "skip":
            # After reset, the device reports ready and DTSF (6764) should skip the next instruction
            ac0, pc0 = execute_iot(lib, cpu, TC08_INSTR_DTSF)
            skipped = pc0 == 2  # instruction at 0 should skip over location 1
            print(f"TC08 DTSF (IOT 6764): AC={ac0:04o} PC={pc0:04o} SKIPPED={'yes' if skipped else 'no'}")
        elif args.command == "write":
            base_addr = 0o2000
            pattern = [(i * 3) & 0x0FFF for i in range(256)]
            for offset, word in enumerate(pattern):
                lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(base_addr + offset), ctypes.c_uint16(word))

            execute_iot(lib, cpu, TC08_INSTR_DTXA, ac=base_addr)
            write_ac = TC08_FLAG_UNIT1 | TC08_FLAG_WRITE  # unit 1, write
            execute_iot(lib, cpu, TC08_INSTR_DTLB, ac=write_ac)

            # Clear the buffer and read the block back from unit 1
            for offset in range(256):
                lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(base_addr + offset), ctypes.c_uint16(0))
            execute_iot(lib, cpu, TC08_INSTR_DTXA, ac=base_addr)
            execute_iot(lib, cpu, TC08_INSTR_DTLB, ac=TC08_FLAG_UNIT1)

            read_back = [
                lib.pdp8_api_read_mem(cpu, ctypes.c_uint16(base_addr + offset)) & 0x0FFF
                for offset in range(256)
            ]
            image_path = os.environ.get("TC08_IMAGE1", "")
            success = read_back == pattern and image_path and Path(image_path).exists()
            print(f"TC08 unit1 write/readback: {'ok' if success else 'FAILED'} image={image_path}")
            if not success:
                return 3
        else:
            print(f"Unknown command: {args.command}", file=sys.stderr)
            return 2
    finally:
        lib.pdp8_api_destroy(cpu)
        if tempdir:
            tempdir.cleanup()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
