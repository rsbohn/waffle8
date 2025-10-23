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

This demonstrates reading structured data from paper tape and formatting it for human-readable output on the line printer.