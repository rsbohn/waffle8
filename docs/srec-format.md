# PDP-8 S-record Format Specification

## Requirements

Generate an S-record file for PDP-8 with the following encoding rules:

### Address Encoding
- S-record addresses are in **hexadecimal**
- PDP-8 addresses (octal) must be converted to hex
- Example: PDP-8 address 0200 (octal) = 128 (decimal) = 0x0080 (hex in S-record)

### Data Encoding  
- Each PDP-8 word is 12 bits
- Encoded as **16-bit little-endian** in the S-record
- The word is masked with 0x0FFF when loaded
- Example: PDP-8 word 07200 (octal) = 3712 (decimal) = 0x0E80 (hex) → bytes `80 0E`

### S-record Structure
- **S0**: Header record (optional, contains filename)
- **S1**: Data records with 16-bit hex address
- **S5**: Record count (optional)
- **S9**: Start address (16-bit hex)

### Format
```
S1<bytecount><address><data_bytes><checksum>
```

Where:
- bytecount = number of bytes following (address + data + checksum)
- address = 4 hex digits (16-bit)
- data_bytes = pairs of hex digits (little-endian 16-bit words)
- checksum = one's complement of sum of all bytes (address + data + bytecount)

## Example Program

PDP-8 Assembly (octal addresses and values):
```
*0200
    CLA          / 7200
    TAD 0225     / 1225  
    IOT 06551    / 6551
    CLA          / 7200
    TAD 0226     / 1226
    TPC          / 6044
    HLT          / 7402
0225: 01036      / Watchdog control
0226: 0102       / 'B' character
```

Conversion to hex for S-record:
- Address 0200 oct = 128 dec = 0x0080
- Word 7200 oct = 3712 dec = 0x0E80 → bytes: 80 0E
- Word 1225 oct = 661 dec = 0x0295 → bytes: 95 02
- Word 6551 oct = 3433 dec = 0x0D69 → bytes: 69 0D
- Word 7200 oct = 3712 dec = 0x0E80 → bytes: 80 0E
- Word 1226 oct = 662 dec = 0x0296 → bytes: 96 02
- Word 6044 oct = 3108 dec = 0x0C24 → bytes: 24 0C
- Word 7402 oct = 3842 dec = 0x0F02 → bytes: 02 0F
- Address 0225 oct = 149 dec = 0x0095
- Word 01036 oct = 542 dec = 0x021E → bytes: 1E 02
- Address 0226 oct = 150 dec = 0x0096  
- Word 0102 oct = 66 dec = 0x0042 → bytes: 42 00

## Task

Please generate a correct S-record file for the BBB printer program with watchdog:

```assembly
*0200
    CLA              / Clear AC
    TAD WDCTL        / Load watchdog control
    IOT 06551        / Start watchdog
    CLA              / Clear AC
    TAD CHR1         / Load 'B'
    TPC              / Print (IOT 06044)
    CLA
    TAD CHR2         / Load 'B'
    TPC
    CLA
    TAD CHR3         / Load 'B'
    TPC
    CLA
    TAD CR           / Load CR
    TPC
    CLA
    TAD LF           / Load LF
    TPC
    HLT              / Halt
WDCTL: 01036         / at 0225: Watchdog control (3 sec reset)
CHR1:  0102          / at 0226: 'B'
CHR2:  0102          / at 0227: 'B'
CHR3:  0102          / at 0230: 'B'
CR:    0015          / at 0231: carriage return
LF:    0012          / at 0232: line feed
```

All addresses and values above are in octal. Convert them properly to hex with little-endian encoding as specified.