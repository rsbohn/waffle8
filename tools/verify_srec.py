#!/usr/bin/env python3
"""
Verify SREC encoding at specific addresses
"""

def decode_srec_line(line):
    """Decode a single SREC line"""
    if not line.startswith('S1'):
        return None
    
    count = int(line[2:4], 16)
    address = int(line[4:8], 16)
    data_field = line[8:-2]  # Exclude checksum
    
    print(f"SREC: addr=0x{address:04X}, data_len={len(data_field)//2}")
    
    # Extract words
    words = {}
    for i in range(0, len(data_field), 4):
        if i + 3 < len(data_field):
            lo = int(data_field[i:i+2], 16)
            hi = int(data_field[i+2:i+4], 16)
            word_addr = (address + i // 2) // 2
            word_value = ((hi << 8) | lo) & 0x0FFF
            words[word_addr] = word_value
            
    return words

# Read the fixed SREC file
with open('demo/cal3_fixed.srec', 'r') as f:
    for line in f:
        line = line.strip()
        if line.startswith('S1'):
            words = decode_srec_line(line)
            if words and 0o211 in words:
                print(f"Found address 0211 (octal) = {0o211} (decimal)")
                print(f"Instruction at 0211: {words[0o211]:04o} (octal)")
                break