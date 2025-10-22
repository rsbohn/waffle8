from pdp8 import EMULATOR as emu


def trace(emulator, steps: int, reset_halt: bool = False) -> None:
    """Execute a bounded number of instructions and print key registers."""
    if reset_halt:
        emulator.halted = False
    for _ in range(steps):
        if emulator.halted:
            break
        emulator.step()
    print(
        f"AC={emulator.ac} PC={emulator.pc} LINK={emulator.link} HALTED={emulator.halted}"
    )
    m0 = [oct(word) for word in emulator.memory[:10]]
    ret_direct = oct(emulator.memory[0o20])
    ret_indirect = oct(emulator.memory[0o30])
    print("M0=", m0)
    print(f"RET20={ret_direct} RET30={ret_indirect}")


emu.reset()

# Main program:
#   0: JMS 20 (direct jump to subroutine A)
#   1: JMP 4  (direct jump skips over the pointer data at address 2)
#   2: Pointer to subroutine B (used by JMS I 2)
#   4: JMS I 2 (indirect jump to subroutine B)
#   5: HLT     (terminate after both subroutines run)
emu.load(
    0o0000,
    [
        0o4020,  # JMS 20
        0o5004,  # JMP 4
        0o0030,  # pointer to subroutine B
        0o0000,  # padding
        0o4402,  # JMS I 2
        0o7402,  # HLT
    ],
)

# Subroutine A at 0o20: increment AC once then return via indirect jump.
emu.load(
    0o0020,
    [
        0o0000,  # return slot overwritten by JMS
        0o7001,  # IAC
        0o5420,  # JMP I 20
    ],
)

# Subroutine B at 0o30: increment AC twice, then return indirectly.
emu.load(
    0o0030,
    [
        0o0000,  # return slot overwritten by JMS I
        0o7001,  # IAC
        0o7001,  # IAC
        0o5430,  # JMP I 30
    ],
)

for _ in range(9):
    trace(emu, 1)
