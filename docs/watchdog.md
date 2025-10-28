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

Next actions (optional)
- Implement true interrupt delivery for the watchdog instead of mapping to RESET.
- Harden config parsing and return clear errors on invalid keys/values.
- Add an integration test that starts the monitor binary, attaches the watchdog, and verifies runtime behavior end-to-end.

