# Fruit Jam Monitor Port Brief

## Problem Statement

Port the PDP-8 monitor (`tools/monitor.c`) from the POSIX host environment to the Adafruit Fruit Jam (RP2350). The Fruit Jam build must drive video via the board’s DVI output, accept KL8E keyboard input from a USB keyboard, and reuse the existing emulator core without duplicating monitor logic.

## Desired Outcomes

- Shared monitor core that compiles for both POSIX and Fruit Jam targets.
- New Fruit Jam binary (`fruitjam_monitor.elf`) that runs under the Pico SDK toolchain, presents the monitor UI on a DVI text console, and accepts USB keyboard input routed into the KL8E emulation.
- Existing desktop monitor (`monitor`) continues to build and behave as it does today.

## Scope & Deliverables

1. **Platform Abstraction**
   - Introduce a `monitor_platform.h` API that hides host-specific I/O and timing concerns.
   - Implement two backends:
     - `monitor_platform_posix.c` (wraps current `pdp8.config` parsing, FILE-based console/printer I/O, and non-blocking stdin).
     - `fruitjam-board.c` (Fruit Jam runtime glue for DVI, USB keyboard, and any timers/flash resources).
   - Update `tools/monitor.c` to depend only on the abstraction and choose the board spec (`pdp8_board_host_simulator()` vs. `pdp8_board_adafruit_fruit_jam()`) via the platform hooks.

2. **Fruit Jam Text Console**
   - Allocate a frame buffer compatible with the Pico DVI engine (see Apple II emulator reference).
   - Provide functions such as:
     - `void fj_console_init(void);`
     - `void fj_console_putc(uint8_t ch);`
     - `void fj_console_flush(void);`
   - Support at least an 80×24 character grid using 7×9 (or similar) glyphs stored in flash.
   - Ensure line wrapping, CR/LF handling, and a simple attribute scheme (monochrome output is acceptable initially).

3. **USB Keyboard Integration**
   - Use TinyUSB to enumerate a USB HID keyboard.
   - Translate HID keycodes (including modifiers) into ASCII suitable for the KL8E device.
   - Feed input into the existing KL8E pending-input queue through a new platform callback (e.g., `void monitor_platform_enqueue_key(uint8_t ch);`).
   - Implement key-repeat behaviour with a timer hook if supported by the Apple II example.

4. **Timing & Main Loop**
   - Provide a platform timing hook to run `pdp8_api_run()` in bursts (≈10 K cycles) between DVI scanout servicing.
   - Ensure DVI and USB IRQ handlers remain responsive—follow the scheduling pattern in the Apple II example.

5. **Build Integration**
   - Extend the top-level `makefile` with:
     - Updated object lists shared by both monitor targets.
     - A `fruitjam_monitor.elf` target that links the Pico SDK, Fruit Jam board support, and the new platform shim.
   - Document any required Pico SDK setup or environment variables.

6. **Reference & Documentation**
   - Capture the Reload Apple II Fruit Jam references (Adafruit guide: https://learn.adafruit.com/apple-e-emulator-on-fruit-jam/install-emulator, source repo: https://github.com/adafruit/reload-emulator) and note the exact commit(s) consulted in comments or this doc.
   - Add inline documentation summarising the Fruit Jam-specific initialisation steps.

## Constraints & Considerations

- Stay within the RP2350 SRAM budget (default frame buffer size should fit alongside the emulator state).
- Follow existing coding style: C11, 4-space indentation, line length < 100 chars, `pdp8_api_*` naming conventions for public API.
- Avoid introducing POSIX headers into the Fruit Jam build.
- Prefer compile-time feature selection (`#ifdef MONITOR_PLATFORM_FRUITJAM`) over duplicated sources.
- Match the DVI resolution and timings used by the Apple II Fruit Jam port for maximum reuse.
- Implement `save` / `restore` commands as “not implemented” stubs when on Fruit Jam (host behaviour unchanged).
- Direct printer output to the on-screen console only on Fruit Jam (no external serial logging required initially).

## Testing Expectations

- `make monitor` still builds on the host; existing tests (`make -C tests && ./tests/pdp8_tests`) pass.
- Cross-compile `fruitjam_monitor.elf` with the Pico SDK toolchain (document the command).
- Provide a minimal Fruit Jam smoke test plan: e.g., booting to the monitor prompt, displaying `regs`, typing via USB keyboard, and verifying DVI output.
- If hardware is unavailable during development, supply a host-side stub or notes describing how to exercise the text console/keyboard layers.

## Open Questions

- Is additional logging (e.g., UART for debugging) desired alongside the on-screen console?
