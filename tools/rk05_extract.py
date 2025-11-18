#!/usr/bin/env python3
"""
Simple RK05 disk image extractor for OS/8 file systems.
This attempts to parse the OS/8 directory structure and extract files.
"""

import sys
from pathlib import Path
from typing import List, Optional, Tuple
import struct

# OS/8 filesystem constants
BLOCK_SIZE = 256  # 256 12-bit words per block
WORDS_PER_BLOCK = 256
DIRECTORY_BLOCK = 1  # Directory usually starts at block 1

def read_12bit_words(data: bytes, offset: int, count: int) -> List[int]:
    """Read 12-bit words from byte data, assuming little-endian packing."""
    words = []
    byte_offset = offset * 3 // 2  # Each word is 1.5 bytes
    
    for i in range(count):
        if byte_offset + 1 >= len(data):
            break
            
        # Read two bytes and extract 12-bit word
        if (offset + i) % 2 == 0:
            # Even word: bits 0-11 of first byte + bits 0-3 of second byte
            word = data[byte_offset] | ((data[byte_offset + 1] & 0x0F) << 8)
            byte_offset += 1
        else:
            # Odd word: bits 4-7 of first byte + bits 0-7 of second byte
            word = ((data[byte_offset] & 0xF0) >> 4) | (data[byte_offset + 1] << 4)
            byte_offset += 2
            
        words.append(word & 0xFFF)  # Mask to 12 bits
    
    return words

def decode_sixbit(word: int) -> str:
    """Decode a 12-bit word containing two SIXBIT characters."""
    char1 = (word >> 6) & 0x3F
    char2 = word & 0x3F
    
    def sixbit_to_char(val: int) -> str:
        if val == 0:
            return ' '
        elif 1 <= val <= 26:
            return chr(ord('A') + val - 1)
        elif 27 <= val <= 36:
            return chr(ord('0') + val - 27)
        elif val == 37:
            return '.'
        elif val == 38:
            return '$'
        else:
            return '?'
    
    return sixbit_to_char(char1) + sixbit_to_char(char2)

def parse_os8_filename(words: List[int], offset: int) -> Optional[Tuple[str, str]]:
    """Parse OS/8 filename from directory entry (3 words for name + extension)."""
    if offset + 2 >= len(words):
        return None
        
    # First 3 words contain the filename in SIXBIT
    name_chars = ""
    for i in range(3):
        name_chars += decode_sixbit(words[offset + i])
    
    # Split into name (first 6 chars) and extension (last 2 chars)
    name = name_chars[:6].rstrip()
    ext = name_chars[6:8].rstrip()
    
    return (name, ext) if name else None

def extract_basic_files(image_path: Path, output_dir: Path):
    """Extract BASIC files from RK05 disk image."""
    print(f"Reading disk image: {image_path}")
    
    with open(image_path, 'rb') as f:
        disk_data = f.read()
    
    print(f"Disk size: {len(disk_data)} bytes")
    
    # Try to parse directory starting at different blocks
    for dir_block in [1, 2, 6]:  # Common directory locations
        print(f"\nTrying directory at block {dir_block}...")
        
        # Read directory block
        block_offset = dir_block * BLOCK_SIZE
        dir_words = read_12bit_words(disk_data, block_offset, WORDS_PER_BLOCK)
        
        # Look for file entries
        files_found = 0
        for i in range(0, len(dir_words) - 5, 5):  # OS/8 directory entries are typically 5 words
            filename_info = parse_os8_filename(dir_words, i)
            if filename_info:
                name, ext = filename_info
                if name and (ext.upper() in ['BA', 'BAS'] or 'BASIC' in name.upper()):
                    print(f"Found potential BASIC file: {name}.{ext}")
                    files_found += 1
                elif name:
                    print(f"Found file: {name}.{ext}")
        
        if files_found > 0:
            print(f"Found {files_found} files in directory at block {dir_block}")
    
    # Also try scanning for ASCII text that looks like BASIC
    print("\nScanning for BASIC program text...")
    text_data = disk_data.decode('ascii', errors='ignore')
    
    basic_keywords = ['REM', 'PRINT', 'INPUT', 'FOR', 'NEXT', 'IF', 'THEN', 'GOTO', 'GOSUB', 'RETURN']
    lines = text_data.split('\n')
    
    potential_basic = []
    for i, line in enumerate(lines):
        line = line.strip().upper()
        if any(keyword in line for keyword in basic_keywords):
            # Check if it looks like a numbered BASIC line
            if line and (line[0].isdigit() or any(kw in line for kw in basic_keywords)):
                potential_basic.append((i, lines[i]))
    
    if potential_basic:
        print(f"\nFound {len(potential_basic)} potential BASIC program lines:")
        for line_num, line in potential_basic[:10]:  # Show first 10
            print(f"  {line_num}: {line.strip()}")
        
        if len(potential_basic) > 10:
            print(f"  ... and {len(potential_basic) - 10} more lines")
        
        # Try to extract coherent BASIC programs
        output_dir.mkdir(exist_ok=True)
        basic_file = output_dir / "extracted_basic.txt"
        
        with open(basic_file, 'w') as f:
            f.write("# BASIC program extracted from RK05 disk image\n\n")
            for _, line in potential_basic:
                f.write(line + '\n')
        
        print(f"\nExtracted BASIC program to: {basic_file}")

def main():
    if len(sys.argv) != 3:
        print("Usage: rk05_extract.py <rk05_image> <output_directory>")
        print("Example: rk05_extract.py disk/ock.rk05 extracted/")
        return 1
    
    image_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    
    if not image_path.exists():
        print(f"Error: {image_path} not found")
        return 1
    
    extract_basic_files(image_path, output_dir)
    return 0

if __name__ == "__main__":
    sys.exit(main())