# How to Use the Magtape Device

## Overview
The magtape device provides magnetic tape storage for the PDP-8 emulator with two units:
- **Unit 0**: Read-only access to files in the `demo/` directory
- **Unit 1**: Write access to the `magtape/` directory (for output/storage)

## Device Configuration
The magtape is configured in `pdp8.config`:
```
device magtape0 {
  path = demo
  unit = 0
  write_protected = true
}

device magtape1 {
  path = magtape
  unit = 1
  write_protected = false
}
```

## IOT Instructions
The magtape uses device code **070** (octal) with these operations:

| Octal | Binary | Name    | Description                           |
|-------|--------|---------|---------------------------------------|
| 6701  | 001    | GO      | Start/continue operation              |
| 6702  | 010    | READ    | Read data from tape to AC             |
| 6704  | 100    | WRITE   | Write data from AC to tape            |
| 6710  | 001000 | SKIP    | Skip if tape ready                    |
| 6720  | 010000 | REWIND  | Rewind tape to beginning              |
| 6740  | 100000 | SENSE   | Read status into AC                   |

## Monitor Commands

### View Magtape Status
```
show magtape
```
Shows all configured magtape units with:
- Unit number and path
- Current record (file) and position
- Ready/error status
- Write protection status

### Control Magtape Units
```
magtape rewind <unit>    # Rewind unit to beginning
magtape new <unit>       # Start new record (writable units only)
```

Examples:
```
magtape rewind 0         # Rewind demo unit to first file
magtape rewind 1         # Rewind magtape unit to first record
magtape new 1            # Start new output file on unit 1
```

## Programming Examples

### Reading from Demo Unit (Unit 0)
```assembly
        / Read a word from magtape unit 0
READ_WORD,
        IOT 6710        / Skip if ready
        JMP READ_WORD   / Wait until ready
        IOT 6702        / Read word into AC
        / AC now contains the word from tape
```

### Writing to Magtape Unit (Unit 1)
```assembly
        / Write a word to magtape unit 1
WRITE_WORD,
        IOT 6710        / Skip if ready
        JMP WRITE_WORD  / Wait until ready
        TAD DATA        / Load data to write
        IOT 6704        / Write AC to tape
```

### Complete Read Loop
```assembly
        / Read all records from demo unit
        IOT 6720        / Rewind to beginning
READ_LOOP,
        IOT 6710        / Skip if ready
        JMP READ_DONE   / Not ready = end of tape
        IOT 6702        / Read word
        / Process word in AC here
        JMP READ_LOOP
READ_DONE,
        / End of tape reached
```

## Usage Workflow

### 1. Reading Demo Files
```bash
# Start monitor
./monitor

# Check magtape status
show magtape

# Your PDP-8 program can now read from demo/ files
# Files are presented as sequential records
#
# Sample reels included in this repository:
#   magtape/demo-hello-ascii.tap   (ASCII header + ASCII payload)
#   magtape/demo-hello-sixbit.tap  (SIXBIT header + SIXBIT payload)
```

### 2. Writing Output Files
```bash
# Start monitor
./monitor

# Check status
show magtape

# Your PDP-8 program writes to unit 1
# Each write session creates a new file in magtape/

# Start new output file
magtape new 1

# Files will be created as record-YYYYMMDD-HHMMSS.tap
```

## File Format
- **Demo files**: Raw binary files treated as 12-bit word sequences
- **Output files**: Created in `magtape/` with timestamped names
- **Records**: Each file represents one tape record
- **End conditions**: End-of-record when file ends, end-of-tape after last file

### Included Demo Records

| File                                 | Label   | Format | Payload summary        |
|--------------------------------------|---------|--------|------------------------|
| `magtape/demo-hello-ascii.tap`       | ASCII0  | ASCII  | “Hello from ASCII tape” |
| `magtape/demo-hello-sixbit.tap`      | SIXBIT  | SIXBIT | “SIXBIT DEMO 01”       |

### Record Header Layout
Every magtape record begins with a 6-word header encoded in SIXBIT:

1. Three words for the six-character label (two SIXBIT characters per word)
2. Three words for the six-character data format field

Common format values:
- `SIXBIT` – payload contains pairs of SIXBIT characters packed into 12-bit words
- `ASCII` – payload contains 8-bit characters stored in 12-bit words (low byte in bits 0–7)

After the header comes the payload words followed by the 0xFFFF sentinel. The first 16-bit word
of the file is the declared payload length (header + body, excluding the sentinel).

### Inspecting Records from the Host
Use `tools/magtape_tool.py` to explore `.tap` files:

```bash
# Show labels/data formats without touching the payload
python3 tools/magtape_tool.py inspect magtape/demo-header.tap --head

# Dump 12-bit words in octal
python3 tools/magtape_tool.py inspect magtape/demo-header.tap --raw

# Decode according to the header (SIXBIT pairs or ASCII bytes)
python3 tools/magtape_tool.py inspect magtape/demo-header.tap --decode
```

`--decode` prints each payload word alongside a readable representation and includes the combined
text stream for convenience.

## Available Demo Files
Unit 0 provides access to all files in the `demo/` directory:
- `cal.srec` - Calendar program
- `core.srec` - Core system 
- `fib.srec` - Fibonacci demo
- `kl8e_echo.srec` - KL8E echo demo
- `mcopy.srec` - Memory copy routine
- And others...

## Status Bits (SENSE command)
The SENSE operation returns status in the AC:
- Bit 0: Ready flag
- Bit 1: End of record
- Bit 2: End of tape  
- Bit 3: Error condition
- Bit 4: Write protected (unit 0)

## Tips
1. Always check ready status before operations
2. Use `show magtape` to monitor current position
3. Unit 0 is read-only (write operations will error)
4. Unit 1 creates new files - use `magtape new 1` between logical records
5. Rewind units as needed to restart from beginning

### Sample Host Tapes
- `magtape/demo-header.tap`: SIXBIT payload with label `MTDEMO` and data format `SIXBIT`
- `magtape/demo-hello-ascii.tap`: ASCII payload with label `ASCII0` and the text `Hello from ASCII tape`

### Web UI Integration
`tools/webdp8.py` now creates a writable magtape controller at startup and mounts
`./magtape` as unit 0. Launch the web UI (`python3 tools/webdp8.py`) and use
`demo/mt.asm` plus the new host samples to experiment with headers and payloads
directly from the browser.

### PDP-8 Utility: `demo/mt.asm`
The `demo/mt.asm` program runs on the PDP-8 console to inspect the first record on
magtape unit 0. After assembling (`python3 tools/pdp8_asm.py demo/mt.asm`) and
loading the resulting image in the monitor or web UI:

1. Ensure the KL8E teleprinter is attached (the monitor and web UI do this by default).
2. Set the switch register to the desired unit number (octal 0-7) before running; the
   program samples `S` to pick a unit (`switch 1` selects unit 1, etc.).
2. Run the program at address `0200` (octal).
3. The utility rewinds unit 0, reads the six-word header, and prints the decoded label
   and data format via the teleprinter.

If the first record lacks a SIXBIT header (legacy `.srec` payloads), the utility reports
that condition instead of printing garbage.

This is handy for confirming headers directly from within the emulator before
inspecting payload records with host tooling.
