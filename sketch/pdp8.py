"""
Minimal PDP-8 emulator skeleton for CircuitPython.

This module sketches out the basic data structures and control flow
needed for a PDP-8 style fetch/decode/execute loop. The implementation
is deliberately incomplete so it can serve as a starting point for
interactive experiments on a CircuitPython board.
"""

from __future__ import annotations

WORD_MASK = 0x0FFF  # PDP-8 uses 12-bit words.
MEMORY_SIZE = 4096  # Core memory size in words.
MAJOR_OPCODE_MASK = 0x0E00  # Top bits that select the instruction class.
HLT_OPCODE = 0o7402  # Group 2 operate with the halt bit set.
IAC_OPCODE = 0o7001  # Increment accumulator (IAC micro-op).
INDIRECT_BIT = 0o400
CURRENT_PAGE_BIT = 0o200
ADDRESS_MASK = 0o177
PAGE_MASK = 0o7600
AUTO_INDEX_LOW = 0o0010
AUTO_INDEX_HIGH = 0o0017


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

    def reset(self) -> None:
        """Return registers and core memory to a known state."""
        for i in range(MEMORY_SIZE):
            self.memory[i] = 0
        self.pc = 0
        self.ac = 0
        self.link = 0
        self.halted = False

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

    def run(self, steps: int = 1) -> None:
        """Execute a bounded number of instructions."""
        for _ in range(steps):
            if self.halted:
                break
            self.step()


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
    address = (base | offset) & WORD_MASK
    if opcode & INDIRECT_BIT:
        pointer = address
        if AUTO_INDEX_LOW <= pointer <= AUTO_INDEX_HIGH:
            emulator.memory[pointer] = (emulator.memory[pointer] + 1) & WORD_MASK
        address = emulator.memory[pointer] & WORD_MASK
    return address


def operate_group(emulator, opcode) -> bool:
    """Handle Group 1 operate instructions, including HLT."""
    if opcode == HLT_OPCODE:
        emulator.halted = True
        return True
    if opcode == IAC_OPCODE:
        combined = ((emulator.link & 0x1) << 12) | (emulator.ac & WORD_MASK)
        combined = (combined + 1) & 0x1FFF
        emulator.link = (combined >> 12) & 0x1
        emulator.ac = combined & WORD_MASK
        return True
    return instruction_not_implemented(emulator, opcode)


def memory_reference_jms(emulator, opcode) -> bool:
    """Jump to subroutine, storing the return address."""
    target = effective_address(emulator, opcode)
    emulator.memory[target] = emulator.pc & WORD_MASK
    emulator.pc = (target + 1) & (MEMORY_SIZE - 1)
    return True


def memory_reference_jmp(emulator, opcode) -> bool:
    """Jump to the computed effective address."""
    emulator.pc = effective_address(emulator, opcode)
    return True


DISPATCH_TABLE = {
    0o0000: instruction_not_implemented,
    0o1000: instruction_not_implemented,
    0o2000: instruction_not_implemented,
    0o3000: instruction_not_implemented,
    0o4000: memory_reference_jms,
    0o5000: memory_reference_jmp,
    0o6000: instruction_not_implemented,
    0o7000: operate_group,
}


if __name__ == "__main__":
    demo()
