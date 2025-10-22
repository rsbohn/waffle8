# CircuitPython 10 Deployment Guide

Use this cheat sheet when pushing PDP-8 sketches from Linux (or WSL) to an Adafruit CircuitPython 10.x board such as the ItsyBitsy RP2040.

## Prerequisites

- CircuitPython 10.0.3 or newer already flashed. Confirm with the REPL banner.
- Board mounted under `/mnt/<drive>` (e.g. `/mnt/g`) so the host can write to it.
- Disco CLI available at `/opt/disco/.venv/bin/dtool`.
- Python files in this repository built for CircuitPython’s limited standard library (see below).

## Workspace Layout Tips

- CircuitPython executes `code.py` at boot. Keep experiments there or `import` helper modules that live next to it (for example `pdp8.py`).
- Avoid mixing a single-file module (`pdp8.py`) with a package directory of the same name (`pdp8/`)—CircuitPython will prioritise the directory and hide the file’s attributes.
- Keep third-party libraries inside `lib/`; CircuitPython’s import search path is shallow, so a flat layout reduces surprises.

## Python 3 Subset Constraints

CircuitPython 10 omits several CPython modules to save flash space. In practice:

- Static typing helpers such as `typing`, `typing_extensions`, `dataclasses`, and `pathlib` are unavailable. Stick to basic containers (lists, dicts, tuples) and simple duck typing.
- No `enum`, `functools.lru_cache`, or `contextlib`. Recreate light helpers inline when needed.
- Keep memory usage low: prefer module-level singletons over large class hierarchies, and avoid list comprehensions that build temporary copies of large buffers.

When adapting desktop Python code, strip annotations or guard them behind `try/except ImportError` blocks that default to simple aliases.

## Deploying Files with `dtool`

The Disco CLI streamlines copying files over USB:

1. Detect boards:

   ```bash
   /opt/disco/.venv/bin/dtool scan
   ```

2. Inspect the filesystem (optional but handy when confirming the mount point):

   ```bash
   /opt/disco/.venv/bin/dtool list-files
   ```

3. Push a file:

   ```bash
   /opt/disco/.venv/bin/dtool write-file sketch/pdp8.py pdp8.py
   ```

   - Use `--device /mnt/g` if multiple boards are attached.
   - WSL mounts may require escalated permissions; retry with elevated privileges if you see `Permission denied`.
   - `dtool` writes the file atomically and triggers the board’s auto-reload.

## Interacting from the REPL

- After copying, the board usually soft-resets and runs `code.py`. If you are testing a helper module, import it manually:

  ```python
  import pdp8
  pdp8.demo()
  ```

- During iterative development, reload an already-imported module without cycling power:

  ```python
  import importlib
  import pdp8
  importlib.reload(pdp8)
  ```

- Keep an eye on stdout; syntax or runtime errors appear there immediately after the auto-reload.

## Troubleshooting

- **`ImportError: no module named 'typing'`** — remove type-hint imports or provide local stubs; the module is not bundled with CircuitPython.
- **`AttributeError` after pruning a package directory** — confirm no stale subdirectory remains on the board; delete it before copying the flat module.
- **No device found** — the board may still be booting. Reconnect USB, rerun `dtool scan`, or push the reset button twice to enter UF2 mode and return to normal operation.
- **Writes fail intermittently** — verify the drive is not mounted read-only. Eject/unmount cleanly before unplugging the device to prevent filesystem corruption.

Refer back to this guide whenever upgrading CircuitPython or onboarding a new workstation so deployments stay smooth.
