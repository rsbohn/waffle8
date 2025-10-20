#!/usr/bin/env python3
"""
Minimal sanity check for the magtape device.

python3 factory/test_magtape.py [--use-sense]

Loads the PDP-8 shared library, attaches a magtape controller, configures
unit 0 to point at the repository's demo directory, and prints the name of
the current record for that unit.
"""

from __future__ import annotations

import argparse
import ctypes
import sys
from pathlib import Path

MAGTAPE_INSTR_GO = 0o6701
MAGTAPE_INSTR_READ = 0o6702
MAGTAPE_INSTR_SENSE = 0o6740

MAGTAPE_STATUS_READY = 0x0001
MAGTAPE_STATUS_EOR = 0x0004
MAGTAPE_STATUS_EOT = 0x0008


class MagtapeUnitParams(ctypes.Structure):
    _fields_ = [
        ("unit_number", ctypes.c_uint),
        ("path", ctypes.c_char_p),
        ("write_protected", ctypes.c_bool),
    ]


class MagtapeUnitStatus(ctypes.Structure):
    _fields_ = [
        ("configured", ctypes.c_bool),
        ("unit_number", ctypes.c_uint),
        ("path", ctypes.c_char_p),
        ("current_record", ctypes.c_char_p),
        ("record_index", ctypes.c_size_t),
        ("record_count", ctypes.c_size_t),
        ("word_position", ctypes.c_size_t),
        ("word_count", ctypes.c_size_t),
        ("ready", ctypes.c_bool),
        ("write_protected", ctypes.c_bool),
        ("end_of_record", ctypes.c_bool),
        ("end_of_tape", ctypes.c_bool),
        ("error", ctypes.c_bool),
        ("partial_record", ctypes.c_bool),
    ]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect magtape unit 0 records.")
    parser.add_argument(
        "--use-sense",
        action="store_true",
        help="Detect record boundaries via the SENSE IOT (may expose device bugs).",
    )
    return parser.parse_args()


def _read_limit(status: MagtapeUnitStatus) -> int:
    base = int(status.word_count) + 1 if int(status.word_count) > 0 else 512
    base = max(base, 32)
    return min(base, 65536)


def load_library() -> ctypes.CDLL:
    factory_dir = Path(__file__).resolve().parent
    lib_path = factory_dir / "libpdp8.so"
    if not lib_path.exists():
        print(
            f"{lib_path} not found. Build it with "
            "cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o libpdp8.so",
            file=sys.stderr,
        )
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

    lib.pdp8_magtape_device_create.restype = ctypes.c_void_p
    lib.pdp8_magtape_device_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_magtape_device_destroy.restype = None

    lib.pdp8_magtape_device_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_magtape_device_attach.restype = ctypes.c_int

    lib.pdp8_magtape_device_configure_unit.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(MagtapeUnitParams),
    ]
    lib.pdp8_magtape_device_configure_unit.restype = ctypes.c_int

    lib.pdp8_magtape_device_get_status.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint,
        ctypes.POINTER(MagtapeUnitStatus),
    ]
    lib.pdp8_magtape_device_get_status.restype = ctypes.c_int


def execute_iot(lib: ctypes.CDLL, cpu: int, instruction: int, ac: int | None = None) -> int:
    if ac is not None:
        lib.pdp8_api_set_ac(cpu, ctypes.c_uint16(ac & 0x0FFF))
    lib.pdp8_api_clear_halt(cpu)
    if (
        lib.pdp8_api_write_mem(
            cpu, ctypes.c_uint16(0), ctypes.c_uint16(instruction & 0x0FFF)
        )
        != 0
    ):
        raise RuntimeError("Failed to write IOT instruction to memory.")
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute IOT instruction.")
    return lib.pdp8_api_get_ac(cpu) & 0x0FFF


def seek_next_record(
    lib: ctypes.CDLL, cpu: int, device: int, status: MagtapeUnitStatus, unit: int = 0
) -> tuple[str, MagtapeUnitStatus] | None:
    if not status.configured:
        return None
    if status.record_count == 0 or status.record_index + 1 >= status.record_count:
        return None

    execute_iot(lib, cpu, MAGTAPE_INSTR_GO, ac=unit)

    word_limit = _read_limit(status)
    reads = 0

    while True:
        current = MagtapeUnitStatus()
        if lib.pdp8_magtape_device_get_status(device, ctypes.c_uint(unit), ctypes.byref(current)) != 0:
            raise RuntimeError("Failed to query magtape status during seek.")
        if current.end_of_tape:
            return None
        if current.end_of_record:
            break
        execute_iot(lib, cpu, MAGTAPE_INSTR_READ)
        reads += 1
        if reads > word_limit:
            raise RuntimeError("Reached read limit while seeking next record.")

    execute_iot(lib, cpu, MAGTAPE_INSTR_GO, ac=unit)

    updated = MagtapeUnitStatus()
    if lib.pdp8_magtape_device_get_status(device, ctypes.c_uint(unit), ctypes.byref(updated)) != 0:
        raise RuntimeError("Failed to query magtape status after seek.")
    if updated.current_record:
        return updated.current_record.decode("utf-8"), updated
    return None


def seek_next_record_via_sense(
    lib: ctypes.CDLL, cpu: int, device: int, status: MagtapeUnitStatus, unit: int = 0
) -> tuple[str, MagtapeUnitStatus] | None:
    if not status.configured:
        return None
    if status.record_count == 0 or status.record_index + 1 >= status.record_count:
        return None

    execute_iot(lib, cpu, MAGTAPE_INSTR_GO, ac=unit)

    word_limit = _read_limit(status)
    sense_limit = word_limit * 2
    reads = 0
    sense_ops = 0

    while True:
        sense = execute_iot(lib, cpu, MAGTAPE_INSTR_SENSE)
        sense_ops += 1
        if sense & MAGTAPE_STATUS_EOT:
            raise RuntimeError("SENSE reported end-of-tape before reaching the next record.")
        if sense & MAGTAPE_STATUS_EOR:
            break
        execute_iot(lib, cpu, MAGTAPE_INSTR_READ)
        reads += 1
        if reads > word_limit or sense_ops > sense_limit:
            raise RuntimeError("SENSE-based seek exceeded safety limits.")

    execute_iot(lib, cpu, MAGTAPE_INSTR_GO, ac=unit)

    updated = MagtapeUnitStatus()
    if lib.pdp8_magtape_device_get_status(device, ctypes.c_uint(unit), ctypes.byref(updated)) != 0:
        raise RuntimeError("Failed to query magtape status after SENSE-based seek.")
    if updated.current_record:
        return updated.current_record.decode("utf-8"), updated
    return None


def main() -> int:
    args = parse_args()

    lib = load_library()
    configure_signatures(lib)

    cpu = lib.pdp8_api_create(ctypes.c_size_t(0))
    if not cpu:
        print("Unable to create PDP-8 instance.", file=sys.stderr)
        return 1

    device = lib.pdp8_magtape_device_create()
    if not device:
        print("Unable to create magtape controller.", file=sys.stderr)
        lib.pdp8_api_destroy(cpu)
        return 1

    try:
        if lib.pdp8_magtape_device_attach(cpu, device) != 0:
            print("Failed to attach magtape controller.", file=sys.stderr)
            return 1

        lib.pdp8_api_reset(cpu)

        repo_root = Path(__file__).resolve().parent.parent
        demo_path = (repo_root / "demo").resolve()
        if not demo_path.is_dir():
            print(f"Magtape demo directory not found: {demo_path}", file=sys.stderr)
            return 1

        path_bytes = str(demo_path).encode("utf-8")

        params = MagtapeUnitParams()
        params.unit_number = 0
        params.path = path_bytes
        params.write_protected = True
        if lib.pdp8_magtape_device_configure_unit(device, ctypes.byref(params)) != 0:
            print("Failed to configure magtape unit 0.", file=sys.stderr)
            return 1

        status = MagtapeUnitStatus()
        if lib.pdp8_magtape_device_get_status(device, ctypes.c_uint(0), ctypes.byref(status)) != 0:
            print("Failed to query magtape status.", file=sys.stderr)
            return 1

        if not status.configured:
            print("Magtape unit 0 is not configured.", file=sys.stderr)
            return 1

        records: list[str] = []
        error: RuntimeError | None = None
        current_status = status

        if current_status.current_record:
            records.append(current_status.current_record.decode("utf-8"))

        seek_func = seek_next_record_via_sense if args.use_sense else seek_next_record

        while len(records) < current_status.record_count:
            try:
                result = seek_func(lib, cpu, device, current_status, 0)
            except RuntimeError as exc:
                error = exc
                break
            if result is None:
                break
            next_name, current_status = result
            records.append(next_name)

        if not records:
            print("No records found on magtape unit 0.")
        else:
            width = max(3, len(str(len(records) - 1)))
            for index, name in enumerate(records):
                print(f"{index:0{width}d} {name}")

        if error:
            print(f"SENSE test aborted: {error}", file=sys.stderr)
            return 1

    finally:
        if device:
            lib.pdp8_magtape_device_destroy(device)
        if cpu:
            lib.pdp8_api_destroy(cpu)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
