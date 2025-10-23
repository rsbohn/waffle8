#!/usr/bin/env python3
"""
Convert ASCII text to PDP-8 papertape format.
Creates ASCII papertape with 6-bit encoding for PDP-8 systems.
"""

import sys
from auto_lorem import generate_auto_lorem

def ascii_to_6bit(char):
    """Convert ASCII character to 6-bit representation using proper SIXBIT encoding."""
    ascii_val = ord(char)
    
    # Proper SIXBIT encoding to avoid letter/control character conflicts
    if ascii_val == 32:   # Space
        return '000000'   # 0
    elif 65 <= ascii_val <= 90:  # A-Z
        return format(ascii_val - 64, '06b')  # A=1, B=2, ..., Z=26
    elif 97 <= ascii_val <= 122: # a-z -> convert to uppercase
        return format(ascii_val - 96, '06b')  # a=1, b=2, ..., z=26
    elif ascii_val == 10: # Line feed - use a safe value not conflicting with letters
        return '011111'   # 31 (reserved for LF)
    elif ascii_val == 13: # Carriage return - use another safe value
        return '011110'   # 30 (reserved for CR)
    elif 48 <= ascii_val <= 57:  # 0-9
        return format(ascii_val - 48 + 32, '06b')  # 0=32, 1=33, ..., 9=41
    elif ascii_val == 33: # !
        return format(42, '06b')   # 42
    elif ascii_val == 44: # ,
        return format(43, '06b')   # 43
    elif ascii_val == 45: # -
        return format(44, '06b')   # 44
    elif ascii_val == 46: # .
        return format(45, '06b')   # 45
    elif ascii_val == 39: # '
        return format(46, '06b')   # 46
    elif ascii_val == 58: # :
        return format(47, '06b')   # 47
    elif ascii_val == 59: # ;
        return format(48, '06b')   # 48
    elif ascii_val == 63: # ?
        return format(49, '06b')   # 49
    else:
        return '000000'   # Default to space for unknown chars

def text_to_papertape(text, tape_id="TP001"):
    """Convert text to papertape format."""
    # Clean up text - remove excessive whitespace and limit line length
    lines = text.strip().split('\n')
    cleaned_lines = []
    
    for line in lines:
        # Wrap long lines at word boundaries
        words = line.split()
        current_line = ""
        for word in words:
            if len(current_line + word) < 60:  # Keep lines reasonable
                current_line += word + " "
            else:
                if current_line:
                    cleaned_lines.append(current_line.strip())
                current_line = word + " "
        if current_line:
            cleaned_lines.append(current_line.strip())
    
    # Convert to 6-bit sequences
    bit_sequences = []
    for line in cleaned_lines:
        for char in line:
            bit_sequences.append(ascii_to_6bit(char))
        # Add line feed
        bit_sequences.append(ascii_to_6bit('\n'))
    
    # Format as papertape with reasonable line length
    tape_lines = []
    chunks_per_line = 64  # Match the demo format
    
    for i in range(0, len(bit_sequences), chunks_per_line):
        chunk = bit_sequences[i:i + chunks_per_line]
        if chunk:  # Only add non-empty chunks
            line_num = (i // chunks_per_line) + 1
            tape_line = f"{tape_id}{line_num:03o}: " + " ".join(chunk)
            tape_lines.append(tape_line)
    
    return "\n".join(tape_lines)

def main():
    print("Generating Auto Manufacturer Lorem Ipsum Papertape...")
    
    # Generate automotive lorem ipsum
    lorem_text = generate_auto_lorem(3, 4)
    print("Generated text:")
    print("-" * 50)
    print(lorem_text)
    print("-" * 50)
    print()
    
    # Convert to papertape format
    papertape = text_to_papertape(lorem_text, "AU")
    
    # Save to file
    output_file = "/home/rsbohn/emulation/waffle8/tapes/lorem.tape"
    with open(output_file, 'w') as f:
        f.write(papertape)
    
    print(f"Papertape saved to: {output_file}")
    print(f"Number of lines: {len(papertape.split(chr(10)))}")
    
    # Show first few lines as preview
    print("\nFirst few lines of papertape:")
    lines = papertape.split('\n')
    for i, line in enumerate(lines[:3]):
        print(f"  {line}")
    if len(lines) > 3:
        print(f"  ... and {len(lines)-3} more lines")

if __name__ == "__main__":
    main()