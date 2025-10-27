#!/usr/bin/env python3
"""
Convert man page to ASCII papertape format for PDP-8.
"""

import sys
from pathlib import Path
from typing import List


def text_to_ascii_tape(text: str, tape_id: str = "MA") -> str:
    """Convert text to ASCII papertape format with octal encoding."""
    lines = text.split('\n')
    tape_lines: List[str] = []
    
    words_per_line = 64
    line_num = 1
    
    for text_line in lines:
        # Convert each character to octal words (one ASCII char per word)
        words: List[str] = []
        for char in text_line:
            ascii_val = ord(char) & 0xFF
            words.append(f"{ascii_val:03o}")
        
        # Add newline at end of each line
        words.append("012")  # \n in octal
        
        # Output in chunks of words_per_line
        for i in range(0, len(words), words_per_line):
            chunk = words[i:i + words_per_line]
            tape_line = f"{tape_id}{line_num:03o}: " + " ".join(chunk)
            tape_lines.append(tape_line)
            line_num += 1
    
    return "\n".join(tape_lines)


def main():
    if len(sys.argv) < 2:
        print("Usage: man_to_tape.py <input_text_file> [output_tape_file] [tape_id]")
        print("Example: man_to_tape.py 8asm-man.txt tapes/8asm-man.tape MA")
        return 1
    
    input_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(f"tapes/{input_file.stem}.tape")
    tape_id = sys.argv[3] if len(sys.argv) > 3 else "MA"
    
    if not input_file.exists():
        print(f"Error: {input_file} not found")
        return 1
    
    text = input_file.read_text()
    tape = text_to_ascii_tape(text, tape_id[:2])
    
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(tape + "\n")
    
    print(f"Converted {input_file} to ASCII papertape")
    print(f"Output: {output_file}")
    print(f"Lines: {len(tape.split(chr(10)))}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
