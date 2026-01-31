# Monitor Stack Operations Plan

## Goal
Add optional monitor commands that make it easy to inspect and manipulate a
software stack used by PDP-8 programs. Because the PDP-8 has no hardware stack,
these commands operate on memory locations and a monitor-managed stack pointer.

## Constraints
- The stack lives in emulator RAM; there is no CPU stack register to mirror.
- Many programs use auto-increment locations (0010-0017) as a stack pointer.
- Commands must not assume a specific program layout; defaults must be
  configurable and safe.

## Proposed Command Surface
Introduce a new top-level command: `stack`.

- `stack` or `stack status`
  - Show current stack configuration and a short preview of the top entries.
- `stack config sp=<addr> base=<addr> limit=<addr> dir=<up|down>`
  - Configure the stack pointer value and bounds.
  - `sp` is the current top-of-stack address tracked by the monitor.
  - `base`/`limit` are optional bounds for guard checks.
  - `dir` controls whether pushes increment or decrement the pointer.
- `stack peek [count]`
  - Display `count` entries from the top without modifying memory.
- `stack push <w0> [w1 ...]`
  - Write words onto the stack and update the pointer accordingly.
- Bare octal input at the monitor prompt pushes the value onto the stack using
  the current stack configuration.
- `.` (single dot)
  - Pop and print the top of the stack. Emits an error on underflow.
- `stack pop [count]`
  - Read and remove `count` entries, updating the pointer.
- `stack reset <addr>`
  - Initialize the pointer to a known value (typically the empty-stack value).

## Default Behavior
- Default stack region: `00170-00177` (8 words) in guest memory.
- Default `sp`: empty (one step before `base` for `up` stacks).
- Default direction: `up` (increment before use) to match `DCA I SP` patterns.
- Default bounds: `base=00170`, `limit=00177` unless configured by `stack config`.

## Implementation Notes (Monitor)
- Store stack configuration in `monitor_runtime` (not in the emulator core).
- Use existing memory access helpers (same ones as `mem`/`dep`).
- Enforce bounds when provided and print a clear error when exceeded.
- Do not attempt to decode call frames; treat entries as raw words.
- On stack overflow or underflow, print an error message and discard the rest of
  the current user input.

## Examples (Intended)
```
stack config base=00170 limit=00177 dir=up
stack status
stack push 07777 01234
stack peek 4
stack pop 2
```

## Tests/Validation
- Add monitor transcript tests once the commands exist (driver-based).
- Validate push/pop on both `up` and `down` directions.
- Ensure bounds errors do not modify memory.

## Open Questions
- Should `stack` default to the auto-increment page when explicitly requested?
