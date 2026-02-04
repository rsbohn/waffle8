# PDP-8 WebAssembly Build

This directory contains the WebAssembly build of the PDP-8 emulator, configured to run the "dull boy" demo by default.

## Overview

The WebAssembly build provides a self-contained, browser-based PDP-8 emulator that runs entirely in the web browser without requiring any backend server (except for serving the static files). This approach is inspired by projects like wanix that demonstrate running complete systems in the browser using WebAssembly.

## Features

- **Self-contained**: The emulator and ROM image are compiled into a single WebAssembly module
- **No backend required**: Once loaded, runs entirely in the browser
- **Pre-loaded program**: Includes the "dull boy" demo (prints "All work and no play makes Jack a dull boy" repeatedly)
- **Interactive controls**: Start, stop, reset, and step through execution
- **Register display**: View PC, AC, LINK registers in real-time
- **Console output**: See the program output as it executes

## Building

### Prerequisites

- Emscripten SDK installed (`emcc` command available)
- Standard C compiler tools

### Build Steps

1. Install Emscripten (if not already installed):
   ```bash
   # On Ubuntu/Debian
   sudo apt-get install emscripten
   ```

2. Build the WebAssembly module:
   ```bash
   cd wasm
   ./build.sh
   ```

This will generate:
- `pdp8.js` - JavaScript loader and glue code
- `pdp8.wasm` - WebAssembly binary containing the emulator and ROM

## Running

1. Serve the files with any HTTP server:
   ```bash
   cd wasm
   python3 -m http.server 8080
   ```

2. Open your browser to:
   ```
   http://localhost:8080/
   ```

3. The emulator will automatically:
   - Load the WebAssembly module
   - Initialize the PDP-8 CPU
   - Load the dull boy program
   - Start execution

## Architecture

### Files

- `pdp8_wasm.c` - WebAssembly interface wrapper that:
  - Embeds the dull-boy ROM image as a C array
  - Provides JavaScript-callable functions
  - Captures console output to a buffer
  - Manages emulator lifecycle

- `build.sh` - Build script that compiles all emulator sources with Emscripten

- `index.html` - Self-contained web page with:
  - Embedded styles (no external CSS)
  - JavaScript interface to the WASM module
  - Interactive controls and display

### Exported Functions

The following C functions are exported to JavaScript:

- `pdp8_init()` - Initialize emulator and load dull boy program
- `pdp8_reset()` - Reset emulator to initial state
- `pdp8_step(cycles)` - Execute specified number of CPU cycles
- `pdp8_is_halted()` - Check if CPU is halted
- `pdp8_get_pc()` - Get program counter
- `pdp8_get_ac()` - Get accumulator
- `pdp8_get_link()` - Get link register
- `pdp8_get_output()` - Get console output buffer
- `pdp8_clear_output()` - Clear output buffer
- `pdp8_cleanup()` - Free resources

## Inspiration from Wanix

This build takes inspiration from the wanix project, which demonstrates running a complete Unix-like system in the browser:

1. **Self-contained**: Everything needed is compiled into the WASM bundle
2. **No backend dependencies**: Runs entirely client-side
3. **Embedded resources**: ROM image is compiled directly into the binary
4. **Single-page application**: One HTML file with embedded styles and scripts

## Default Program: Dull Boy

The emulator is configured to automatically load and run the "dull boy" demo, which:

- Prints "All work and no play makes Jack a dull boy." repeatedly
- Demonstrates console I/O via the KL8E teleprinter
- Uses the watchdog timer for timing delays
- Runs continuously until stopped by the user

The program starts at address 0x0100 (octal 0200) and includes:
- Character-by-character output with delays for dramatic effect
- Watchdog timer configuration and restart
- Inline string printing routines

## Technical Details

### Memory Layout

- ROM loaded at: 0x0100 (256 decimal, 0200 octal)
- ROM size: ~100 words (12-bit PDP-8 words)
- Total memory: 4K words (standard PDP-8 configuration)

### Build Configuration

- C standard: C11
- Optimization: -O3
- WASM features:
  - Memory growth allowed
  - Modularized output
  - Web environment only

### Browser Compatibility

Tested on modern browsers with WebAssembly support:
- Chrome/Chromium 57+
- Firefox 52+
- Safari 11+
- Edge 16+

## Customization

To change the default program:

1. Edit `pdp8_wasm.c` and modify the `dullboy_rom[]` array
2. Update `DULLBOY_ROM_SIZE` and `DULLBOY_START_ADDRESS` as needed
3. Rebuild with `./build.sh`

Alternatively, you could extend the interface to support loading S-record files dynamically from JavaScript.
