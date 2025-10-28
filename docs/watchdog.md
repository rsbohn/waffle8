# Watchdog Timer (emulator device)

This document describes the Watchdog Timer device implemented in the PDP‑8 emulator.

Overview
- The watchdog is a wall-clock countdown timer that guest code can control via IOT.
- When the countdown reaches zero the device performs one of the configured actions (RESET or HALT), and may be one-shot or periodic.

Control register (12 bits)
- Bits 11..9: CMD (3 bits)
  - 000 = disabled
  - 001 = RESET (one-shot)
  - 010 = RESET (periodic)
  - 011 = HALT (one-shot)
  - 100 = HALT (periodic)
- Bits 8..0: COUNT (9 bits) — countdown value in deciseconds (0..511). 0 means disabled / no countdown.

IOT interface
- IOT microbits mapping implemented by the device:
  - 01 (WRITE): write AC into the control register. Writing starts/stops the countdown depending on CMD/COUNT.
  - 02 (READ):  read the control register into AC.
  - 04 (RESTART): reload the active counter from the COUNT field and resume ticking.

Config stanza (pdp8.config)
Add a `device watchdog` block to `pdp8.config` to expose and configure the watchdog at monitor startup. Example:

device watchdog {
  iot = 055x           # documented device code (optional, informational)
  enabled = true
  mode = "halt"       # string: "halt" or "reset" (informational)
  periodic = false
  default_count = 5    # default countdown in deciseconds
  pause_on_halt = true # if true, the monitor will not advance the watchdog while the CPU is halted
}

Notes on semantics
- Timing uses the host's monotonic clock. COUNT is interpreted as deciseconds and converted to nanoseconds internally.
- The emulator core provides a tick-callback API devices can register. The watchdog registers a tick handler invoked on each CPU step and checks for expiry there.
- `pause_on_halt` is parsed and available; the current implementation avoids advancing the counter while the CPU is halted in common usage, but precise paused-time accounting can be improved if needed.
- Interrupt-mode is not implemented; choosing interrupt in the `mode`/CMD currently falls back to RESET. Implementing true device interrupts requires wiring into the core interrupt priority system.

Developer notes
- When adding this device the build targets were updated so monitor and shared library builds include `watchdog.c`.
- POSIX feature test macros are used in a few files to ensure `clock_gettime()` and `mkdtemp()` are declared.

Testing
- `factory/test_watchdog.py` contains ctypes-based tests that exercise write/read, one-shot HALT, and one-shot RESET behavior against `factory/libpdp8.so`.
- `tests/test_config.c` verifies parsing of the `pdp8.config` watchdog stanza and includes tests for invalid configurations (missing stanza and out-of-range values).
-
Factory runner wiring
- The Python `factory/driver.py` runner can create and attach the watchdog when a `device watchdog { ... }` stanza is present in `pdp8.config`.
- The driver initializes the watchdog control register by issuing an `IOT` from Python: it sets AC, writes an IOT instruction into address 0 and executes a single step. This avoids adding a new C helper API.
- The driver checks for the presence of `pdp8_watchdog_create` in the shared library and falls back gracefully if the device isn't available in an older build.

Assembler notes and symbolic IOTs
- The project's assembler supports labels and resolves symbols to addresses. It also accepts `IOT <octal>` syntax.
- Important note: defining a symbol as a data word and then using `IOT SYMBOL` will assemble an IOT whose operand is the symbol's address (not the numeric octal constant). Example:
  - `WD_WRITE, 06551` followed by `IOT WD_WRITE` results in an IOT coded with the address of `WD_WRITE`, not `06551`.
- Recommended approaches:
  - Use `IOT 06551` (explicit octal literal) when you mean the IOT opcode 06551.
  - If you want a readable symbolic name that does not allocate memory, we can add an `EQU` directive to the assembler so symbols can represent numeric constants without occupying a word. (Not yet implemented.)

Demos and examples
- `demo/hello-wd.asm` shows a minimal program that writes the watchdog control register and waits for HALT.
- `demo/dull-boy.asm` demonstrates refreshing the watchdog from a long-running guest program. Lessons learned:
  - Initialize the watchdog control register before entering long loops so `RESTART` IOTs refresh an armed timer.
  - Refresh the watchdog frequently enough during long operations (for example, after each printed character) to avoid unintended expiry when delays exceed the watchdog period.
  - Use `IOT <octal>` forms to clearly express IOT ops with octal constants; the assembler marks these as IOT entries.

Next actions (optional)
- Implement true interrupt delivery for the watchdog instead of mapping to RESET.
- Harden config parsing and return clear errors on invalid keys/values.
- Add an integration test that starts the monitor binary, attaches the watchdog, and verifies runtime behavior end-to-end.

