#!/usr/bin/env python3
"""A simple visual tape reader for PDP-8 paper tape files.

Usage:
    python3 demo/scripts/visual_tape_reader.py <tape_file>
    python3 demo/scripts/visual_tape_reader.py <tape_file> --slowly

Arguments:
    tape_file  Path to the PDP-8 paper tape file to read.
    --slowly   Optional flag to introduce a delay between reading each tape word.

This script understands two tape payload formats:
- bit-strings composed of '0' and '1' (ignored whitespace, comments after '#'),
  interpreted as a stream of bits grouped into 12-bit words.
- ASCII-octal tokens (e.g. 070 101 123), each token is an octal byte (000-377)
  that becomes one 12-bit word with the low 8 bits set.

The script will auto-detect the format per line (trying bit-strings first,
then falling back to ASCII-octal) to match the emulator's loader behavior.

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


def detect_and_parse_payload(payload: str) -> Tuple[List[int], str]:
    """Try bit parsing first; if that fails, try ASCII-octal. Returns (words, parser_name)."""
    words, err = parse_bits(payload)
    if err == '':
        return words, 'bits'
    words, err2 = parse_ascii_octal(payload)
    if err2 == '':
        return words, 'ascii-octal'
    # Return bit error by default (keep diagnostics simple)
    raise ValueError(f"Unable to parse payload as bits ({err}) or ascii-octal ({err2})")


def load_tape(path: str):
    blocks = []
    label = ''
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
            if payload == '':
                # empty block
                words = []
                parser = 'empty'
            else:
                try:
                    words, parser = detect_and_parse_payload(payload)
                except ValueError as exc:
                    print(f"Line {lineno}: {exc}", file=sys.stderr)
                    continue
            if not label:
                label = lbl
            elif label != lbl:
                # tolerate differing labels but warn
                print(f"Warning: inconsistent label on line {lineno}: {lbl} != {label}", file=sys.stderr)
            blocks.append({'block': int(block_text, 8), 'words': words, 'parser': parser})
    return label, blocks


def show_block(block_info, index=None):
    blocknum = block_info['block']
    words = block_info['words']
    parser = block_info['parser']
    header = f"{block_info.get('label','??')}{blocknum:03o} ({blocknum:03o}): {len(words)} word(s) parser={parser}"
    print(header)
    for i, w in enumerate(words):
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
    args = ap.parse_args(argv)

    label, blocks = load_tape(args.tape)
    # annotate block dicts with label for printing
    for b in blocks:
        b['label'] = label

    if args.dump:
        print(f"Tape label={label} blocks={len(blocks)}")
        for b in blocks:
            print(f"  {label}{b['block']:03o}: {len(b['words'])} words parser={b['parser']}")
        return 0

    # select blocks to show
    sel = blocks
    if args.block is not None:
        sel = [b for b in blocks if b['block'] == args.block]
        if not sel:
            print(f"Block {args.block:03o} not found in tape")
            return 1

    for b in sel:
        print(f"{b['label']}{b['block']:03o}: {len(b['words'])} words (parser={b['parser']})")
        for i, w in enumerate(b['words']):
            ch = chr(w) if 32 <= w <= 126 else '.'
            print(f"{i:03d}: {w:04o} '{ch}'")
            if args.slowly:
                time.sleep(args.delay)
        print()
    return 0


def curses_viewer(label: str, blocks: List[dict]):
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
            stdscr.addstr(0, 0, f"Tape {label} blocks={n} READY={int(current_ready)}{autoplay_status}  (Arrows: block/word, space: play/pause, q: quit)")
            # show current block immediately on the next line (no blank row)
            header = f"{b['label']}{b['block']:03o}: {len(b['words'])} words parser={b['parser']}"
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
                    low = wv & 0xFF
                    if low == 0o12:
                        tele_append('\r\n')
                    elif low == 0o11:
                        tele_append('    ')
                    elif 32 <= low <= 126:
                        tele_append(chr(low))
                    else:
                        tele_append('?')
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
                    low = wv & 0xFF
                    if low == 0o12:
                        tele_append('\r\n')
                    elif low == 0o11:
                        tele_append('    ')
                    elif 32 <= low <= 126:
                        tele_append(chr(low))
                    else:
                        tele_append('?')
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

    import curses
    curses.wrapper(draw_screen)


if __name__ == '__main__':
    # Provide a fallback entry for direct runs that still do dump/test
    if '--curses' in sys.argv:
        ap = argparse.ArgumentParser(add_help=False)
        ap.add_argument('tape')
        ap.add_argument('--curses', action='store_true')
        parsed = ap.parse_args()
        label, blocks = load_tape(parsed.tape)
        curses_viewer(label, blocks)
    else:
        raise SystemExit(main())
