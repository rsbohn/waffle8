"""
tu56 - TC08 DECtape utilities for OS/8 images (.tu56, SIMH format).

This module provides reusable functions for reading and parsing DECtape images:
- Physical and OS/8 logical block reading
- Sixbit character encoding/decoding
- OS/8 directory parsing
"""

import os
import struct
from typing import Iterable, List, Tuple

# Physical DECtape constants
BLOCK_WORDS = 129              # physical DECtape block size in 12-bit words
BLOCK_BYTES = BLOCK_WORDS * 2  # packed little-endian 16-bit words on disk

# OS/8 logical block constants
OS8_LOGICAL_WORDS = 256
OS8_LOGICAL_BYTES = OS8_LOGICAL_WORDS * 2
OS8_LOGICAL_STRIDE = 512  # OS/8 logical block size in bytes
DEFAULT_OS8_LOGICAL_BLOCKS = 575  # Matches 1150 physical frames (common OS/8 DECtape size)
DEFAULT_OS8_ORIGIN_BLOCK = 0o70   # First data block in OS/8 DECtape layout


def _unpack_words(data: bytes) -> List[int]:
    """Unpack bytes into 12-bit words (stored as 16-bit little-endian)."""
    return [struct.unpack("<H", data[i:i + 2])[0] & 0o7777 for i in range(0, len(data), 2)]


def sixbit_char(val: int) -> str:
    """Convert a 6-bit value to its ASCII character representation."""
    c = val & 0x3F
    if c == 0:
        return " "
    if 1 <= c <= 26:
        return chr(ord("A") + c - 1)
    if 27 <= c <= 36:
        return chr(ord("0") + c - 27)
    # Some tapes store digits using ASCII codes (0o60-0o71); treat them as 0-9.
    if 0o60 <= c <= 0o71:
        return chr(ord("0") + c - 0o60)
    if c == 46:
        return "."
    return "_"


def sixbit_pair(word: int) -> str:
    """Convert a 12-bit word to two sixbit characters."""
    return sixbit_char((word >> 6) & 0x3F) + sixbit_char(word & 0x3F)


def signed12(word: int) -> int:
    """Interpret a 12-bit word as a signed integer."""
    return word if word < 0o4000 else word - 0o10000


def read_physical_block(path: str, block: int) -> List[int]:
    """Read a physical DECtape block (129 words)."""
    offset = block * BLOCK_BYTES
    with open(path, "rb") as f:
        f.seek(offset)
        data = f.read(BLOCK_BYTES)
    if len(data) != BLOCK_BYTES:
        raise ValueError(f"Block {block} is incomplete (got {len(data)} bytes).")
    return _unpack_words(data)


def read_os8_block(path: str, block: int) -> List[int]:
    """Read an OS/8 logical block (256 data words).

    OS/8 lays a 256-word logical block across two physical DECtape frames:
    take physical blocks 2*block and 2*block+1, drop the trailing checksum
    word from each (129 -> 128 words), and concatenate.
    """
    phys0 = 2 * block
    phys1 = phys0 + 1
    size = os.path.getsize(path)
    total_phys = size // BLOCK_BYTES
    if phys1 >= total_phys:
        raise ValueError(f"Block {block} is incomplete (physical blocks {phys0},{phys1} not present).")
    w0 = read_physical_block(path, phys0)
    w1 = read_physical_block(path, phys1)
    data_words = w0[:-1] + w1[:-1]
    if len(data_words) != OS8_LOGICAL_WORDS:
        raise ValueError(f"Block {block} combined length {len(data_words)} != {OS8_LOGICAL_WORDS}.")
    return data_words


def dump_block(words: Iterable[int], sixbit: bool = False) -> None:
    """Print a block of words in octal or sixbit format."""
    words = list(words)
    if sixbit:
        for i in range(0, len(words), 8):
            chunk = words[i:i + 8]
            rendered = " ".join(f"{sixbit_pair(w)}" for w in chunk)
            print(f"{i:04o}: {rendered}")
    else:
        for i in range(0, len(words), 8):
            chunk = words[i:i + 8]
            rendered = " ".join(f"{w:04o}" for w in chunk)
            print(f"{i:04o}: {rendered}")


def format_block(words: Iterable[int], sixbit: bool = False) -> List[str]:
    """Format a block of words as lines (octal or sixbit), returns list of strings."""
    words = list(words)
    lines = []
    if sixbit:
        for i in range(0, len(words), 8):
            chunk = words[i:i + 8]
            rendered = " ".join(f"{sixbit_pair(w)}" for w in chunk)
            lines.append(f"{i:04o}: {rendered}")
    else:
        for i in range(0, len(words), 8):
            chunk = words[i:i + 8]
            rendered = " ".join(f"{w:04o}" for w in chunk)
            lines.append(f"{i:04o}: {rendered}")
    return lines


def parse_directory_segment(words: List[int]) -> Tuple[dict, List[dict]]:
    """Parse an OS/8 directory segment taken from a DECtape logical block.

    DECtape frames include a 5-word leader before the directory payload.
    The usable directory begins at words[5:] and uses 6-word entries with
    length stored as two's-complement negative.

    Returns:
        A tuple of (header_dict, list_of_entry_dicts).
    """
    if len(words) < 11:
        raise ValueError("Directory segment too small.")
    leader = words[:5]
    count = abs(signed12(leader[0]))
    origin, link = leader[1], leader[2]
    header = {
        "raw": leader,
        "count": count,
        "origin": origin,
        "link": link,
    }
    entries: List[dict] = []
    idx = 5
    current_block = origin
    entry_idx = 0
    free_blocks = 0
    while entry_idx < count and idx + 5 < len(words):
        name1, name2, name3, name4, date, length_word = words[idx:idx + 6]
        length_signed = signed12(length_word)
        length_blocks = abs(length_signed)
        filename = (
            sixbit_pair(name1)
            + sixbit_pair(name2)
            + sixbit_pair(name3)
        ).rstrip()
        ext = sixbit_pair(name4).rstrip()
        if name1 == 0:
            status = "free"
        elif length_word == 0:
            status = "tentative"
        else:
            status = "file"
        entries.append(
            {
                "index": entry_idx,
                "start_block": current_block,
                "length_blocks": length_blocks,
                "status": status,
                "name": filename,
                "ext": ext,
                "date_raw": date,
                "length_word": length_word,
            }
        )
        current_block += length_blocks
        if status == "free":
            free_blocks += length_blocks
        entry_idx += 1
        idx += 6
        if name1 == 0 and length_word == 0:
            break
    header["free_blocks"] = free_blocks
    return header, entries


def iter_directory(path: str, start_block: int = 1):
    """Iterate over all directory segments in an OS/8 DECtape image.

    Yields (block_number, header, entries) for each directory segment.
    Follows DLINK chain until end or loop detected.
    """
    seen = set()
    block = start_block
    while block and block not in seen:
        seen.add(block)
        words = read_os8_block(path, block)
        header, entries = parse_directory_segment(words)
        yield block, header, entries
        block = header["link"]
        if block == 0:
            break


def list_files(path: str, start_block: int = 1) -> List[dict]:
    """Return a flat list of all file entries from an OS/8 DECtape image."""
    files = []
    for _block, _header, entries in iter_directory(path, start_block):
        for e in entries:
            if e["status"] == "file":
                files.append(e)
    return files


# --- Writing support ---

def char_to_sixbit(c: str) -> int:
    """Convert an ASCII character to its 6-bit value."""
    if c == " " or c == "\0":
        return 0
    if "A" <= c <= "Z":
        return ord(c) - ord("A") + 1
    if "a" <= c <= "z":
        return ord(c) - ord("a") + 1  # treat lowercase as uppercase
    if "0" <= c <= "9":
        return ord(c) - ord("0") + 27
    if c == ".":
        return 46
    return 0  # unknown -> space


def encode_sixbit_pair(s: str) -> int:
    """Encode two characters as a 12-bit word (high char in bits 11-6, low in 5-0)."""
    c1 = s[0] if len(s) > 0 else " "
    c2 = s[1] if len(s) > 1 else " "
    return ((char_to_sixbit(c1) & 0x3F) << 6) | (char_to_sixbit(c2) & 0x3F)


def encode_filename(name: str, ext: str) -> Tuple[int, int, int, int]:
    """Encode a filename (up to 6 chars) and extension (up to 2 chars) as 4 words."""
    name = name.upper().ljust(6)[:6]
    ext = ext.upper().ljust(2)[:2]
    return (
        encode_sixbit_pair(name[0:2]),
        encode_sixbit_pair(name[2:4]),
        encode_sixbit_pair(name[4:6]),
        encode_sixbit_pair(ext[0:2]),
    )


def _pack_words(words: List[int]) -> bytes:
    """Pack 12-bit words into bytes (16-bit little-endian, masked to 12 bits)."""
    return b"".join(struct.pack("<H", w & 0o7777) for w in words)


def write_physical_block(path: str, block: int, words: List[int]) -> None:
    """Write a physical DECtape block (129 words)."""
    if len(words) != BLOCK_WORDS:
        raise ValueError(f"Expected {BLOCK_WORDS} words, got {len(words)}.")
    offset = block * BLOCK_BYTES
    data = _pack_words(words)
    with open(path, "r+b") as f:
        f.seek(offset)
        f.write(data)


def write_os8_block(path: str, block: int, words: List[int]) -> None:
    """Write an OS/8 logical block (256 data words).

    Splits the 256 words across two physical blocks, adding a checksum word
    at the end of each physical block.
    """
    if len(words) != OS8_LOGICAL_WORDS:
        raise ValueError(f"Expected {OS8_LOGICAL_WORDS} words, got {len(words)}.")
    phys0 = 2 * block
    phys1 = phys0 + 1
    # First 128 words + checksum
    w0 = list(words[:128]) + [0]  # checksum placeholder
    # Second 128 words + checksum
    w1 = list(words[128:]) + [0]  # checksum placeholder
    write_physical_block(path, phys0, w0)
    write_physical_block(path, phys1, w1)


def format_empty_directory_block(origin_block: int, free_blocks: int, entries: int = 1) -> List[int]:
    """Create a zeroed OS/8 directory block with a single free entry.

    Args:
        origin_block: First data block managed by the directory.
        free_blocks: Total free blocks starting at origin_block.
        entries: Number of directory entries (default 1 free entry).
    """
    words = [0] * OS8_LOGICAL_WORDS
    if entries < 1:
        entries = 1
    words[0] = (-entries) & 0o7777  # directory entry count (negative)
    words[1] = origin_block & 0o7777  # origin block
    words[2] = 0  # no link to another directory block
    words[3] = 0  # reserved
    words[4] = 0  # reserved/checksum placeholder

    # First directory entry: mark the entire data area free
    entry_off = 5
    words[entry_off + 0] = 0  # name word 1 (0 => free)
    words[entry_off + 1] = 0  # name word 2
    words[entry_off + 2] = 0  # name word 3
    words[entry_off + 3] = 0  # extension
    words[entry_off + 4] = 0  # date
    words[entry_off + 5] = (-free_blocks) & 0o7777  # negative length = free space
    return words


def create_blank_tu56(path: str,
                      logical_blocks: int = DEFAULT_OS8_LOGICAL_BLOCKS,
                      origin_block: int = DEFAULT_OS8_ORIGIN_BLOCK,
                      force: bool = False) -> None:
    """Create a zero-filled .tu56 image with an empty OS/8 directory in block 1."""
    if logical_blocks <= origin_block:
        raise ValueError("logical_blocks must exceed origin_block for usable space.")
    physical_blocks = logical_blocks * 2
    total_bytes = physical_blocks * BLOCK_BYTES
    if os.path.exists(path) and not force:
        raise FileExistsError(f"{path} already exists. Use force=True to overwrite.")

    # Pre-size the file with zeros
    with open(path, "wb") as f:
        f.truncate(total_bytes)

    # Write an empty directory into logical block 1
    free_blocks = logical_blocks - origin_block
    dir_block = format_empty_directory_block(origin_block, free_blocks)
    write_os8_block(path, 1, dir_block)


def find_free_space(path: str, needed_blocks: int, start_block: int = 1) -> Tuple[int, int, int, List[int]]:
    """Find free space in the directory for a new file.

    Returns: (dir_block, entry_word_offset, start_data_block, directory_words)
    Raises ValueError if not enough space.
    """
    for dir_block, header, entries in iter_directory(path, start_block):
        for e in entries:
            if e["status"] == "free" and e["length_blocks"] >= needed_blocks:
                words = read_os8_block(path, dir_block)
                entry_offset = 5 + e["index"] * 6
                return dir_block, entry_offset, e["start_block"], words
    raise ValueError(f"No free space for {needed_blocks} blocks.")


def add_file_to_image(path: str, filename: str, ext: str, data: bytes) -> int:
    """Add a file to an OS/8 DECtape image.

    Args:
        path: Path to the .tu56 image file.
        filename: OS/8 filename (up to 6 characters).
        ext: File extension (up to 2 characters).
        data: Raw file data bytes.

    Returns:
        Number of blocks written.

    Raises:
        ValueError: If there's not enough free space.
    """
    # Calculate blocks needed (256 words = 512 bytes per OS/8 block)
    # OS/8 stores ASCII as one byte per word in the low 8 bits
    words_needed = len(data)
    blocks_needed = (words_needed + OS8_LOGICAL_WORDS - 1) // OS8_LOGICAL_WORDS
    if blocks_needed == 0:
        blocks_needed = 1  # minimum 1 block

    # Find free space
    dir_block, entry_offset, start_block, dir_words = find_free_space(path, blocks_needed)

    # Get the current free entry info
    old_length = abs(signed12(dir_words[entry_offset + 5]))

    # Encode filename
    n1, n2, n3, n4 = encode_filename(filename, ext)

    # Update the directory entry for our new file
    dir_words[entry_offset + 0] = n1
    dir_words[entry_offset + 1] = n2
    dir_words[entry_offset + 2] = n3
    dir_words[entry_offset + 3] = n4
    dir_words[entry_offset + 4] = 0  # date
    dir_words[entry_offset + 5] = (-blocks_needed) & 0o7777  # negative length

    # If there's leftover space, add a new free entry after this one
    leftover = old_length - blocks_needed
    if leftover > 0:
        # Insert a free entry (name1=0 means empty/free)
        next_entry = entry_offset + 6
        if next_entry + 6 <= len(dir_words):
            # Shift existing entries down or create free marker
            dir_words[next_entry + 0] = 0  # empty name = free
            dir_words[next_entry + 1] = 0
            dir_words[next_entry + 2] = 0
            dir_words[next_entry + 3] = 0
            dir_words[next_entry + 4] = 0
            dir_words[next_entry + 5] = (-leftover) & 0o7777

    # Write updated directory
    write_os8_block(path, dir_block, dir_words)

    # Write file data blocks
    # OS/8 stores ASCII with one character per word (low 8 bits)
    file_words = [0] * (blocks_needed * OS8_LOGICAL_WORDS)
    for i, b in enumerate(data):
        if i < len(file_words):
            file_words[i] = b & 0o377

    for blk_idx in range(blocks_needed):
        blk_start = blk_idx * OS8_LOGICAL_WORDS
        blk_end = blk_start + OS8_LOGICAL_WORDS
        write_os8_block(path, start_block + blk_idx, file_words[blk_start:blk_end])

    return blocks_needed
