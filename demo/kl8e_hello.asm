/ kl8e_hello.asm  
/ Simple "Hello World" demo using core.asm for KL8E teleprinter output
/ Demonstrates using the core system for console output

/ Zero-page interface slots used by core.asm
        *0004
CORE_OPCODE, 0                   / Core opcode slot
CORE_VPTR, 0                     / Core parameter slot

        *0200                   / Program origin

START,  CLA CLL                 / Clear accumulator and link
        TAD HELLO_PTR           / Load pointer to message
        DCA CHAR_PTR            / Set up character pointer
        JMS PRINT_STRING        / Print the message
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
        CLA                     / Ensure AC clear before loading next character
        TAD I CHAR_PTR          / Get next character
        SZA                     / Skip if null terminator
        JMP PRINT_CHAR
        JMP I PRINT_STRING      / Return if done

PRINT_CHAR,
        DCA CORE_VPTR           / Store character for core system
        JMS 0001                / Call core system to output character
        
        ISZ CHAR_PTR            / Advance to next character
        JMP PRINT_LOOP          / Continue printing

/------------------------------------------------------------
/ Data and constants
/------------------------------------------------------------
CORE_ENTRY, 7000                / Entry point for core system
PUTCH_OP, 2                     / Opcode 2: KL8E teleprinter output

CHAR_PTR, 0                     / String pointer
HELLO_PTR, HELLO_MSG

/ "Hello, KL8E!" message
HELLO_MSG,
        0110                    / 'H'
        0145                    / 'e'
        0154                    / 'l' 
        0154                    / 'l'
        0157                    / 'o'
        0054                    / ','
        0040                    / ' '
        0113                    / 'K'
        0114                    / 'L'
        0070                    / '8'
        0105                    / 'E'
        0041                    / '!'
        0015                    / CR
        0012                    / LF
        0000                    / null terminator
