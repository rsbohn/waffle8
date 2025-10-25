# webdp8 — HTTP front-end for the PDP-8 emulator

This document describes the lightweight Flask-based web frontend implemented in
`tools/webdp8.py`. The server exposes a small REST API to inspect registers,
read/write memory, load Motorola S-records (S1/S9 style), run the CPU, and
capture console / line-printer output.

## Quick start

Prerequisites:

- A built native emulator shared library `libpdp8.so` in the `factory/` folder.
  Build it from the repository root with:

```bash
cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o factory/libpdp8.so
```

- Python 3.8+ (the tools use ctypes and Flask). Install Flask if you don't have
  it already:

```bash
python3 -m pip install flask
```

Start the server from the project root:

```bash
python3 tools/webdp8.py
```

The server listens on 0.0.0.0:5000 by default. You can use `curl` to interact
with it (examples below).

## Endpoints

All values returned are formatted as 4-digit octal strings where appropriate.

### GET /regs

Returns the current CPU registers and the local cycle counter.

Response JSON:

```json
{
  "pc": "0100",
  "ac": "0000",
  "link": 0,
  "cycles": 0
}
```

### GET /mem?addr=<octal|decimal>&len=<n>

Read `len` words starting from `addr`. `addr` may be an octal string (leading
`0`) or decimal integer.

Response JSON:

```json
{
  "start": "0100",
  "words": [ {"addr":"0100","val":"7200"}, ... ]
}
```

### PUT /mem

Write memory. Accepts JSON body in one of two forms:

1. Single word: `{ "addr": "0200", "val": "7001" }`
2. Block write: `{ "start": "0600", "values": ["03751","0012"] }`

Response: `{ "written": [ {"addr":"0600","val":"3751"}, ... ] }`

### POST /loader

Upload a Motorola S-record (S1/S9 style). Accepts a multipart file field named
`file` or raw body. The loader uses `factory.driver.load_srec` to parse the
S-record and writes decoded 12-bit words into emulator memory. If the SREC
contains a start address (S9/S8/S7) the PC is set to that address.

Example:

```bash
curl -F "file=@demo/cal3.srec" http://127.0.0.1:5000/loader
```

Response includes a `written` list of `(addr,val)` and a `start` field with the
start address (if provided).

### GET /trace?start=<pc>&cycles=<n>

Run the CPU for up to `cycles` steps (clamped to 1..1024). If `start` is
provided the PC will be set before tracing. The endpoint attempts to clear any
previous HALT condition before tracing so it will resume execution.

Response JSON contains `begin_pc`, an array of `steps` and `halted`.

Each step record contains:

- step (0-based index), pc_before, instr (octal), pc_after, ac_before/after,
  link_before/after, halted boolean (true when the step returned 0 from the
  native step API).

### GET /output/printer

Return incremental output written by the native line-printer device. The
implementation opens a temporary file which the C line-printer writes into.
Each call returns only the new bytes written since the last call. Response is
JSON with `text` (decoded UTF-8, ANSI color sequences may be present) and
`bytes` (array of octal strings).

Note: this endpoint currently returns deltas (new content since last read).

### GET /output/teleprinter

Return teleprinter output captured by the KL8E console. If the server was able
to attach the KL8E console with a file-backed output stream this endpoint will
read the temp file (and advance the internal read pointer). If not, it will
try to pop the console internal output buffer via the C API.

Optional query parameter: `?peek=1` — when reading from the file-backed
teleprinter this will read without advancing the internal pointer (non-
destructive peek).

Response JSON: `{ "text": "...", "bytes": ["033","133",...] }`

### POST /input/keyboard

Queue keyboard characters into the KL8E console input buffer.

Request JSON: `{ "chars": "Hello\n" }`

Response: `{ "queued": <n> }` number of characters accepted.

### Notes about switch register (S)

The current `tools/webdp8.py` does not (yet) expose an endpoint for setting the
PDP-8 switch register. To run programs that read the S register (via OSR), you
have two options:

1. Write the desired value into a memory word the program reads (if the
   program uses memory-based inputs), or
2. Modify `tools/webdp8.py` to add a small endpoint that calls
   `pdp8_api_set_switch_register(cpu, value)`. This is a small change I can
   make if you want remote control of the S register.

## Implementation notes

- The server prefers `./factory/libpdp8.so` as the native emulator library. If
  you copy the shared object elsewhere, update the loader path accordingly.
- The S-record loader expects little-endian word byte pairs (the project's
  assembler `tools/pdp8_asm.py` emits compatible S1 records).
- The printer / teleprinter capture is implemented using temporary files opened
  with the C `fopen` (so the native C peripherals can write to the stream).

## Examples

Load and run `demo/cal3.srec`, then fetch the printer output (simple):

```bash
curl -F "file=@demo/cal3.srec" http://127.0.0.1:5000/loader
curl -X PUT -H 'Content-Type: application/json' -d '{"start":"0600","values":["03751","0012"]}' http://127.0.0.1:5000/mem
curl "http://127.0.0.1:5000/trace?start=0100&cycles=1024"
curl "http://127.0.0.1:5000/output/printer"
```

## Troubleshooting

- If IOT instructions (e.g. 6601) appear to spin in a busy loop, verify the
  console/printer devices are attached. The server attempts to attach them at
  startup, but if the native library is missing symbols or `fopen` fails the
  server will continue without devices attached and the IOTs will be
  no-ops.
- If `/loader` reports write failures, confirm `factory/libpdp8.so` exists and
  that the S-record is well-formed (use `tools/pdp8_asm.py --list` to inspect
  assembly and SREC output).

## Contact / next steps

If you'd like, I can:

- Return the entire printer log instead of deltas.
- Provide an HTML UI for interactive stepping and output viewing.

Pick one and I'll implement it.
