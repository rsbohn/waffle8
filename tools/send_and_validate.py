#!/usr/bin/env python3
from pathlib import Path
from factory import driver

SREC = Path(__file__).resolve().parents[1] / 'bbb.srec'
if not SREC.exists():
    print('bbb.srec not found at', SREC)
    raise SystemExit(2)

try:
    rom_words, start_word = driver.load_srec(SREC)
except Exception as e:
    print('Failed to parse SREC:', e)
    raise

lib = driver.load_library()
driver.configure_api(lib)

# create CPU with default size (4K)
cpu = lib.pdp8_api_create(0x1000)
# write words into memory
try:
    start_addr, end_addr = driver.load_rom_into_memory(lib, cpu, rom_words)
except Exception as e:
    print('Failed to load ROM into memory:', e)
    raise

# read back and validate
errors = []
for addr, expected in rom_words:
    got = lib.pdp8_api_read_mem(cpu, addr & 0o7777) & 0x0FFF
    if got != (expected & 0x0FFF):
        errors.append((addr, expected & 0x0FFF, got))

if errors:
    print('Validation FAILED: mismatches:')
    for a, exp, got in errors:
        print(f" {a:04o}: expected {exp:04o}, got {got:04o}")
    raise SystemExit(1)

print('Validation OK: all words written and read back match.')
print('Start word from SREC (if any):', f"{start_word:04o}" if start_word is not None else None)
print(f'ROM written from {start_addr:04o} to {end_addr:04o}')
