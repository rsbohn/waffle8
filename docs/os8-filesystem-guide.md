# OS/8 Filesystem Guide (from `pip.pa`)

This note summarizes what the PIP source (`pip.pa`) reveals about the OS/8 on‑disk format and how PIP manipulates it.

## Blocks, devices, and directories
- OS/8 uses 256-word blocks. PIP treats “directory devices” (disks, DECtape, etc.) as random-access targets; sequential devices (TTY, PTR, PTP) are excluded from directory work (`OTYPE` returns a negative DCB word for directory devices).
- The directory lives in record 1 (block 1). When zeroing, PIP writes a 6-block template starting at block 1 (`DZERO` uses control word `5410` with source `DIRECT`).
- PIP keeps a device-length table (`DEVLEN`) keyed by device type codes (RK08 ~3248 blocks, RF08/DF32 variants, DECtape/LINCTape, RX01, etc.). A zero entry marks a non-directory device and rejects `/Z`, `/E`, `/F`, `/L`, `/S`, or `/Y` on it.
- File storage origin is patched into the directory image: non-system zeros start at block 07 (octal); system zeros (`/Y` or handler 7607) start at block 061 to leave room for the OS/8 system area.
- The `=N` option (mod 100) sets “waste words” per directory entry (`DWASTE`) so you can grow entries later without moving data.
- DECtape (`.tu56`) specifics: each physical frame is 129 words (258 bytes). OS/8 logical blocks are 256 words built from two frames; drop the checksum word from each frame and concatenate. DECtape directories carry a 5-word leader at the front of each logical block: count (two’s-complement negative), origin, link, waste, and 7777 as a marker.

## Directory segment layout
Each directory consists of one or more segments; the first starts at block 1 and links forward via block numbers.

Segment header (first four words as read by `NXTDIR`):
1. Entry count (`DCOUNT`) – number of directory entries in this segment.
2. Starting data block (`BLOKNO`) – first block available for file storage within this segment.
3. Link to next segment (`DLINK`) – block number of the next segment, or 0 if this is the last.
4. Waste word count (`WASTE`) – extra words reserved after each entry’s date (set via `=N` or when zeroing).

## Directory entry layout
After the header, entries appear consecutively (including any waste padding):
- Name: three 12-bit sixbit words (`NAME1`–`NAME3`).
- Extension: one sixbit word (`NAME4`); 0 prints as blanks, nonzero prints with a leading period.
- Creation date: one packed date word (`DDATE`) rendered by `PDATE` as three numeric fields separated by “/”.
- Waste: `WASTE` words (often 0) reserved per entry.
- Length: one word (`FLENGT`); positive for a permanent file, 0 for tentative entries (ignored), negative for free space.

Interpretation rules in PIP:
- A zero name word marks a free entry; the negative length is negated to accumulate the free-block count.
- Tentative entries (length = 0) are skipped.
- For permanent files, `BLOKNO` is the starting block; after printing an entry, `BLOKNO` is incremented by the file length to locate the next file in the segment.
- If `DLINK` is nonzero, `NXTDIR` jumps to that block and repeats the scan, producing a multi-segment listing.

## Operations that reshape the directory
- `/Z` zero directory: validates the device, writes the template from `DIRECT`, sets `DFORG` (file storage origin), `DWASTE` (from `=N`), and `DLENGT` (device length minus origin). System zero asks “ZERO SYS?” unless `/O` is present.
- `/S` squish: copies the directory into work buffers (`SQBUF1`/`SQBUF2`), compacts free space, rewrites segments, and reuses the image-mode transfer loop to move file data when needed. It respects waste words and detects same-device squishes to avoid clobbering.
- `/E`, `/F`, `/L` directory listings: share the printer pipeline (`DIRPRE`, `NXTDIR`). `/E` lists free entries, `/L` suppresses free entries, `/F` prints names only (no lengths).
- `/Y` system area copy: validates system headers (`TSTHED`), copies system blocks (records 0–15) and directory, and can fall back to an image transfer if an output device is provided.

## Date encoding
PIP stores a packed date word per entry (`DDATE`). The `PDATE` routine splits it into three numeric fields separated by slashes (ASCII 057). The high-order field comes from the upper bits; the low-order field uses the low three bits. The exact epoch isn’t hard-coded in PIP, so use PIP’s output as the authoritative rendering.

## Error handling highlights
- Errors such as “DEVICE NOT A DIRECTORY DEVICE”, “BAD DIRECTORY ON DEVICE”, “DIRECTORY ERROR”, or “NO ROOM FOR OUTPUT FILE” come from `PIPERR` using message table `ERR0`–`ERR11`.
- `TSTIO` is the guard for “is the output a directory device?” when opening ASCII outputs or running `/Z`/`/S`.
- `SLASHG` handles `/G` (ignore errors) and suppresses hard I/O faults when allowed.

## Tools
- `tools/DECtape` reads SIMH `.tu56` images with the DECtape framing described above. Examples:
  - `python tools/DECtape dir media/v3d-tc08.tu56` — print the OS/8 directory (uses logical blocks).
  - `python tools/DECtape read media/v3d-tc08.tu56 1` — dump a physical 129-word frame.
