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