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

## RTS-8 Features Used

- Interrupt-driven I/O: All device handlers use interrupts
- Priority scheduling: Higher priority for time-critical tasks
- Foreground/background: Fast control loops vs. housekeeping
- Device handlers: One handler per device code
- Timer services: 60Hz system clock