#!/usr/bin/env python3
"""
Minimal sanity checks for the paper tape device.

Usage:
    python3 factory/test_papertape.py [skip|select|read|list]

Each subcommand exercises the real IOT interface rather than relying solely
on host-side helpers.
"""

from __future__ import annotations

import argparse
import ctypes
import sys
from pathlib import Path

# Paper tape IOT instructions (067x)
PAPER_TAPE_INSTR_SKIP = 0o6671    # Skip if ready
PAPER_TAPE_INSTR_SELECT = 0o6672  # Select block (AC contains block number)
PAPER_TAPE_INSTR_READ = 0o6674    # Read word from current block
PAPER_TAPE_INSTR_SKIP_SELECT = 0o6673  # Skip if ready + Select
PAPER_TAPE_INSTR_SKIP_READ = 0o6675     # Skip if ready + Read
PAPER_TAPE_INSTR_SELECT_READ = 0o6676   # Select + Read
PAPER_TAPE_INSTR_ALL = 0o6677           # Skip + Select + Read


class PaperTapeStatus(ctypes.Structure):
    _fields_ = [
        ("loaded", ctypes.c_bool),
        ("label", ctypes.c_char_p),
        ("block_count", ctypes.c_size_t),
        ("current_block", ctypes.c_uint16),
        ("current_index", ctypes.c_size_t),
        ("current_word_count", ctypes.c_size_t),
        ("ready", ctypes.c_bool),
    ]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Test paper tape device functionality.")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    parser.set_defaults(command="list")

    # Skip subcommand
    subparsers.add_parser("skip", help="Test IOT 6671 (SKIP) instruction ready status detection")

    # Select subcommand  
    select_parser = subparsers.add_parser("select", help="Test IOT 6672 (SELECT) to select a block")
    select_parser.add_argument("block", type=str, help="Block number to select (octal)")

    # Read subcommand
    read_parser = subparsers.add_parser("read", help="Test IOT 6674 (READ) to read words from current block")
    read_parser.add_argument("--block", type=str, help="Block number to select first (octal)")
    read_parser.add_argument("--count", type=int, default=16, help="Maximum words to read")

    # List subcommand
    subparsers.add_parser("list", help="List all blocks in the loaded paper tape")

    return parser.parse_args()


def load_library() -> ctypes.CDLL:
    factory_dir = Path(__file__).resolve().parent
    lib_path = factory_dir / "libpdp8.so"
    if not lib_path.exists():
        print(
            f"{lib_path} not found. Build it with "
            "cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o libpdp8.so",
            file=sys.stderr,
        )
        raise SystemExit(1)
    try:
        return ctypes.CDLL(str(lib_path))
    except OSError as exc:
        print(f"Failed to load {lib_path}: {exc}", file=sys.stderr)
        raise SystemExit(1)


def configure_signatures(lib: ctypes.CDLL) -> None:
    lib.pdp8_api_create.argtypes = [ctypes.c_size_t]
    lib.pdp8_api_create.restype = ctypes.c_void_p

    lib.pdp8_api_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_destroy.restype = None

    lib.pdp8_api_reset.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_reset.restype = None

    lib.pdp8_api_clear_halt.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_clear_halt.restype = None

    lib.pdp8_api_set_pc.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_pc.restype = None

    lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
    lib.pdp8_api_write_mem.restype = ctypes.c_int

    lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_step.restype = ctypes.c_int

    lib.pdp8_api_set_ac.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_ac.restype = None

    lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_ac.restype = ctypes.c_uint16

    lib.pdp8_api_get_pc.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_pc.restype = ctypes.c_uint16

    lib.pdp8_paper_tape_device_create.restype = ctypes.c_void_p
    lib.pdp8_paper_tape_device_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_paper_tape_device_destroy.restype = None

    lib.pdp8_paper_tape_device_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_paper_tape_device_attach.restype = ctypes.c_int

    lib.pdp8_paper_tape_device_load.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.pdp8_paper_tape_device_load.restype = ctypes.c_int

    lib.pdp8_paper_tape_device_label.argtypes = [ctypes.c_void_p]
    lib.pdp8_paper_tape_device_label.restype = ctypes.c_char_p


def test_skip_ready(lib: ctypes.CDLL, cpu: int, device: int) -> bool:
    """Test if paper tape device reports ready status via IOT 6671."""
    # Load a simple program: IOT 6671 (skip if ready), HLT, HLT
    lib.pdp8_api_write_mem(cpu, 0o200, PAPER_TAPE_INSTR_SKIP)
    lib.pdp8_api_write_mem(cpu, 0o201, 0o7402)  # HLT
    lib.pdp8_api_write_mem(cpu, 0o202, 0o7402)  # HLT

    lib.pdp8_api_set_pc(cpu, 0o200)
    lib.pdp8_api_clear_halt(cpu)
    
    # Execute the SKIP instruction and then read PC to see if it skipped
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute SKIP instruction")

    pc = lib.pdp8_api_get_pc(cpu)
    # If skip occurred PC will have advanced past the first HLT (0o201 -> 0o202)
    return (pc & 0o7777) == 0o202


def test_select_block(lib: ctypes.CDLL, cpu: int, device: int, block_num: int) -> None:
    """Test selecting a block via IOT 6672."""
    # Load AC with block number
    lib.pdp8_api_set_ac(cpu, block_num & 0o7777)
    
    # Load a simple program: IOT 6672 (select block), HLT
    lib.pdp8_api_write_mem(cpu, 0o200, PAPER_TAPE_INSTR_SELECT)
    lib.pdp8_api_write_mem(cpu, 0o201, 0o7402)  # HLT

    lib.pdp8_api_set_pc(cpu, 0o200)
    lib.pdp8_api_clear_halt(cpu)
    
    # Execute the select instruction
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError(f"Failed to execute SELECT instruction for block {block_num:04o}")
    
    print(f"Selected block {block_num:04o}")


def test_read_words(lib: ctypes.CDLL, cpu: int, device: int, max_words: int = 16) -> list[int]:
    """Test reading words from current block via IOT 6674."""
    words = []
    
    for i in range(max_words):
        # Check if ready first
        if not test_skip_ready(lib, cpu, device):
            print(f"Device not ready after reading {i} words")
            break
            
        # Load a simple program: IOT 6674 (read word), HLT
        lib.pdp8_api_write_mem(cpu, 0o200, PAPER_TAPE_INSTR_READ)
        lib.pdp8_api_write_mem(cpu, 0o201, 0o7402)  # HLT

        lib.pdp8_api_set_pc(cpu, 0o200)
        lib.pdp8_api_clear_halt(cpu)
        
        # Execute the read instruction
        if lib.pdp8_api_step(cpu) == 0:
            print(f"Failed to execute READ instruction at word {i}")
            break
            
        # Get the word from AC
        word = lib.pdp8_api_get_ac(cpu)
        words.append(word)
        
    return words


def get_tape_path() -> str:
    """Get the paper tape path from pdp8.config or use default."""
    config_path = Path("pdp8.config")
    if config_path.exists():
        with open(config_path, 'r') as f:
            in_paper_tape_section = False
            for line in f:
                line = line.strip()
                if line.startswith("device paper_tape"):
                    in_paper_tape_section = True
                elif in_paper_tape_section and line.startswith("image ="):
                    return line.split("=", 1)[1].strip()
                elif in_paper_tape_section and line.startswith("}"):
                    break
    
    # Default to demo tape
    return "tapes/tp_demo.tape"


def main() -> int:
    args = parse_args()
    
    lib = load_library()
    configure_signatures(lib)
    
    cpu = None
    device = None
    
    try:
        # Create CPU and device
        cpu = lib.pdp8_api_create(4096)
        if not cpu:
            print("Failed to create CPU", file=sys.stderr)
            return 1
            
        device = lib.pdp8_paper_tape_device_create()
        if not device:
            print("Failed to create paper tape device", file=sys.stderr)
            return 1
            
        # Attach device to CPU
        if lib.pdp8_paper_tape_device_attach(cpu, device) != 0:
            print("Failed to attach paper tape device", file=sys.stderr)
            return 1
            
        # Load tape
        tape_path = get_tape_path()
        tape_path_bytes = tape_path.encode('utf-8')
        if lib.pdp8_paper_tape_device_load(device, tape_path_bytes) != 0:
            print(f"Failed to load paper tape from {tape_path}", file=sys.stderr)
            return 1
            
        print(f"Loaded paper tape: {tape_path}")
        
        # Get tape label
        label = lib.pdp8_paper_tape_device_label(device)
        if label:
            print(f"Tape label: {label.decode('utf-8')}")
        
        # Execute command
        if args.command == "skip":
            ready = test_skip_ready(lib, cpu, device)
            print(f"Paper tape device ready: {ready}")
            return 0
            
        elif args.command == "select":
            try:
                block_num = int(args.block, 8)  # Parse as octal
                test_select_block(lib, cpu, device, block_num)
                
                # Test if ready after selection
                ready = test_skip_ready(lib, cpu, device)
                print(f"Device ready after selecting block {block_num:04o}: {ready}")
                return 0
            except ValueError:
                print(f"Invalid block number: {args.block}", file=sys.stderr)
                return 1
                
        elif args.command == "read":
            if args.block:
                try:
                    block_num = int(args.block, 8)  # Parse as octal
                    test_select_block(lib, cpu, device, block_num)
                except ValueError:
                    print(f"Invalid block number: {args.block}", file=sys.stderr)
                    return 1
                    
            words = test_read_words(lib, cpu, device, args.count)
            if not words:
                print("No words read from paper tape")
            else:
                print(f"Read {len(words)} words:")
                for i, word in enumerate(words):
                    # Display in octal and as ASCII if printable
                    ascii_char = chr(word) if 32 <= word <= 126 else '.'
                    print(f"  {i:3d}: {word:04o} ({word:4d}) '{ascii_char}'")
            return 0
            
        elif args.command == "list":
            # For list command, we need to examine the tape structure
            # We'll check only the first few blocks since the device seems to 
            # accept any block number. Real blocks typically start from 1.
            print("Checking first 10 blocks for content...")
            found_blocks = []
            
            for block_num in range(10):  # Check blocks 0-9
                try:
                    # Try to select this block
                    lib.pdp8_api_set_ac(cpu, block_num)
                    lib.pdp8_api_write_mem(cpu, 0o200, PAPER_TAPE_INSTR_SELECT)
                    lib.pdp8_api_write_mem(cpu, 0o201, 0o7402)  # HLT
                    lib.pdp8_api_set_pc(cpu, 0o200)
                    lib.pdp8_api_clear_halt(cpu)
                    
                    if lib.pdp8_api_step(cpu) != 0:
                        # Check if device is ready after selection
                        if test_skip_ready(lib, cpu, device):
                            # Try to read a few words to see content
                            words = test_read_words(lib, cpu, device, 16)
                            if words:
                                # Check if words have meaningful content (not all zeros)
                                non_zero_words = [w for w in words if w != 0]
                                if non_zero_words:
                                    found_blocks.append((block_num, words))
                except:
                    continue
                    
            if not found_blocks:
                print("No blocks with content found in paper tape")
            else:
                print(f"Found {len(found_blocks)} blocks with content:")
                for block_num, words in found_blocks:
                    word_preview = " ".join(f"{w:04o}" for w in words[:64])
                    print(f"  Block {block_num:04o}: {word_preview}...")
            return 0
            
    finally:
        if device:
            lib.pdp8_paper_tape_device_destroy(device)
        if cpu:
            lib.pdp8_api_destroy(cpu)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())