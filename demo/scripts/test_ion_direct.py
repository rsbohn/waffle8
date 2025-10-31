#!/usr/bin/env python3
"""Direct test of ION/IOFF/SKON instructions via libpdp8"""

import ctypes
import sys
from pathlib import Path

# Load the library
lib_path = Path("libpdp8.so")
if not lib_path.exists():
    print(f"Error: {lib_path} not found", file=sys.stderr)
    sys.exit(1)

lib = ctypes.CDLL(str(lib_path))

# Configure API functions
lib.pdp8_api_create.argtypes = [ctypes.c_size_t]
lib.pdp8_api_create.restype = ctypes.c_void_p

lib.pdp8_api_destroy.argtypes = [ctypes.c_void_p]
lib.pdp8_api_destroy.restype = None

lib.pdp8_api_set_ac.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
lib.pdp8_api_set_ac.restype = ctypes.c_int

lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
lib.pdp8_api_get_ac.restype = ctypes.c_uint16

lib.pdp8_api_set_pc.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
lib.pdp8_api_set_pc.restype = ctypes.c_int

lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
lib.pdp8_api_write_mem.restype = ctypes.c_int

lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
lib.pdp8_api_step.restype = ctypes.c_int

lib.pdp8_api_is_interrupt_enabled.argtypes = [ctypes.c_void_p]
lib.pdp8_api_is_interrupt_enabled.restype = ctypes.c_int

# Interrupt control attach
if hasattr(lib, "pdp8_interrupt_control_attach"):
    lib.pdp8_interrupt_control_attach.argtypes = [ctypes.c_void_p]
    lib.pdp8_interrupt_control_attach.restype = ctypes.c_int
    has_interrupt_control = True
else:
    has_interrupt_control = False

# Create CPU
cpu = lib.pdp8_api_create(ctypes.c_size_t(4096))
if not cpu:
    print("Failed to create CPU", file=sys.stderr)
    sys.exit(1)

print(f"CPU created: {cpu}")

# Attach interrupt control device
if has_interrupt_control:
    result = lib.pdp8_interrupt_control_attach(cpu)
    print(f"Interrupt control attach result: {result}")
    if result != 0:
        print("Warning: Failed to attach interrupt control device", file=sys.stderr)
else:
    print("Warning: interrupt_control_attach not found in library", file=sys.stderr)

# Test 1: IOFF (6000 octal = 0xC00 hex)
print("\n=== Test 1: IOFF (disable interrupts) ===")
lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(0o6000))  # IOFF
lib.pdp8_api_step(cpu)
ion_status = lib.pdp8_api_is_interrupt_enabled(cpu)
print(f"After IOFF: ION={ion_status} (expected 0)")

# Test 2: ION (6001 octal = 0xC01 hex)
print("\n=== Test 2: ION (enable interrupts) ===")
lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(0o6001))  # ION
lib.pdp8_api_step(cpu)
ion_status = lib.pdp8_api_is_interrupt_enabled(cpu)
print(f"After ION: ION={ion_status} (expected 1)")

# Test 3: SKON (6002 octal = skip if ION)
print("\n=== Test 3: SKON (skip if ION enabled) ===")
lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(0), ctypes.c_uint16(0o6002))  # SKON
lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(1), ctypes.c_uint16(0o5004))  # JMP 4
lib.pdp8_api_step(cpu)
pc = lib.pdp8_api_get_ac(cpu)  # This doesn't get PC, just for testing
ion_status = lib.pdp8_api_is_interrupt_enabled(cpu)
print(f"After SKON: ION still enabled={ion_status}")
# PC should have been incremented by 2 due to skip if ION is enabled
print("(SKON should have skipped the JMP if ION is enabled)")

lib.pdp8_api_destroy(cpu)
print("\nTest complete.")
