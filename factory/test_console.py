#!/usr/bin/env python3
"""
Minimal sanity checks for the KL8E console device.

Usage:
    python3 factory/test_console.py [keyboard|teleprinter|echo|interactive]

Each subcommand exercises the real IOT interface rather than relying solely
on host-side helpers.
"""

from __future__ import annotations

import argparse
import ctypes
import sys
import time
from pathlib import Path

# KL8E Console IOT Instructions
# Keyboard (device 03)
KL8E_KEYBOARD_KSF = 0o6031   # Skip if keyboard flag set
KL8E_KEYBOARD_KCC = 0o6032   # Clear keyboard flag
KL8E_KEYBOARD_KRS = 0o6034   # Read keyboard buffer, static
KL8E_KEYBOARD_KRB = 0o6036   # Clear flag + read buffer

# Teleprinter (device 04)  
KL8E_TELEPRINTER_TSF = 0o6041  # Skip if teleprinter flag set
KL8E_TELEPRINTER_TCF = 0o6042  # Clear teleprinter flag
KL8E_TELEPRINTER_TPC = 0o6044  # Load character, static
KL8E_TELEPRINTER_TLS = 0o6046  # Clear flag + load and send character


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Test KL8E console device functionality.")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    parser.set_defaults(command="keyboard")

    # Keyboard subcommand
    subparsers.add_parser("keyboard", help="Test keyboard input functionality with IOT 6031/6036")

    # Teleprinter subcommand
    subparsers.add_parser("teleprinter", help="Test teleprinter output functionality with IOT 6041/6046")

    # Echo subcommand  
    subparsers.add_parser("echo", help="Test echo functionality - read from keyboard and output to teleprinter")

    # Interactive subcommand
    subparsers.add_parser("interactive", help="Interactive console test - type characters and see immediate echo")

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
    # Core PDP-8 API
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

    lib.pdp8_api_get_pc.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_pc.restype = ctypes.c_uint16

    lib.pdp8_api_write_mem.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16]
    lib.pdp8_api_write_mem.restype = ctypes.c_int

    lib.pdp8_api_step.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_step.restype = ctypes.c_int

    lib.pdp8_api_set_ac.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.pdp8_api_set_ac.restype = None

    lib.pdp8_api_get_ac.argtypes = [ctypes.c_void_p]
    lib.pdp8_api_get_ac.restype = ctypes.c_uint16

    # KL8E Console API
    lib.pdp8_kl8e_console_create.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_kl8e_console_create.restype = ctypes.c_void_p

    lib.pdp8_kl8e_console_destroy.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_destroy.restype = None

    lib.pdp8_kl8e_console_attach.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.pdp8_kl8e_console_attach.restype = ctypes.c_int

    lib.pdp8_kl8e_console_queue_input.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
    lib.pdp8_kl8e_console_queue_input.restype = ctypes.c_int

    lib.pdp8_kl8e_console_input_pending.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_input_pending.restype = ctypes.c_size_t

    lib.pdp8_kl8e_console_output_pending.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_output_pending.restype = ctypes.c_size_t

    lib.pdp8_kl8e_console_pop_output.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8)]
    lib.pdp8_kl8e_console_pop_output.restype = ctypes.c_int

    lib.pdp8_kl8e_console_flush.argtypes = [ctypes.c_void_p]
    lib.pdp8_kl8e_console_flush.restype = ctypes.c_int


def execute_iot(lib: ctypes.CDLL, cpu: int, instruction: int, ac: int | None = None) -> int:
    """Execute an IOT instruction and return the resulting accumulator value."""
    if ac is not None:
        lib.pdp8_api_set_ac(cpu, ctypes.c_uint16(ac & 0x0FFF))
    lib.pdp8_api_clear_halt(cpu)
    if (
        lib.pdp8_api_write_mem(
            cpu, ctypes.c_uint16(0), ctypes.c_uint16(instruction & 0x0FFF)
        )
        != 0
    ):
        raise RuntimeError("Failed to write IOT instruction to memory.")
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(0))
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute IOT instruction.")
    return lib.pdp8_api_get_ac(cpu) & 0x0FFF


def test_skip_instruction(lib: ctypes.CDLL, cpu: int, skip_instruction: int) -> bool:
    """Test if a skip instruction actually skips by checking PC advancement."""
    # Clear state
    lib.pdp8_api_clear_halt(cpu)
    
    # Use a high address to avoid conflicts
    test_addr = 100
    
    # Write skip instruction 
    if lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(test_addr), ctypes.c_uint16(skip_instruction)) != 0:
        raise RuntimeError("Failed to write skip instruction.")
    
    # Write NOP at next address
    if lib.pdp8_api_write_mem(cpu, ctypes.c_uint16(test_addr + 1), ctypes.c_uint16(0o7000)) != 0:  # NOP
        raise RuntimeError("Failed to write NOP instruction.")
    
    # Set PC and execute skip instruction
    lib.pdp8_api_set_pc(cpu, ctypes.c_uint16(test_addr))
    
    pc_before = lib.pdp8_api_get_pc(cpu)
    if lib.pdp8_api_step(cpu) == 0:
        raise RuntimeError("Failed to execute skip instruction.")
    pc_after = lib.pdp8_api_get_pc(cpu)
    
    # If skip occurred: PC should be test_addr + 2
    # If no skip: PC should be test_addr + 1
    expected_no_skip = test_addr + 1
    expected_skip = test_addr + 2
    
    if pc_after == expected_skip:
        return True  # Skip occurred
    elif pc_after == expected_no_skip:
        return False  # No skip
    else:
        raise RuntimeError(f"Unexpected PC value: {pc_after}, expected {expected_no_skip} or {expected_skip}")


def test_keyboard_functionality(lib: ctypes.CDLL, cpu: int, console: int) -> None:
    """Test keyboard input functionality using IOT 6031 (KSF) and IOT 6036 (KRB)."""
    print("Testing keyboard functionality...")
    
    # Check initial state - should have no character ready
    print("  Initial state - no character should be ready")
    skip_result = test_skip_instruction(lib, cpu, KL8E_KEYBOARD_KSF)
    print(f"    KSF (6031) skip result: {skip_result} (should be False)")
    
    # Queue a test character
    test_char = ord('A')
    print(f"  Queueing character: '{chr(test_char)}' (0x{test_char:02x})")
    if lib.pdp8_kl8e_console_queue_input(console, ctypes.c_uint8(test_char)) != 0:
        raise RuntimeError("Failed to queue input character.")
    
    # Check pending input count
    pending = lib.pdp8_kl8e_console_input_pending(console)
    print(f"    Pending input characters: {pending}")
    
    # Now KSF should indicate character ready
    print("  After queueing character - should be ready")
    skip_result = test_skip_instruction(lib, cpu, KL8E_KEYBOARD_KSF)
    print(f"    KSF (6031) skip result: {skip_result} (should be True)")
    
    # Read the character using KRB
    print("  Reading character with KRB (6036)")
    ac_result = execute_iot(lib, cpu, KL8E_KEYBOARD_KRB)
    received_char = ac_result & 0x7F
    print(f"    AC result: 0o{ac_result:04o} (0x{ac_result:03x})")
    print(f"    Character received: '{chr(received_char)}' (0x{received_char:02x})")
    
    if received_char == test_char:
        print("    ✓ Character received correctly")
    else:
        print(f"    ✗ Character mismatch: expected 0x{test_char:02x}, got 0x{received_char:02x}")
    
    # After reading, keyboard should no longer be ready
    print("  After reading - should no longer be ready")
    skip_result = test_skip_instruction(lib, cpu, KL8E_KEYBOARD_KSF)
    print(f"    KSF (6031) skip result: {skip_result} (should be False)")
    
    # Test multiple characters
    print("  Testing multiple character queue")
    test_string = "HELLO"
    for char in test_string:
        if lib.pdp8_kl8e_console_queue_input(console, ctypes.c_uint8(ord(char))) != 0:
            raise RuntimeError(f"Failed to queue character '{char}'.")
    
    pending = lib.pdp8_kl8e_console_input_pending(console)
    print(f"    Queued {len(test_string)} characters, pending: {pending}")
    
    # Read all characters
    received_string = ""
    for i in range(len(test_string)):
        # Check that character is ready
        skip_result = test_skip_instruction(lib, cpu, KL8E_KEYBOARD_KSF)
        if not skip_result:
            print(f"    ✗ Character {i+1} not ready when expected")
            break
        
        # Read character
        ac_result = execute_iot(lib, cpu, KL8E_KEYBOARD_KRB)
        char = chr(ac_result & 0x7F)
        received_string += char
    
    print(f"    Sent: '{test_string}', Received: '{received_string}'")
    if received_string == test_string:
        print("    ✓ Multiple character test passed")
    else:
        print("    ✗ Multiple character test failed")


def test_teleprinter_functionality(lib: ctypes.CDLL, cpu: int, console: int) -> None:
    """Test teleprinter output functionality using IOT 6041 (TSF) and IOT 6046 (TLS)."""
    print("Testing teleprinter functionality...")
    
    # Check initial state - should be ready for output
    print("  Initial state - should be ready for output")
    skip_result = test_skip_instruction(lib, cpu, KL8E_TELEPRINTER_TSF)
    print(f"    TSF (6041) skip result: {skip_result} (should be True)")
    
    # Send a test character
    test_char = ord('X')
    print(f"  Sending character: '{chr(test_char)}' (0x{test_char:02x})")
    
    # Use TLS to send character
    execute_iot(lib, cpu, KL8E_TELEPRINTER_TLS, ac=test_char)
    
    # Check if character was captured in output buffer
    output_pending = lib.pdp8_kl8e_console_output_pending(console)
    print(f"    Output pending: {output_pending}")
    
    if output_pending > 0:
        # Pop the output character
        output_char = ctypes.c_uint8()
        if lib.pdp8_kl8e_console_pop_output(console, ctypes.byref(output_char)) == 0:
            received_char = output_char.value
            print(f"    Character in output buffer: '{chr(received_char)}' (0x{received_char:02x})")
            if received_char == test_char:
                print("    ✓ Character sent correctly")
            else:
                print(f"    ✗ Character mismatch: expected 0x{test_char:02x}, got 0x{received_char:02x}")
        else:
            print("    ✗ Failed to pop output character")
    else:
        print("    ✗ No character found in output buffer")
    
    # Test sending a string
    print("  Testing string output")
    test_string = "Hello, PDP-8!"
    
    for char in test_string:
        # Check teleprinter ready
        skip_result = test_skip_instruction(lib, cpu, KL8E_TELEPRINTER_TSF)
        if not skip_result:
            print(f"    ✗ Teleprinter not ready for character '{char}'")
            break
        
        # Send character
        execute_iot(lib, cpu, KL8E_TELEPRINTER_TLS, ac=ord(char))
    
    # Collect all output
    output_string = ""
    while True:
        output_char = ctypes.c_uint8()
        if lib.pdp8_kl8e_console_pop_output(console, ctypes.byref(output_char)) != 0:
            break
        output_string += chr(output_char.value)
    
    print(f"    Sent: '{test_string}'")
    print(f"    Received: '{output_string}'")
    if output_string == test_string:
        print("    ✓ String output test passed")
    else:
        print("    ✗ String output test failed")


def test_echo_functionality(lib: ctypes.CDLL, cpu: int, console: int) -> None:
    """Test combined keyboard input and teleprinter output (echo)."""
    print("Testing echo functionality...")
    
    test_string = "ECHO TEST"
    
    # Queue input characters
    print(f"  Queueing input: '{test_string}'")
    for char in test_string:
        if lib.pdp8_kl8e_console_queue_input(console, ctypes.c_uint8(ord(char))) != 0:
            raise RuntimeError(f"Failed to queue character '{char}'.")
    
    # Echo loop: read from keyboard and write to teleprinter
    echo_result = ""
    for i in range(len(test_string)):
        # Wait for keyboard ready
        if not test_skip_instruction(lib, cpu, KL8E_KEYBOARD_KSF):
            print(f"    ✗ Keyboard not ready for character {i+1}")
            break
        
        # Read character
        char_code = execute_iot(lib, cpu, KL8E_KEYBOARD_KRB) & 0x7F
        
        # Wait for teleprinter ready  
        if not test_skip_instruction(lib, cpu, KL8E_TELEPRINTER_TSF):
            print(f"    ✗ Teleprinter not ready for character {i+1}")
            break
        
        # Echo character
        execute_iot(lib, cpu, KL8E_TELEPRINTER_TLS, ac=char_code)
    
    # Collect echoed output
    while True:
        output_char = ctypes.c_uint8()
        if lib.pdp8_kl8e_console_pop_output(console, ctypes.byref(output_char)) != 0:
            break
        echo_result += chr(output_char.value)
    
    print(f"    Input: '{test_string}'")
    print(f"    Echo:  '{echo_result}'")
    
    if echo_result == test_string:
        print("    ✓ Echo test passed")
    else:
        print("    ✗ Echo test failed")


def test_interactive_console(lib: ctypes.CDLL, cpu: int, console: int) -> None:
    """Interactive console test - requires user input."""
    print("Interactive console test...")
    print("Type characters and press Enter. Type 'quit' to exit.")
    print("Each character should be echoed back immediately.")
    print()
    
    try:
        while True:
            # Get user input
            user_input = input("Enter text: ").strip()
            if user_input.lower() == 'quit':
                break
            
            # Queue all characters
            for char in user_input:
                if lib.pdp8_kl8e_console_queue_input(console, ctypes.c_uint8(ord(char))) != 0:
                    print(f"Failed to queue character '{char}'")
                    continue
            
            # Echo all characters
            echo_output = ""
            for _ in range(len(user_input)):
                # Check if keyboard has character
                if not test_skip_instruction(lib, cpu, KL8E_KEYBOARD_KSF):
                    print("No character ready from keyboard")
                    break
                
                # Read character
                char_code = execute_iot(lib, cpu, KL8E_KEYBOARD_KRB) & 0x7F
                
                # Check if teleprinter is ready
                if not test_skip_instruction(lib, cpu, KL8E_TELEPRINTER_TSF):
                    print("Teleprinter not ready")
                    break
                
                # Send character to teleprinter
                execute_iot(lib, cpu, KL8E_TELEPRINTER_TLS, ac=char_code)
            
            # Collect output
            while True:
                output_char = ctypes.c_uint8()
                if lib.pdp8_kl8e_console_pop_output(console, ctypes.byref(output_char)) != 0:
                    break
                echo_output += chr(output_char.value)
            
            print(f"Echo: '{echo_output}'")
            
    except KeyboardInterrupt:
        print("\nInteractive test interrupted.")


def main() -> int:
    args = parse_args()

    lib = load_library()
    configure_signatures(lib)

    cpu = lib.pdp8_api_create(ctypes.c_size_t(0))
    if not cpu:
        print("Unable to create PDP-8 instance.", file=sys.stderr)
        return 1

    # Create console with NULL file pointers (we'll use the API functions)
    console = lib.pdp8_kl8e_console_create(None, None)
    if not console:
        print("Unable to create KL8E console.", file=sys.stderr)
        lib.pdp8_api_destroy(cpu)
        return 1

    try:
        if lib.pdp8_kl8e_console_attach(cpu, console) != 0:
            print("Failed to attach KL8E console.", file=sys.stderr)
            return 1

        lib.pdp8_api_reset(cpu)

        if args.command == "keyboard":
            try:
                test_keyboard_functionality(lib, cpu, console)
                return 0
            except RuntimeError as exc:
                print(f"Keyboard test failed: {exc}", file=sys.stderr)
                return 1

        elif args.command == "teleprinter":
            try:
                test_teleprinter_functionality(lib, cpu, console)
                return 0
            except RuntimeError as exc:
                print(f"Teleprinter test failed: {exc}", file=sys.stderr)
                return 1

        elif args.command == "echo":
            try:
                test_echo_functionality(lib, cpu, console)
                return 0
            except RuntimeError as exc:
                print(f"Echo test failed: {exc}", file=sys.stderr)
                return 1

        elif args.command == "interactive":
            try:
                test_interactive_console(lib, cpu, console)
                return 0
            except RuntimeError as exc:
                print(f"Interactive test failed: {exc}", file=sys.stderr)
                return 1

        else:
            print(f"Unknown command: {args.command}", file=sys.stderr)
            return 1

    finally:
        if console:
            lib.pdp8_kl8e_console_destroy(console)
        if cpu:
            lib.pdp8_api_destroy(cpu)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())