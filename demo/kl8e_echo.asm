/ kl8e_echo.asm
/ Simple echo demo using core.asm for KL8E console I/O
/ This program demonstrates using the core system for keyboard input and teleprinter output.
/ It echoes characters typed on the keyboard to the teleprinter until ESC is pressed.

/ Zero-page interface slots used by core.asm
        *0005
CORE_OPCODE, 0                   / Core opcode slot (BIOS selector)
        *0006
CORE_VPTR, 0                     / Core parameter slot (VPTR)

        *0200                   / Program origin

START,  CLA CLL                 / Clear accumulator and link
        TAD MSG_PTR             / Load pointer to welcome message
        DCA CHAR_PTR            / Set up character pointer
        JMS PRINT_STRING        / Print welcome message

ECHO_LOOP,
        TAD GETCH_OP            / Load keyboard input opcode (1)
        DCA CORE_OPCODE         / Store opcode in core system
        CLA                     / Clear vptr
        DCA CORE_VPTR
        JMS 0002                / Call core system to get character
        TAD CORE_VPTR           / Get the character that was read
        DCA CHAR                / Store it
        
        TAD CHAR                / Check if ESC (033 octal)
        TAD NEG_ESC
        SZA                     / Skip if ESC
        JMP ECHO_CHAR
        JMP EXIT_PROG           / ESC pressed, exit

ECHO_CHAR,
        TAD PUTCH_OP            / Load teleprinter output opcode (2)  
        DCA CORE_OPCODE         / Store opcode in core system
        TAD CHAR                / Load character to output
        DCA CORE_VPTR           / Store in vptr
        JMS 0002                / Call core system to output character
        
        JMP ECHO_LOOP           / Continue echoing

EXIT_PROG,
        CLA CLL
        TAD GOODBYE_PTR         / Load pointer to goodbye message
        DCA CHAR_PTR
        JMS PRINT_STRING        / Print goodbye message
        HLT                     / Halt program

/------------------------------------------------------------
/ PRINT_STRING - Print null-terminated string using core system
/ Entry: CHAR_PTR points to string
/------------------------------------------------------------
PRINT_STRING, 0                 / Return address
        CLA                     / Clear AC before writing opcode
        TAD PUTCH_OP            / Preload teleprinter opcode
        DCA CORE_OPCODE

PRINT_LOOP,
        CLA                     / Ensure AC is clear before loading the next character
        TAD I CHAR_PTR          / Get next character
        SZA                     / Skip if null terminator
        JMP PRINT_NEXT
        JMP I PRINT_STRING      / Return if done

PRINT_NEXT,
        DCA CORE_VPTR           / Store character for core system
        JMS 0002                / Output character via core system
        
        ISZ CHAR_PTR            / Advance to next character
        JMP PRINT_LOOP          / Continue

/------------------------------------------------------------
/ Data and constants
/------------------------------------------------------------

GETCH_OP, 1                     / Opcode 1: KL8E keyboard input
PUTCH_OP, 2                     / Opcode 2: KL8E teleprinter output

NEG_ESC, 7745                   / -033 (negative ESC character)

CHAR, 0                         / Current character
CHAR_PTR, 0                     / String pointer

MSG_PTR, WELCOME_MSG
GOODBYE_PTR, GOODBYE_MSG

/ Welcome message
WELCOME_MSG,
        0115                    / 'M'
        0145                    / 'e'
        0163                    / 's'
        0163                    / 's' 
        0141                    / 'a'
        0147                    / 'g'
        0145                    / 'e'
        0040                    / ' '
        0145                    / 'e'
        0143                    / 'c'
        0150                    / 'h'
        0157                    / 'o'
        0040                    / ' '
        0144                    / 'd'
        0145                    / 'e'
        0155                    / 'm'
        0157                    / 'o'
        0041                    / '!'
        0040                    / ' '
        0120                    / 'P'
        0162                    / 'r'
        0145                    / 'e'
        0163                    / 's'
        0163                    / 's'
        0040                    / ' '
        0105                    / 'E'
        0123                    / 'S'
        0103                    / 'C'
        0040                    / ' '
        0164                    / 't'
        0157                    / 'o'
        0040                    / ' '
        0145                    / 'e'
        0170                    / 'x'
        0151                    / 'i'
        0164                    / 't'
        0056                    / '.'
        0015                    / CR
        0012                    / LF  
        0000                    / null terminator

/ Goodbye message
GOODBYE_MSG,
        0107                    / 'G'
        0157                    / 'o'
        0157                    / 'o'
        0144                    / 'd'
        0142                    / 'b'
        0171                    / 'y'
        0145                    / 'e'
        0041                    / '!'
        0015                    / CR
        0012                    / LF
        0000                    / null terminator
