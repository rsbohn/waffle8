#!/usr/bin/env python3
"""A simple visual tape reader for PDP-8 paper tape files.

Usage:
    python3 demo/scripts/visual_tape_reader.py <tape_file>
    python3 demo/scripts/visual_tape_reader.py <tape_file> --slowly

Arguments:
    tape_file  Path to the PDP-8 paper tape file to read.
    --slowly   Optional flag to introduce a delay between reading each tape word.

This script understands three tape payload formats:
- bit-strings composed of '0' and '1' (ignored whitespace, comments after '#'),
  interpreted as a stream of bits grouped into 12-bit words.
- ASCII-octal tokens (e.g. 070 101 123), each token is an octal byte (000-377)
  that becomes one 12-bit word with the low 8 bits set.
- SIXBIT-octal tokens (e.g. 010 023 045), each token is a 6-bit character (000-077)
  that packs two characters per 12-bit word.

The script auto-detects the format once from the first non-empty record, since
tapes do not switch format mid-stream.

Sample output:
    MA001 (001): 50 words
      0: 0134  (  92) 'A'
      1: 0123  (  83) 'S'

"""

from __future__ import annotations

import argparse
import sys
import time
import re
from typing import List, Tuple

LINE_RE = re.compile(r"^([A-Z]{2})([0-7]{3}):\s*(.*)$")

# SIXBIT character encoding (6-bit to ASCII)
SIXBIT_TO_ASCII = {
    0o00: ' ',  # space
    0o01: 'A', 0o02: 'B', 0o03: 'C', 0o04: 'D', 0o05: 'E', 0o06: 'F',
    0o07: 'G', 0o10: 'H', 0o11: 'I', 0o12: 'J', 0o13: 'K', 0o14: 'L',
    0o15: 'M', 0o16: 'N', 0o17: 'O', 0o20: 'P', 0o21: 'Q', 0o22: 'R',
    0o23: 'S', 0o24: 'T', 0o25: 'U', 0o26: 'V', 0o27: 'W', 0o30: 'X',
    0o31: 'Y', 0o32: 'Z', 0o33: '[', 0o34: '\\', 0o35: ']', 0o36: '^',
    0o37: '_', 0o40: '0', 0o41: '1', 0o42: '2', 0o43: '3', 0o44: '4',
    0o45: '5', 0o46: '6', 0o47: '7', 0o50: '8', 0o51: '9', 0o52: ':',
    0o53: ';', 0o54: '<', 0o55: '=', 0o56: '>', 0o57: '?', 0o60: '@',
    0o61: '!', 0o62: '"', 0o63: '#', 0o64: '$', 0o65: '%', 0o66: '&',
    0o67: "'", 0o70: '(', 0o71: ')', 0o72: '*', 0o73: '+', 0o74: ',',
    0o75: '-', 0o76: '.', 0o77: '/',
}

def parse_bits(text: str) -> Tuple[List[int], str]:
    """Parse a bit-text into 12-bit words. Returns (words, error) where error is '' on success."""
    bits = []
    for ch in text:
        if ch.isspace():
            continue
        if ch == '#':
            break
        if ch not in '01':
            return [], f"invalid bit char '{ch}'"
        bits.append(ch)
    if len(bits) == 0:
        return [], "empty bit string"
    if len(bits) % 12 != 0:
        return [], f"bit count {len(bits)} not multiple of 12"
    words = []
    for i in range(0, len(bits), 12):
        wbits = ''.join(bits[i:i+12])
        words.append(int(wbits, 2) & 0x0FFF)
    return words, ''


def parse_sixbit_octal(text: str) -> Tuple[List[int], str]:
    """Parse SIXBIT-octal tokens (000..077) into 12-bit words (two 6-bit chars per word)."""
    tokens = []
    for tok in text.split():
        if tok.startswith('#'):
            break
        if not all('0' <= c <= '7' for c in tok):
            return [], f"non-octal token '{tok}'"
        try:
            val = int(tok, 8)
        except ValueError:
            return [], f"bad octal token '{tok}'"
        if val < 0 or val > 0o77:
            return [], f"SIXBIT token out of range '{tok}'"
        tokens.append(val & 0o77)
    if len(tokens) == 0:
        return [], 'no tokens'
    # Pack pairs of 6-bit values into 12-bit words
    words = []
    for i in range(0, len(tokens), 2):
        if i + 1 < len(tokens):
            word = (tokens[i] << 6) | tokens[i+1]
        else:
            word = tokens[i] << 6
        words.append(word & 0x0FFF)
    return words, ''


def parse_ascii_octal(text: str) -> Tuple[List[int], str]:
    """Parse ASCII-octal tokens (000..377) into words (8-bit zero-extended into 12-bit words)."""
    tokens = []
    for tok in text.split():
        if tok.startswith('#'):
            break
        if not all('0' <= c <= '7' for c in tok):
            return [], f"non-octal token '{tok}'"
        try:
            val = int(tok, 8)
        except ValueError:
            return [], f"bad octal token '{tok}'"
        if val < 0 or val > 0xFF:
            return [], f"octal token out of range '{tok}'"
        tokens.append(val & 0xFF)
    if len(tokens) == 0:
        return [], 'no tokens'
    return tokens, ''


def detect_format(payload: str) -> str:
    """Detect tape format from first non-empty payload. Returns 'sixbit', 'ascii-octal', or 'bits'."""
    # Try SIXBIT first (most restrictive - only 000-077)
    _, err = parse_sixbit_octal(payload)
    if err == '':
        return 'sixbit'
    # Then try ASCII-octal (000-377)
    _, err2 = parse_ascii_octal(payload)
    if err2 == '':
        return 'ascii-octal'
    # Finally try bits (least restrictive - accepts 0/1)
    _, err3 = parse_bits(payload)
    if err3 == '':
        return 'bits'
    raise ValueError(f"Unable to detect format: sixbit ({err}), ascii-octal ({err2}), bits ({err3})")


def decode_sixbit_char(code: int) -> str:
    """Decode a single 6-bit SIXBIT character to output string.
    
    Special characters:
      0o54 '<' = EOL (combined CR+LF)
      0o36 '^' = CR (carriage return)
      0o37 '_' = LF (line feed)
    """
    if code == 0o54:
        return '\r\n'
    elif code == 0o36:
        return '\r'
    elif code == 0o37:
        return '\n'
    return SIXBIT_TO_ASCII.get(code, '?')


def decode_word_for_teleprinter(word: int, encoding: str) -> str:
    """Decode a 12-bit word into teleprinter output based on display encoding.
    
    For SIXBIT, handles both EOL encodings:
      - Single character: 0o54 (< → CR+LF)
      - Two character: 0o36 0o37 (^_ → CR+LF)
    """
    word = word & 0x0FFF
    if encoding == 'sixbit':
        hi = (word >> 6) & 0o77
        lo = word & 0o77
        # Check for the two-character CR+LF sequence (^_)
        if hi == 0o36 and lo == 0o37:
            return '\r\n'
        return decode_sixbit_char(hi) + decode_sixbit_char(lo)
    else:
        # ASCII mode
        low = word & 0xFF
        if low == 0o12:
            return '\r\n'
        elif low == 0o11:
            return '    '
        elif 32 <= low <= 126:
            return chr(low)
        else:
            return '?'


def load_tape(path: str):
    blocks = []
    label = ''
    tape_format = None
    
    with open(path, 'r', encoding='utf-8') as f:
        for lineno, raw in enumerate(f, start=1):
            line = raw.rstrip('\n')
            if not line.strip():
                continue
            m = LINE_RE.match(line)
            if not m:
                # try to skip leading whitespace and retry
                s = line.lstrip()
                m = LINE_RE.match(s)
                if not m:
                    print(f"Skipping unparsable line {lineno}: {line}", file=sys.stderr)
                    continue
            lbl = m.group(1)
            block_text = m.group(2)
            payload = m.group(3).strip()
            
            # Detect format from first non-empty payload
            if tape_format is None and payload != '':
                try:
                    tape_format = detect_format(payload)
                except ValueError as exc:
                    print(f"Line {lineno}: {exc}", file=sys.stderr)
                    continue
            
            # Parse payload using detected format
            if payload == '':
                words = []
            else:
                if tape_format == 'sixbit':
                    words, err = parse_sixbit_octal(payload)
                elif tape_format == 'ascii-octal':
                    words, err = parse_ascii_octal(payload)
                else:  # bits
                    words, err = parse_bits(payload)
                
                if err:
                    print(f"Line {lineno}: parse error: {err}", file=sys.stderr)
                    continue
            
            if not label:
                label = lbl
            elif label != lbl:
                # tolerate differing labels but warn
                print(f"Warning: inconsistent label on line {lineno}: {lbl} != {label}", file=sys.stderr)
            blocks.append({'block': int(block_text, 8), 'words': words})
    
    return label, blocks, tape_format if tape_format else 'empty'


def show_block(block_info, tape_format, index=None):
    blocknum = block_info['block']
    words = block_info['words']
    header = f"{block_info.get('label','??')}{blocknum:03o} ({blocknum:03o}): {len(words)} word(s) format={tape_format}"
    print(header)
    for i, w in enumerate(words):
        if tape_format == 'sixbit':
            hi = (w >> 6) & 0o77
            lo = w & 0o77
            ch_hi = SIXBIT_TO_ASCII.get(hi, '?')
            ch_lo = SIXBIT_TO_ASCII.get(lo, '?')
            print(f"  {i:3d}: {w:04o} ({w:4d}) '{ch_hi}{ch_lo}'")
        else:
            ch = chr(w) if 32 <= w <= 126 else '.'
            print(f"  {i:3d}: {w:04o} ({w:4d}) '{ch}'")


def main(argv=None):
    ap = argparse.ArgumentParser(description='Visual PDP-8 paper tape reader')
    ap.add_argument('tape', help='tape file')
    ap.add_argument('--slowly', '-s', action='store_true', help='delay between words')
    ap.add_argument('--delay', type=float, default=0.05, help='delay seconds when --slowly')
    ap.add_argument('--block', '-b', type=lambda x: int(x,8), help='select octal block to show/read')
    ap.add_argument('--dump', action='store_true', help='dump all blocks summary')
    ap.add_argument('--curses', action='store_true', help='interactive curses viewer')
    ap.add_argument('--sixbit', action='store_true', help='force SIXBIT interpretation')
    args = ap.parse_args(argv)

    label, blocks, tape_format = load_tape(args.tape)
    # annotate block dicts with label for printing
    for b in blocks:
        b['label'] = label

    # Allow --sixbit to override detected format
    display_format = 'sixbit' if args.sixbit else tape_format

    if args.dump:
        print(f"Tape label={label} blocks={len(blocks)} format={tape_format}")
        for b in blocks:
            print(f"  {label}{b['block']:03o}: {len(b['words'])} words")
        return 0

    # select blocks to show
    sel = blocks
    if args.block is not None:
        sel = [b for b in blocks if b['block'] == args.block]
        if not sel:
            print(f"Block {args.block:03o} not found in tape")
            return 1

    for b in sel:
        print(f"{b['label']}{b['block']:03o}: {len(b['words'])} words (format={display_format})")
        for i, w in enumerate(b['words']):
            if display_format == 'sixbit':
                hi = (w >> 6) & 0o77
                lo = w & 0o77
                ch_hi = SIXBIT_TO_ASCII.get(hi, '?')
                ch_lo = SIXBIT_TO_ASCII.get(lo, '?')
                print(f"{i:03d}: {w:04o} '{ch_hi}{ch_lo}'")
            else:
                ch = chr(w) if 32 <= w <= 126 else '.'
                print(f"{i:03d}: {w:04o} '{ch}'")
            if args.slowly:
                time.sleep(args.delay)
        print()
    return 0


def curses_viewer(label: str, blocks: List[dict], tape_format: str):
    import curses
    # ensure each block has a label key so the UI can display it without KeyError
    for b in blocks:
        b.setdefault('label', label)

    def draw_screen(stdscr):
        curses.curs_set(0)
        # make getch non-blocking with a small timeout so we can check keys during autoplay
        stdscr.timeout(50)  # ms
        stdscr.keypad(True)
        n = len(blocks)
        idx = 0
        word_pos = 0
        autoplay = False
        delay = 0.1
        # teleprinter state: history of appended strings (units for undo)
        tele_hist: List[str] = []
        last_tick = time.monotonic()
        # User-selectable display encoding (rotates with Tab)
        display_encoding = tape_format
        encoding_modes = ['bits', 'ascii', 'sixbit']

        def tele_append(s: str):
            tele_hist.append(s)

        def tele_undo():
            if tele_hist:
                tele_hist.pop()

        def tele_lines():
            # render teleprinter history into display lines
            out = ''.join(tele_hist)
            # normalize CRLF as single newline for display
            out = out.replace('\r\n', '\n')
            return out.split('\n') if out else ['']

        def render_head_info(b, pos):
            # READY is set when there is a word available to read at pos
            ready = (0 <= pos < len(b['words']))
            if ready:
                w = b['words'][pos] & 0x0FFF
                if display_encoding == 'sixbit':
                    hi = (w >> 6) & 0o77
                    lo = w & 0o77
                    ch_hi = SIXBIT_TO_ASCII.get(hi, '?')
                    ch_lo = SIXBIT_TO_ASCII.get(lo, '?')
                    ch = f"{ch_hi}{ch_lo}"
                elif display_encoding == 'bits':
                    ch = f"{w:012b}"
                else:  # ascii
                    ch = chr(w & 0xFF) if 32 <= (w & 0xFF) <= 126 else '.'
                return f"Block {b.get('label','?')}{b['block']:03o} Pos={pos:03d} Word={w:04o} ({w:3d}) '{ch}' READY={int(ready)}"
            else:
                return f"Block {b.get('label','?')}{b['block']:03o} Pos=END READY={int(ready)}"

        while True:
            stdscr.erase()
            h, w = stdscr.getmaxyx()
            # select current block early so header can reference it
            b = blocks[idx]
            # header (compact, no blank line) - show READY state for current head
            # READY is true when the current block has a word at word_pos
            current_ready = (0 <= word_pos < len(b['words']))
            autoplay_status = f" AUTOPLAY={delay:.2f}s" if autoplay else ''
            stdscr.addstr(0, 0, f"Tape {label} blocks={n} READY={int(current_ready)}{autoplay_status}  (Arrows: block/word, space: play/pause, Tab: encoding, q: quit)")
            # show current block immediately on the next line (no blank row)
            header = f"{b['label']}{b['block']:03o}: {len(b['words'])} words format={tape_format} display={display_encoding}"
            stdscr.addstr(1, 0, header)

            # render teleprinter area above the words (compact: no label line)
            tele = tele_lines()
            # reserve up to ~1/6 of the screen for teleprinter, at least 3 lines
            tele_h = min(max(3, len(tele)), max(3, h // 6))
            # tele content starts right below the block header (row 2)
            tele_header_row = 2
            # show last tele_h lines of tele (no separate header line)
            start_line = max(0, len(tele) - tele_h)
            for i, tl in enumerate(tele[start_line:], start=0):
                row = tele_header_row + i
                if row >= h:
                    break
                stdscr.addstr(row, 0, tl[:w-1])

            # separator under teleprinter
            separator_row = tele_header_row + tele_h
            if separator_row < h:
                stdscr.hline(separator_row, 0, ord('-'), max(10, w))

            # show a window of words around word_pos (vertical list) under teleprinter
            words_start_row = separator_row + 1
            max_lines = h - words_start_row - 2
            if max_lines < 1:
                max_lines = 1
            start = max(0, word_pos - max_lines//2)
            for line in range(max_lines):
                wi = start + line
                if wi >= len(b['words']):
                    break
                wv = b['words'][wi]
                if display_encoding == 'sixbit':
                    hi = (wv >> 6) & 0o77
                    lo = wv & 0o77
                    ch_hi = SIXBIT_TO_ASCII.get(hi, '?')
                    ch_lo = SIXBIT_TO_ASCII.get(lo, '?')
                    ch = f"{ch_hi}{ch_lo}"
                elif display_encoding == 'bits':
                    ch = f"{wv:012b}"
                else:  # ascii
                    ch = chr(wv & 0xFF) if 32 <= (wv & 0xFF) <= 126 else '.'
                marker = '>' if wi == word_pos else ' '
                row = words_start_row + line
                if row < h:
                    stdscr.addstr(row, 0, f"{marker}{wi:04d}: {wv:04o} ({(wv&0xFFF):4d}) '{ch}'")

            stdscr.refresh()

            # handle autoplay timing (do one step when enough time elapsed)
            now = time.monotonic()
            if autoplay and (now - last_tick) >= delay:
                # perform one read-and-append
                if 0 <= word_pos < len(b['words']):
                    wv = b['words'][word_pos] & 0x0FFF
                    tele_append(decode_word_for_teleprinter(wv, display_encoding))
                    word_pos += 1
                else:
                    if idx + 1 < n:
                        idx += 1
                        word_pos = 0
                    else:
                        autoplay = False
                last_tick = now

            c = stdscr.getch()
            if c == ord('q') or c == curses.KEY_DC or c == 127:
                break
            elif c == curses.KEY_DOWN:
                if idx + 1 < n:
                    idx += 1
                    word_pos = 0
            elif c == curses.KEY_UP:
                if idx > 0:
                    idx -= 1
                    word_pos = 0
            elif c == curses.KEY_RIGHT:
                # manual read: append current word then advance
                if 0 <= word_pos < len(b['words']):
                    wv = b['words'][word_pos] & 0x0FFF
                    tele_append(decode_word_for_teleprinter(wv, display_encoding))
                    # if this was the last word, advance to the next block
                    if word_pos + 1 < len(b['words']):
                        word_pos += 1
                    else:
                        if idx + 1 < n:
                            idx += 1
                            word_pos = 0
                else:
                    # at end-of-block with head beyond last word: try advance to next block
                    if idx + 1 < n:
                        idx += 1
                        word_pos = 0
            elif c == curses.KEY_LEFT:
                # manual undo: move back and undo last print if any
                # prefer undoing first, then move head back if possible
                tele_undo()
                if word_pos > 0:
                    word_pos -= 1
            elif c == ord(' '):
                autoplay = not autoplay
                last_tick = time.monotonic()
            elif c == ord(']'):
                delay = max(0.01, delay - 0.01)
            elif c == ord('['):
                delay = delay + 0.01
            elif c == ord('\t'):  # Tab key
                current_idx = encoding_modes.index(display_encoding) if display_encoding in encoding_modes else 0
                display_encoding = encoding_modes[(current_idx + 1) % len(encoding_modes)]

    import curses
    curses.wrapper(draw_screen)


if __name__ == '__main__':
    # Provide a fallback entry for direct runs that still do dump/test
    if '--curses' in sys.argv:
        ap = argparse.ArgumentParser(add_help=False)
        ap.add_argument('tape')
        ap.add_argument('--curses', action='store_true')
        parsed = ap.parse_args()
        label, blocks, tape_format = load_tape(parsed.tape)
        curses_viewer(label, blocks, tape_format)
    else:
        raise SystemExit(main())
