#!/bin/bash
# Build PDP-8 emulator for WebAssembly

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_ROOT/src/emulator"
WASM_DIR="$SCRIPT_DIR"
OUTPUT_DIR="$WASM_DIR"

echo "Building PDP-8 WebAssembly emulator..."

# Source files needed for the emulator
EMULATOR_SOURCES=(
    "$SRC_DIR/main.c"
    "$SRC_DIR/board.c"
    "$SRC_DIR/kl8e_console.c"
    "$SRC_DIR/line_printer.c"
    "$SRC_DIR/watchdog.c"
    "$SRC_DIR/interrupt_control.c"
    "$SRC_DIR/paper_tape.c"
    "$SRC_DIR/paper_tape_device.c"
    "$SRC_DIR/paper_tape_punch.c"
    "$SRC_DIR/magtape_device.c"
    "$WASM_DIR/pdp8_wasm.c"
)

# Emscripten compiler flags
EMCC_FLAGS=(
    -std=c11
    -Wall
    -Wextra
    -O3
    -I"$SRC_DIR"
    -I"$PROJECT_ROOT/src"
    -s WASM=1
    -s EXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString"]'
    -s EXPORTED_FUNCTIONS='["_pdp8_init","_pdp8_reset","_pdp8_step","_pdp8_is_halted","_pdp8_get_pc","_pdp8_get_ac","_pdp8_get_link","_pdp8_get_output","_pdp8_get_output_length","_pdp8_clear_output","_pdp8_cleanup","_malloc","_free"]'
    -s ALLOW_MEMORY_GROWTH=1
    -s MODULARIZE=1
    -s EXPORT_NAME='createPDP8Module'
    -s ENVIRONMENT='web'
    --no-entry
)

# Build the WebAssembly module
emcc "${EMCC_FLAGS[@]}" "${EMULATOR_SOURCES[@]}" -o "$OUTPUT_DIR/pdp8.js"

echo "Build complete!"
echo "Output files:"
echo "  - $OUTPUT_DIR/pdp8.js"
echo "  - $OUTPUT_DIR/pdp8.wasm"
