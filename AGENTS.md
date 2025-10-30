# Repository Guidelines

## Project Structure & Module Organization
The PDP-8 core lives in `src/emulator`, which contains the board abstraction (`pdp8_board.h`), the CPU implementation (`main.c`), and board-specific wiring (`board.c`). Python-facing helpers belong in `src/factory`, currently a thin ctypes wrapper that expects a shared library named `libpdp8.so` alongside it. Host-facing assets, docs, and legacy experiments (for example the root `makefile` and `getpal.R`) stay at the repository top level. Use `tests/` for all executable regression checks; keep factory fixtures or data under clearly named subdirectories if you add them.

## Build, Test, and Development Commands
Compile a host library suitable for both tests and the Python wrapper with:
```bash
cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o libpdp8.so
```
For quick validation builds, rely on the existing test makefile:
```bash
make -C tests
./tests/pdp8_tests
```
Run these before opening a pull request. When developing new front ends, point them at the generated `libpdp8.so`. Avoid modifying the historical root `makefile` unless you fully modernize it.

## Coding Style & Naming Conventions
Follow the C11 profile enforced in the tests (`-std=c11 -Wall -Wextra -pedantic`). Use four-space indentation, brace-on-same-line, and keep lines under 100 characters. Public API symbols follow the `pdp8_api_*` prefix; board-specific helpers use `pdp8_board_*`. Reserve `ALL_CAPS` for constants and bit masks, mirroring existing definitions. In Python, stick to PEP 8 naming and type hints, keeping wrappers minimal and well-documented.

## Interrupt Support Conventions
The emulator implements a single hardware interrupt line model, matching real PDP-8 architecture. Devices request service via `pdp8_api_request_interrupt(cpu, device_code)`, which increments a pending counter. When interrupts are enabled (`ION` instruction), the CPU dispatches to ISR at 0x0010, saving AC/PC/LINK at 0x0006-0x0008. Device ISKs in the ISR poll all devices to determine which need service; the handler clears device flags and returns. Multiple simultaneous interrupts are handled by re-triggering the ISR after each device is serviced. See `docs/interrupt-implementation-plan.md` and `demo/interrupt-test.asm` for full implementation details and example code.

## Testing Guidelines
Extend `tests/test_emulator.c` with focused scenarios; mimic the existing ASSERT macros for consistency. Group new tests by PDP-8 feature or board behavior, and name helper functions `test_<feature>` so they are easy to find. Run `make -C tests clean` before committing generated binaries. There is no formal coverage gate, but aim to exercise new opcodes, device handlers, and error paths. If you add Python integration, provide a small script or doctest that drives the shared library.

## Commit & Pull Request Guidelines
Commit messages trend toward short, imperative summaries (for example “Add Fruit Jam board target…”). Keep the first line under 72 characters, describe the why in the body when needed, and reference issues as `Fixes #123`. PRs should include: a concise problem statement, implementation notes, test evidence (`make -C tests` output or similar), and screenshots only when UI changes occur. Tag reviewers familiar with either the emulator core or the factory integration depending on the surface you touched.
