# PDP-8 Waffle Factory

Technical Specification v1.0
Target Platform: Linux (x86_64/ARM64)
Future Target: Adafruit Fruit Jam (RP2350)
License: MIT
Language: C (core) + Python 3.10+ (simulation/UI)

## Project Overview

A historically accurate PDP-8 minicomputer emulator running the RTS-8 real-time operating system to control a simulated waffle production factory. The system demonstrates industrial process control techniques from the 1960s-70s applied to modern simulation and visualization.

## Hardware Targets

- **Host Simulator** – default development configuration running on the local workstation.
- **Adafruit Fruit Jam (RP2350)** – dual-core RP2350 microcontroller platform. The board definition reserves a 4K-word PDP-8 memory map and assumes a 200 MHz system clock with 60 Hz timing ticks for factory automation workloads.

## Factory Features

- Batter preparation system
- 16 automated waffle irons
- Cooling conveyor
- Automated packaging
- Cold storage warehouse

## Non-Goals

- Not cycle-exact at the gate level (we approximate timing)
- Not supporting all PDP-8 peripherals (only those needed for factory control)
- Not a general-purpose PDP-8 emulator (specialized for RTS-8 + factory control)

## Host Tools

- `python3 -m factory <rom-image.srec>` loads a Motorola S-record ROM into the emulator, installs a `JMP I 20` reset vector, and waits for `go` before running. (The module uses `factory/libpdp8.so`; build it with `make factory/libpdp8.so`.)
- Assemble PAL-style sources with `python3 tools/pdp8_asm.py program.asm program.srec`.
- Inspect ROM contents with `./tools/dump-rom program.srec`.
- Build the interactive monitor with `make monitor`; run `./monitor` for manual inspection and device poking.
- Run emulator microbenchmarks with `./tools/pdp8_bench [loop_count]` (default `50_000_000`). The tool times three tight loops—`NOP/JMP`, an auto-increment loop hitting address `0010`, and a `JMS`/Group 1 operate sequence within the 0o0100 budget—and reports Mloops/s and MIPS for each.
- Monitor commands mirror PDP-8 conventions: `dep` deposits consecutive words at an address, and `mem` displays dumps eight words per line.

## Peripherals

- Line printer peripheral sits on IOT device code `060`; use `6601` (skip if ready), `6602` (clear ready), and `6604` (print AC low 7 bits). Combine bits for multi-action sequences (`6606` for clear+print). Output goes to host stdout with a 132-column default width and CR/LF resetting the column counter.

## ROM Images

- ROM snapshots are stored as Motorola S-record files (`*.srec`) in little-endian byte order.
- Use `./tools/dump-rom <image.srec>` to print the decoded 12-bit words in octal, eight words per line.
- The monitor `save` and `restore` commands operate on raw 12-bit word dumps; convert as needed when moving between S-record files and monitor sessions.

## RTS-8 Features Used

- Interrupt-driven I/O: All device handlers use interrupts
- Priority scheduling: Higher priority for time-critical tasks
- Foreground/background: Fast control loops vs. housekeeping
- Device handlers: One handler per device code
- Timer services: 60Hz system clock
