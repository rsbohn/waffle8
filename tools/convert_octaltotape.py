#!/usr/bin/env python3
import sys
import re

# Convert tape lines that look like:
# MA001: 070 101 123 ...
# into bit patterns per word: each octal value -> 12-bit binary string without spaces in-between words.
# The loader expects sequences of '0' and '1' possibly separated by spaces; we'll produce similar to tp_demo.tape

oct_re = re.compile(r"^([A-Z]{2})([0-7]{3}):\s*(.*)$")

if len(sys.argv) < 3:
    print("Usage: convert_octaltotape.py input.tape output.tape")
    sys.exit(2)

inpath = sys.argv[1]
outpath = sys.argv[2]

with open(inpath, 'r', encoding='utf-8') as inf, open(outpath, 'w', encoding='utf-8') as outf:
    for line in inf:
        m = oct_re.match(line.strip())
        if not m:
            # copy blank/comment lines through
            outf.write(line)
            continue
        label = m.group(1)
        block = m.group(2)
        rest = m.group(3).strip()
        if rest == '':
            outf.write(f"{label}{block}:\n")
            continue
        # parse octal numbers separated by spaces
        parts = rest.split()
        bits = []
        for p in parts:
            try:
                val = int(p, 8)
            except ValueError:
                # keep literal characters like '012' as-is
                continue
            # encode as 12-bit binary
            b = format(val & 0xFFF, '012b')
            bits.append(b)
        if bits:
            # join without separator similar to tp_demo (space separated groups is also fine)
            outf.write(f"{label}{block}: "+" ".join(bits)+"\n")
        else:
            outf.write(line)

print('Converted', inpath, '->', outpath)
