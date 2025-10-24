# Paper Tape to Line Printer Demo (ptprint.asm)

This PDP-8 assembly program demonstrates copying data from the paper tape device to the line printer. It reads 6-bit encoded characters from the configured paper tape file and prints them as ASCII text to the line printer.

## Usage

1. **Configure the paper tape** in `pdp8.config`:
   ```
   device paper_tape {
     iot = 667x
     image = tapes/lorem.tape
   }
   ```

2. **Load the program** at address 200 (octal):
   ```
   LOAD demo/ptprint.srec
   ```

3. **Run the program**:
   ```
   START 200
   ```

## What it does

The program:
- Selects block 1 from the paper tape
- Reads 12-bit words from the tape (each word contains two 6-bit characters)
- Converts 6-bit SIXBIT encoding to printable ASCII
- Prints each character to the line printer
- Handles spaces, letters (A-Z), and basic punctuation
- Stops when the tape block is exhausted

## Character Conversion

The program converts DEC SIXBIT encoding to ASCII:
- Value 0 → Space
- Values 1-26 → Letters A-Z  
- Other values → Converted to appropriate ASCII characters

## Error Handling

If no tape is available, the program prints "NO TAPE" and halts.

## Files

- `demo/ptprint.asm` - Source code
- `demo/ptprint.srec` - Assembled binary (S-record format)

## Example Output

When run with `tapes/lorem.tape`, the program will print the automotive lorem ipsum text:

```
THE TACOMA RESPONDS AERODYNAMIC PRECISE STEERING HORSEPOWER...
```

## Printing a Calendar with `ptprint`

You can feed calendar text to the line printer by converting it to the same SIXBIT tape format:

1. Generate a tape image:
   ```bash
   python3 tools/generate_calendar_tape.py 2025 --month 1
   ```
   This writes `tapes/calendar-2025-01.tape`. Omitting `--month` creates the full-year layout.
2. Update `pdp8.config` so the paper tape device points at the generated file:
   ```ini
   device paper_tape {
     iot = 667x
     image = tapes/calendar-2025-01.tape
   }
   ```
3. Load and run `demo/ptprint.srec` as usual. The January 2025 calendar will spill onto the line printer.

This demonstrates reading structured data from paper tape and formatting it for human-readable output on the line printer.
