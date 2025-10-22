"""
Minimal PDP-8 emulator skeleton for CircuitPython.

This module sketches out the basic data structures and control flow
needed for a PDP-8 style fetch/decode/execute loop. The implementation
is deliberately incomplete so it can serve as a starting point for
interactive experiments on a CircuitPython board.
"""

from __future__ import annotations

import time

WORD_MASK = 0x0FFF  # PDP-8 uses 12-bit words.
MEMORY_SIZE = 4096  # Core memory size in words.
MAJOR_OPCODE_MASK = 0x0E00  # Top bits that select the instruction class.
HLT_OPCODE = 0o7402  # Group 2 operate with the halt bit set.
IAC_OPCODE = 0o7001  # Increment accumulator (IAC micro-op).
GROUP_SELECT_BIT = 0o0400  # Distinguish operate group 1 vs 2.
INDIRECT_BIT = 0o400
CURRENT_PAGE_BIT = 0o200
ADDRESS_MASK = 0o177
PAGE_MASK = 0o7600
AUTO_INDEX_LOW = 0o0010
AUTO_INDEX_HIGH = 0o0017
IOT_DEVICE_MASK = 0o077
IOT_SHIFT = 3
TTY_DEVICE_CODE = 0o04  # KL8E teletype device code.
TTY_DELAY_SECONDS = 0.05  # Approximate 20 cps output pacing.


class PDP8Emulator:
    """Rudimentary PDP-8 core with placeholders for CPU logic."""

    def __init__(self) -> None:
        # CircuitPython does not support array("H") bit masking by default,
        # so a Python list keeps the implementation simple for now.
        self.memory = [0] * MEMORY_SIZE
        self.pc = 0  # Program counter.
        self.ac = 0  # Accumulator.
        self.link = 0  # Single-bit carry flag.
        self.halted = False
        self.skip_pending = False
        self.switch_register = 0  # Front panel switches unused in this sketch.
        self.output = []  # Captured teletype output.

    def reset(self) -> None:
        """Return registers and core memory to a known state."""
        for i in range(MEMORY_SIZE):
            self.memory[i] = 0
        self.pc = 0
        self.ac = 0
        self.link = 0
        self.halted = False
        self.skip_pending = False
        self.switch_register = 0
        self.output = []

    def load(self, origin: int, words) -> None:
        """Deposit a sequence of words starting at the given origin."""
        addr = origin & (MEMORY_SIZE - 1)
        for word in words:
            self.memory[addr] = word & WORD_MASK
            addr = (addr + 1) & (MEMORY_SIZE - 1)

    def step(self) -> None:
        """Fetch and execute a single instruction placeholder."""
        if self.halted:
            return
        opcode = self.memory[self.pc & (MEMORY_SIZE - 1)]
        self.pc = (self.pc + 1) & (MEMORY_SIZE - 1)
        handled = dispatch_instruction(self, opcode)
        if self.halted:
            return
        if not handled:
            # TODO: Decode opcodes and implement PDP-8 instructions.
            # For now, simply leave a trace so callers can see progress.
            self.ac = (self.ac + opcode) & WORD_MASK
        self.apply_skip()

    def run(self, steps: int = 1) -> None:
        """Execute a bounded number of instructions."""
        for _ in range(steps):
            if self.halted:
                break
            self.step()

    def apply_skip(self) -> None:
        """Advance the PC when an instruction schedules a skip."""
        if self.skip_pending:
            self.pc = (self.pc + 1) & (MEMORY_SIZE - 1)
            self.skip_pending = False


EMULATOR = PDP8Emulator()


def demo() -> None:
    """
    Quick manual test to confirm the skeleton is wired correctly.

    On CircuitPython you can run this from the REPL:

        >>> import pdp8
        >>> pdp8.demo()
    """
    EMULATOR.reset()
    EMULATOR.load(0, [1, 2, 3, 4])
    EMULATOR.run(steps=4)
    print("AC=", EMULATOR.ac)


def mask_word(value: int) -> int:
    """Constrain a value to the PDP-8 12-bit word width."""
    return value & WORD_MASK


def dispatch_instruction(emulator, opcode) -> bool:
    """Route an opcode to its major-class handler."""
    major = opcode & MAJOR_OPCODE_MASK
    if major in DISPATCH_TABLE:
        return DISPATCH_TABLE[major](emulator, opcode)
    return instruction_not_implemented(emulator, opcode)


def instruction_not_implemented(_emulator, opcode) -> bool:
    """Placeholder handler until the PDP-8 instruction set is wired in."""
    _ = opcode  # Keep the placeholder explicit for now.
    return False


def effective_address(emulator, opcode) -> int:
    """Compute the effective address for a memory reference instruction."""
    offset = opcode & ADDRESS_MASK
    if opcode & CURRENT_PAGE_BIT:
        base = emulator.pc & PAGE_MASK
    else:
        base = 0
    address = mask_word(base | offset)
    if opcode & INDIRECT_BIT:
        pointer = address
        if AUTO_INDEX_LOW <= pointer <= AUTO_INDEX_HIGH:
            emulator.memory[pointer] = mask_word(emulator.memory[pointer] + 1)
        address = mask_word(emulator.memory[pointer])
    return address


def operate_group1(emulator, opcode) -> None:
    """Implement Group 1 operate micro-ops."""
    if opcode & 0o0200:  # CLA
        emulator.ac = 0
    if opcode & 0o0100:  # CLL
        emulator.link = 0
    if opcode & 0o0040:  # CMA
        emulator.ac = mask_word(~emulator.ac)
    if opcode & 0o0020:  # CML
        emulator.link ^= 1

    rotate_twice = False
    rotate_right = False
    rotate_left = False

    if opcode & 0o0002:
        if (opcode & (0o0010 | 0o0004)) == 0:
            high = (emulator.ac & 0o77) << 6
            low = (emulator.ac >> 6) & 0o77
            emulator.ac = mask_word(high | low)
        else:
            rotate_twice = True
    if opcode & 0o0010:
        rotate_right = True
    if opcode & 0o0004:
        rotate_left = True

    if rotate_right and rotate_left:
        rotate_right = False
        rotate_left = False

    if rotate_right or rotate_left:
        rotations = 2 if rotate_twice else 1
        combined = ((emulator.link & 0x1) << 12) | emulator.ac
        while rotations:
            rotations -= 1
            if rotate_right:
                carry = combined & 0x1
                combined = (combined >> 1) | (carry << 12)
            else:
                carry = (combined >> 12) & 0x1
                combined = ((combined << 1) & 0x1FFF) | carry
        emulator.link = (combined >> 12) & 0x1
        emulator.ac = mask_word(combined)

    if opcode & 0o0001:  # IAC
        combined = ((emulator.link & 0x1) << 12) | emulator.ac
        combined = (combined + 1) & 0x1FFF
        emulator.link = (combined >> 12) & 0x1
        emulator.ac = mask_word(combined)


def operate_group2(emulator, opcode) -> None:
    """Implement Group 2 operate (skip and I/O) micro-ops."""
    if opcode & 0o0200:  # CLA
        emulator.ac = 0

    sense = (opcode & 0o0010) != 0
    conditions = [
        (opcode & 0o0100) and (emulator.ac & 0o4000),  # SMA checks sign bit
        (opcode & 0o0040) and (emulator.ac == 0),      # SZA
        (opcode & 0o0020) and (emulator.link != 0),    # SNL
    ]

    any_true = False
    for condition in conditions:
        any_true |= bool(condition)

    if opcode & 0o0004:  # OSR
        emulator.ac = mask_word(emulator.ac | emulator.switch_register)
    if opcode & 0o0002:  # HLT
        emulator.halted = True

    skip = not any_true if sense else any_true
    if skip:
        emulator.skip_pending = True


def operate_instruction(emulator, opcode) -> bool:
    """Dispatch to Group 1 or Group 2 operate micro-ops."""
    if opcode == HLT_OPCODE:
        emulator.halted = True
        return True
    if opcode == IAC_OPCODE:
        combined = ((emulator.link & 0x1) << 12) | emulator.ac
        combined = (combined + 1) & 0x1FFF
        emulator.link = (combined >> 12) & 0x1
        emulator.ac = mask_word(combined)
        return True
    if opcode & GROUP_SELECT_BIT:
        operate_group2(emulator, opcode)
    else:
        operate_group1(emulator, opcode)
    return True


def memory_reference_and(emulator, opcode) -> bool:
    address = effective_address(emulator, opcode)
    emulator.ac = mask_word(emulator.ac & emulator.memory[address])
    return True


def memory_reference_tad(emulator, opcode) -> bool:
    address = effective_address(emulator, opcode)
    value = emulator.memory[address]
    total = emulator.ac + value
    if total & 0x1000:
        emulator.link ^= 1
    emulator.ac = mask_word(total)
    return True


def memory_reference_isz(emulator, opcode) -> bool:
    address = effective_address(emulator, opcode)
    emulator.memory[address] = mask_word(emulator.memory[address] + 1)
    if emulator.memory[address] == 0:
        emulator.skip_pending = True
    return True


def memory_reference_dca(emulator, opcode) -> bool:
    address = effective_address(emulator, opcode)
    emulator.memory[address] = mask_word(emulator.ac)
    emulator.ac = 0
    return True


def memory_reference_jms(emulator, opcode) -> bool:
    target = effective_address(emulator, opcode)
    emulator.memory[target] = mask_word(emulator.pc)
    emulator.pc = (target + 1) & (MEMORY_SIZE - 1)
    return True


def memory_reference_jmp(emulator, opcode) -> bool:
    emulator.pc = effective_address(emulator, opcode)
    return True


def handle_iot(emulator, opcode) -> bool:
    device = (opcode >> IOT_SHIFT) & IOT_DEVICE_MASK
    if device == TTY_DEVICE_CODE:
        operations = opcode & 0o7
        if operations & 0o1:  # Skip if ready
            emulator.skip_pending = True
        if operations & 0o4:  # Transmit character
            ch = chr(emulator.ac & 0x7F)
            emulator.output.append(ch)
            print(ch, end="")
            time.sleep(TTY_DELAY_SECONDS)
        return True
    return instruction_not_implemented(emulator, opcode)


DISPATCH_TABLE = {
    0o0000: memory_reference_and,
    0o1000: memory_reference_tad,
    0o2000: memory_reference_isz,
    0o3000: memory_reference_dca,
    0o4000: memory_reference_jms,
    0o5000: memory_reference_jmp,
    0o6000: handle_iot,
    0o7000: operate_instruction,
}


if __name__ == "__main__":
    demo()
