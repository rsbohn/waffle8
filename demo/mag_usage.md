# mag.asm Usage Guide

## Overview
`mag.asm` demonstrates writing memory contents to magtape unit 1. It copies N words from memory starting at a specified address to the magtape.

## Configuration
- **ZP:20 (0020)**: Word count N (stored as negative value for ISZ loop)
- **ZP:17 (0017)**: Source address pointer (uses auto-increment feature)
- **Magtape Unit 1**: Output destination

## How It Works
1. Sets up count and source pointer
2. Loops through memory locations using auto-increment on ZP:17
3. Waits for magtape ready before each write
4. Writes each word to magtape unit 1
5. Writes completion marker (7777) when done

## Running the Demo

### Method 1: Use Built-in Test Data
```bash
./monitor demo/mag.srec
run
```

This will copy 5 test words (0001, 0002, 0003, 0004, 0005) to magtape unit 1.

### Method 2: Custom Parameters
```bash
./monitor demo/mag.srec

# Set custom word count (negative value)
write 0020 7773    # -5 to copy 5 words (or change as needed)

# Set custom source address  
write 0017 1000    # Start copying from address 1000 (or any address)

# Run the program
run
```

## Checking Results
After running, check the magtape status and output:
```bash
show magtape
```

The program creates a new file in the `magtape/` directory containing the copied words.

## Auto-Increment Feature
The program uses ZP:17 for the source pointer because:
- Locations 0010-0017 (octal) have auto-increment addressing
- `TAD I 0017` loads from the address in location 0017, then increments location 0017
- This automatically advances the pointer for the next iteration

## Program Structure
```assembly
START:      Initialize COUNT (ZP:20) and SRC_PTR (ZP:17)
COPY_LOOP:  ISZ COUNT (exit when zero)
WAIT_READY: IOT 6710 (wait for magtape ready)
            TAD I SRC_PTR (load word, auto-increment pointer)  
            IOT 6704 (write to magtape)
            Loop back
COPY_DONE:  Write completion marker and halt
```

## Customization
To copy different data:

1. **Change word count**: Modify `NEG_COUNT` or write new value to 0020
2. **Change source address**: Modify `SRC_ADDR` or write new address to 0017  
3. **Add your data**: Place data at the source address range
4. **Change completion marker**: Modify `DONE_MSG` if desired

## Example: Copy 10 Words from Address 2000
```bash
./monitor demo/mag.srec
write 0020 7770    # -10 words
write 0017 2000    # Start at address 2000
run
```

This will copy words from addresses 2000-2011 (octal) to magtape unit 1.