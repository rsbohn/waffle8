#!/usr/bin/env python3
"""
RK05 disk image scanner - extract readable content without full OS/8 parsing.
This tool scans for patterns that look like BASIC programs, text files, or other content.
"""

import sys
import re
from pathlib import Path
from typing import List, Tuple, Optional

def extract_ascii_regions(data: bytes, min_length: int = 20) -> List[Tuple[int, str]]:
    """Extract regions of printable ASCII text from binary data."""
    ascii_regions = []
    current_text = ""
    current_offset = 0
    
    for i, byte in enumerate(data):
        if 32 <= byte <= 126 or byte in [9, 10, 13]:  # Printable ASCII + tab/newline/CR
            if not current_text:
                current_offset = i
            current_text += chr(byte)
        else:
            if len(current_text) >= min_length:
                ascii_regions.append((current_offset, current_text))
            current_text = ""
    
    if len(current_text) >= min_length:
        ascii_regions.append((current_offset, current_text))
    
    return ascii_regions

def find_basic_programs(text: str) -> List[Tuple[int, str]]:
    """Find potential BASIC program segments in text."""
    basic_patterns = [
        r'\b\d+\s+(REM|PRINT|INPUT|FOR|NEXT|IF|THEN|GOTO|GOSUB|RETURN|LET|DIM)\b',
        r'\bPRINT\s+["\']',
        r'\bFOR\s+\w+\s*=',
        r'\bIF\s+.+\s+THEN\b',
        r'\bGOTO\s+\d+',
        r'\bGOSUB\s+\d+',
    ]
    
    matches = []
    lines = text.split('\n')
    
    for i, line in enumerate(lines):
        line = line.strip().upper()
        if any(re.search(pattern, line, re.IGNORECASE) for pattern in basic_patterns):
            matches.append((i, line))
    
    return matches

def find_file_patterns(data: bytes) -> List[Tuple[int, str, str]]:
    """Look for common file patterns and headers."""
    patterns = []
    
    # Look for common file extensions in SIXBIT or ASCII
    file_patterns = [
        (rb'\.BA[SC]?', 'BASIC file'),
        (rb'\.FT[HN]?', 'FORTRAN file'),
        (rb'\.PA[LS]?', 'PAL assembly file'),
        (rb'\.SV', 'Save file'),
        (rb'\.BI[NG]?', 'Binary file'),
        (rb'\.TX[T]?', 'Text file'),
    ]
    
    for pattern, description in file_patterns:
        for match in re.finditer(pattern, data, re.IGNORECASE):
            offset = match.start()
            context = data[max(0, offset-20):offset+20]
            patterns.append((offset, description, context.decode('ascii', errors='replace')))
    
    return patterns

def scan_for_program_numbers(text: str) -> bool:
    """Check if text contains numbered program lines (BASIC style)."""
    lines = text.split('\n')
    numbered_lines = 0
    
    for line in lines:
        line = line.strip()
        if re.match(r'^\d+\s+\w+', line):
            numbered_lines += 1
    
    return numbered_lines > 3  # Threshold for likely program

def main():
    if len(sys.argv) < 2:
        print("Usage: rk05_scan.py <rk05_image> [output_dir]")
        print("Example: rk05_scan.py disk/ock.rk05 extracted/")
        return 1
    
    image_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("extracted")
    
    if not image_path.exists():
        print(f"Error: {image_path} not found")
        return 1
    
    print(f"Scanning RK05 image: {image_path}")
    print(f"Size: {image_path.stat().st_size} bytes")
    
    with open(image_path, 'rb') as f:
        disk_data = f.read()
    
    # Extract ASCII regions
    print("\n=== Extracting ASCII text regions ===")
    ascii_regions = extract_ascii_regions(disk_data)
    print(f"Found {len(ascii_regions)} ASCII text regions")
    
    output_dir.mkdir(exist_ok=True)
    
    # Analyze each region for BASIC programs
    basic_programs = []
    for i, (offset, text) in enumerate(ascii_regions):
        print(f"\nRegion {i+1} at offset {offset:06x} ({len(text)} chars):")
        
        # Show first few lines
        lines = text.split('\n')[:5]
        for line in lines:
            print(f"  {line[:80]}")
        if len(text.split('\n')) > 5:
            print(f"  ... ({len(text.split('\n'))} total lines)")
        
        # Check for BASIC patterns
        basic_matches = find_basic_programs(text)
        if basic_matches:
            print(f"  -> Found {len(basic_matches)} BASIC-like lines")
            if scan_for_program_numbers(text):
                print(f"  -> Appears to be a numbered program!")
                basic_programs.append((i, offset, text))
        
        # Save region to file
        region_file = output_dir / f"region_{i+1:03d}_offset_{offset:06x}.txt"
        with open(region_file, 'w', encoding='utf-8', errors='replace') as f:
            f.write(f"# ASCII region {i+1}\n")
            f.write(f"# Offset: 0x{offset:06x} ({offset})\n")
            f.write(f"# Length: {len(text)} characters\n")
            f.write(f"# Lines: {len(text.split('\\n'))}\n\n")
            f.write(text)
    
    # Look for file patterns
    print(f"\n=== Scanning for file patterns ===")
    file_patterns = find_file_patterns(disk_data)
    if file_patterns:
        print(f"Found {len(file_patterns)} potential file references:")
        for offset, description, context in file_patterns:
            print(f"  {offset:06x}: {description} - {repr(context)}")
    else:
        print("No obvious file patterns found")
    
    # Extract potential BASIC programs
    if basic_programs:
        print(f"\n=== Extracting BASIC programs ===")
        for i, (region_num, offset, text) in enumerate(basic_programs):
            basic_file = output_dir / f"basic_program_{i+1}.bas"
            print(f"Writing potential BASIC program to: {basic_file}")
            
            with open(basic_file, 'w') as f:
                f.write(f"REM BASIC program extracted from RK05 image\n")
                f.write(f"REM Source: {image_path}\n")
                f.write(f"REM Region: {region_num+1}, Offset: 0x{offset:06x}\n")
                f.write(f"REM\n")
                
                # Try to clean up the program
                lines = text.split('\n')
                for line in lines:
                    line = line.strip()
                    if line and (line[0].isdigit() or any(kw in line.upper() for kw in ['REM', 'PRINT', 'INPUT', 'FOR', 'NEXT', 'IF', 'GOTO'])):
                        f.write(line + '\n')
    
    print(f"\nExtraction complete. Files saved to: {output_dir}")
    if basic_programs:
        print(f"Found {len(basic_programs)} potential BASIC programs!")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())