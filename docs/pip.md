# pip.pa Outline

- **Front matter and memory map**
  - DEC copyright/version header, abstract, and change log for OS/8 PIP V11A.
  - Notes on USR residency, page-zero scratch locations, and core layout for buffers/handlers.
  - Directory printout constants and CD parameter area equates pulled from the monitor.

- **Options and configuration**
  - Enumerates user switches (/A, /B, /C, /D, /E, /F, /G, /I, /L, /O, /S, /T, /Y, /Z, =N, /V)
    and the OS78 flag that disables /E, /F, /L.

- **Monitor interface and buffer equates**
  - Device/buffer base addresses, handler pages, and DCB/MPARAM offsets.
  - Temporary buffers for line editing, squish directories, and handler staging.

- **General character I/O support (field 0)**
  - `IOPEN`/`ICHAR`: input file sequencing, handler assignment, buffer refill, ^Z EOF detection.
  - `OOPEN`/`OUTDMP`/`OCLOSE`: output setup, buffer flush, close/length checks.
  - `OUSETP`, `OTYPE`, `SLASHG`: pointer initialization, device type inspection, error gating.

- **PIP main control flow (field 1)**
  - Entry at `PIPSA`/`PIP`: clears state, processes switches, and dispatches modes.
  - Handles pre-processing switches (/S squish, /Z zero, /D delete, /Y system copy,
    /E/F/L directory listing, /I image, /B binary, else ASCII default).
  - Chain-entry path `NOPCD` skips command decode when CD area already set.

- **ASCII transfer path**
  - `ASCII` sets options, validates output (`ASCI2`/`OUTOPN`), opens inputs, and drives line
    buffering with form-control translation (/C trim blanks, /T simple TTY formatting).
  - Helpers for carriage control, tabs, rubouts, end-of-line, and end-of-file (`ASCEOF`/`ACLOSE`).

- **Shared subroutines and maintenance helpers**
  - `DELETE` (output-side deletes), `DZERO` (directory zeroing with device-length table),
    confirmation logic (`CONFRM`/`NOYES`), and `FIXLEN`/`LEADER` size prep.
  - `PIPERR` central error dispatcher and `OUTOPN` wrapper for output errors.

- **Directory printer (/E, /F, /L)**
  - `DIR`/`DIRPRE` open output, default to `DSK:` when needed, and seed starting blocks.
  - Printing helpers `PRWD`, `PRNUM`, `PCRLF`, and `PDATE` for names, lengths, and dates.
  - `NXTDIR` main loop walks directory segments, skips/prints free entries (`DEMPTY`), and
    respects listing modes; `ENDFUJ` formats the summary of free blocks.

- **Binary mode processor (/B)**
  - `BINARY` handles absolute and relocatable paper-tape formats; drives leader/trailer output
    via `LTCODE`.
  - `RCOPY1` copies frames and advances input; `RELTBL`/`RELBIN` decode relocatable control
    frames; `RELERR`/`INEFER` feed `PIPERR` on fatal input.

- **Error reporting**
  - `ERPRNT`/`ERPCH` render text table entries (`ERR0`–`ERR11`, `DIRMSG`), with file-number
    substitution via `CHPRNT`/`FILENR`.
  - `PDATE` emits creation dates; `TTYOUT` provides a raw console output primitive.

- **SQUISH processor (/S)**
  - `SQUISH` compacts directory holes using the image-transfer loop (`IMTRA`/`SQTRA`), gathers
    device lengths (`GETEQ`), and validates directories (`INTEST`/`SQDTST`).
  - Handles same-device detection (`SETSAM`), waste-word adjustments (`MWAST`/`CYWAST`), and
    prompts “ARE YOU SURE?” before rewriting.

- **System head copy (/Y)**
  - Paths `YOUSYS`/`YSOUT` read system areas, validate headers (`TSTHED`), and optionally move
    directory blocks to open holes (`MOVE`).
  - Uses `TSTIO` to require a directory device, reuses image transfer for output, and resets
    state via `PIPCLR`.

- **Utility guards and one-time setup**
  - `TSTIO` ensures ASCII output targets are directory-capable; `ASCI2`/`AOUERR` gate output
    errors; `ASCPTCH` simulates 8-bit ^Z detection.
  - `ONCE` runs once per invocation to patch chain entry and optionally print the version string
    when `/V` is present.
