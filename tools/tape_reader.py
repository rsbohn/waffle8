#!/usr/bin/env python3
"""
PDP-8 Papertape Reader

Reads and displays the contents of PDP-8 papertape files in human-readable format.
Supports both SIXBIT (binary) and ASCII (octal) tape formats with auto-detection.
"""

import sys
import argparse
import os

def six_bit_to_ascii(bit_string):
    """Convert 6-bit binary string to ASCII character using DEC SIXBIT encoding."""
    if len(bit_string) != 6:
        return '?'
    
    try:
        # Convert binary string to integer
        six_bit_val = int(bit_string, 2)
        
        # Proper SIXBIT decoding table (matches generate_lorem_tape.py)
        if six_bit_val == 0:
            return ' '    # Space
        elif 1 <= six_bit_val <= 26:
            return chr(six_bit_val + 64)  # A-Z (1=A, 2=B, ..., 26=Z)
        elif six_bit_val == 30:
            return '⏎'   # Carriage return (reserved value)
        elif six_bit_val == 31:
            return '↵'   # Line feed (reserved value)
        elif 32 <= six_bit_val <= 41:  # 0-9
            return chr(six_bit_val - 32 + 48)  # 0-9 (32=0, 33=1, ..., 41=9)
        elif six_bit_val == 42:
            return '!'    # !
        elif six_bit_val == 43:
            return ','    # ,
        elif six_bit_val == 44:
            return '-'    # -
        elif six_bit_val == 45:
            return '.'    # .
        elif six_bit_val == 46:
            return "'"    # '
        elif six_bit_val == 47:
            return ':'    # :
        elif six_bit_val == 48:
            return ';'    # ;
        elif six_bit_val == 49:
            return '?'    # ?
        elif 27 <= six_bit_val <= 29:
            return '.'    # Reserved values
        elif 50 <= six_bit_val <= 63:
            return '.'    # Undefined values
        else:
            return '.'    # Should not happen with 6-bit values
    except ValueError:
        return '?'  # Invalid binary string

def detect_tape_format(line):
    """Detect whether tape is SIXBIT (binary) or ASCII (octal) format."""
    if ':' not in line:
        return None
    
    _, data_part = line.split(':', 1)
    chunks = data_part.strip().split()
    
    if not chunks:
        return None
    
    # Check first chunk to determine format
    first_chunk = chunks[0]
    
    # SIXBIT: 6 characters of 0/1
    if len(first_chunk) == 6 and all(c in '01' for c in first_chunk):
        return 'sixbit'
    
    # ASCII: 3 digits (octal 000-377)
    if len(first_chunk) == 3 and first_chunk.isdigit():
        return 'ascii'
    
    return None

def parse_tape_line(line, format_type='sixbit'):
    """Parse a single tape line and extract characters.
    
    Args:
        line: The tape line to parse
        format_type: Either 'sixbit' (binary) or 'ascii' (octal)
    
    Returns:
        List of decoded characters
    """
    # Remove tape identifier (2-3 char label + 3-digit octal block, e.g., "TP001:", "MAN001:")
    if ':' in line:
        _, data_part = line.split(':', 1)
    else:
        data_part = line
    
    chunks = data_part.strip().split()
    decoded_chars = []
    
    if format_type == 'sixbit':
        # SIXBIT format: 6-bit binary strings
        for chunk in chunks:
            if len(chunk) == 6 and all(c in '01' for c in chunk):
                decoded_chars.append(six_bit_to_ascii(chunk))
    
    elif format_type == 'ascii':
        # ASCII format: 3-digit octal values
        for chunk in chunks:
            if len(chunk) == 3 and chunk.isdigit():
                try:
                    char_code = int(chunk, 8)
                    if 0 <= char_code <= 127:
                        decoded_chars.append(chr(char_code))
                except ValueError:
                    pass
    
    return decoded_chars

def read_tape_file(filename, records_only=False):
    """Read and decode a papertape file (SIXBIT or ASCII format)."""
    if not os.path.exists(filename):
        print(f"Error: File '{filename}' not found.")
        return None
    
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except IOError as e:
        print(f"Error reading file '{filename}': {e}")
        return None
    
    # Auto-detect format from first valid line
    format_type = None
    for line in lines:
        line = line.strip()
        if line and ':' in line:
            format_type = detect_tape_format(line)
            if format_type:
                break
    
    if not format_type:
        print(f"Error: Could not detect tape format (expected SIXBIT or ASCII)")
        return None
    
    format_name = "SIXBIT (binary)" if format_type == 'sixbit' else "ASCII (octal)"
    
    if records_only:
        # For records-only mode, return list of (label, content) tuples
        records = []
        for line_num, line in enumerate(lines, 1):
            line = line.strip()
            if not line:
                continue
                
            # Parse tape line to extract label and decode content
            if ':' in line:
                label_part, _ = line.split(':', 1)
                decoded_chars = parse_tape_line(line, format_type)
                
                if decoded_chars:
                    line_text = ''.join(decoded_chars)
                    records.append((label_part.strip(), line_text))
        return records
    
    decoded_text = ""
    total_chunks = 0
    line_count = 0
    
    if not records_only:
        print(f"Reading tape file: {filename}")
        print(f"Format: {format_name}")
        print("=" * 60)
    
    for line_num, line in enumerate(lines, 1):
        line = line.strip()
        if not line:
            continue
            
        line_count += 1
        decoded_chars = parse_tape_line(line, format_type)
        
        if not decoded_chars:
            continue
            
        total_chunks += len(decoded_chars)
        
        # Append decoded characters
        for char in decoded_chars:
            decoded_text += char
    
    if not records_only:
        print(f"Tape statistics:")
        print(f"  Lines processed: {line_count}")
        chunk_label = "6-bit chunks" if format_type == 'sixbit' else "ASCII characters"
        print(f"  Total {chunk_label}: {total_chunks}")
        print(f"  Decoded characters: {len(decoded_text)}")
        print()
    
    return decoded_text

def display_tape_content(content, show_raw=False, show_hex=False):
    """Display the decoded tape content in various formats."""
    if not content:
        print("No content to display.")
        return
    
    print("Decoded tape content:")
    print("-" * 40)
    
    # Clean up the content for display
    display_content = content.replace('\r', '')  # Remove carriage returns
    
    # Show readable text
    print(display_content)
    print()
    
    if show_raw:
        print("Raw character codes:")
        print("-" * 40)
        for i, char in enumerate(content):
            if i > 0 and i % 16 == 0:
                print()
            print(f"{ord(char):3d}", end=" ")
        print("\n")
    
    if show_hex:
        print("Hexadecimal representation:")
        print("-" * 40)
        for i, char in enumerate(content):
            if i > 0 and i % 16 == 0:
                print()
            print(f"{ord(char):02x}", end=" ")
        print("\n")

def main():
    parser = argparse.ArgumentParser(
        description="Read and display PDP-8 papertape files (SIXBIT or ASCII format)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  %(prog)s tapes/lorem.tape
  %(prog)s tapes/8asm-man.tape
  %(prog)s tapes/tp_demo.tape --raw
  %(prog)s tapes/lorem.tape --hex --raw
  %(prog)s tapes/lorem.tape --records
        """
    )
    
    parser.add_argument('tape_file', 
                       help='Path to the papertape file to read (SIXBIT or ASCII format)')
    parser.add_argument('--raw', action='store_true',
                       help='Show raw character codes')
    parser.add_argument('--hex', action='store_true',
                       help='Show hexadecimal representation')
    parser.add_argument('--records', action='store_true',
                       help='Show only record labels and content, no statistics')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Show detailed parsing information')
    
    args = parser.parse_args()
    
    if args.records:
        # Records-only mode
        records = read_tape_file(args.tape_file, records_only=True)
        if records is not None:
            for label, content in records:
                # Clean up content and replace control characters
                clean_content = content.replace('\n', ' ').replace('\r', ' ').strip()
                # Remove excessive spaces
                clean_content = ' '.join(clean_content.split())
                print(f"{label}: {clean_content}")
        else:
            sys.exit(1)
    else:
        # Regular mode
        content = read_tape_file(args.tape_file)
        if content is not None:
            display_tape_content(content, show_raw=args.raw, show_hex=args.hex)
        else:
            sys.exit(1)

if __name__ == "__main__":
    main()