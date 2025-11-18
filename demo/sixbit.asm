/ sixbit.asm - Simple SIXBIT encoding test program
/ Reads keyboard input and displays SIXBIT encoding for each character
/ 
/ Usage:
/   - Type letters A-Z to see SIXBIT encoding
/   - Non-letters display "INVALID"
/   - Press EOT (Ctrl+D) to exit
/
/ Output format:
/   A 0001
/   B 0002
/   I 0011
/   - INVALID
/   . INVALID

        *0
CHAR,           0
SIXBIT_VAL,     0
OCT_DIGIT,      0
OCT_WORK,       0

        *0600
START,  CLA CLL

MAIN_LOOP,
        / Read character from keyboard
        JMS READ_CHAR
        
        / Echo the character
        CLA
        TAD CHAR
        JMS PUTCHR
        
        / Print space
        CLA
        TAD CHAR_SPACE
        JMS PUTCHR
        
        / Convert to SIXBIT and print
        JMS CONVERT_SIXBIT
        JMS PRINT_SIXBIT
        
        / Print CRLF
        JMS PRINT_CRLF
        
        / Check for EOT
        CLA
        TAD CHAR
        TAD NEG_EOT
        SZA
        JMP MAIN_LOOP
        
        / Exit
        HLT

/------------------------------------------------------------------------------
/ Read character from keyboard
/------------------------------------------------------------------------------
READ_CHAR,      0
KC_WAIT,
        IOT 6031                / Wait for keyboard character
        JMP KC_WAIT
        IOT 6036                / Read character into AC
        DCA CHAR
        JMP I READ_CHAR

/------------------------------------------------------------------------------
/ Convert character to SIXBIT (A=1, B=2, ... Z=26, others=0)
/------------------------------------------------------------------------------
CONVERT_SIXBIT, 0
        CLA
        TAD CHAR
        / Check if uppercase letter A-Z
        TAD NEG_A
        SMA                     / < 'A'?
        JMP CHECK_UPPER_BOUND
        / Not a letter
        CLA
        DCA SIXBIT_VAL          / Store 0 for invalid
        JMP I CONVERT_SIXBIT

CHECK_UPPER_BOUND,
        CLA
        TAD CHAR
        TAD NEG_Z
        SPA                     / > 'Z'?
        JMP VALID_LETTER
        / Not a letter
        CLA
        DCA SIXBIT_VAL          / Store 0 for invalid
        JMP I CONVERT_SIXBIT

VALID_LETTER,
        / Convert A-Z to 1-26
        CLA
        TAD CHAR
        TAD NEG_A               / CHAR - 'A'
        TAD ONE                 / Add 1 (A=1, B=2, etc.)
        DCA SIXBIT_VAL
        JMP I CONVERT_SIXBIT

/------------------------------------------------------------------------------
/ Print SIXBIT value as 4-digit octal or "INVALID"
/------------------------------------------------------------------------------
PRINT_SIXBIT,   0
        CLA
        TAD SIXBIT_VAL
        SZA                     / Zero means invalid
        JMP PRINT_OCTAL
        
        / Print "INVALID"
        CLA
        TAD INVALID_MSG
        JMS PRINT_STRING
        JMP I PRINT_SIXBIT

PRINT_OCTAL,
        / Print 4-digit octal number
        CLA
        TAD DIGIT_0
        JMS PUTCHR              / Leading zero
        
        CLA
        TAD DIGIT_0
        JMS PUTCHR              / Second zero
        
        / Print tens digit
        CLA
        TAD SIXBIT_VAL
        AND OCT_MASK_TENS       / Mask bits 3-5 (tens)
        RAR; RAR; RAR           / Shift right 3 positions
        TAD DIGIT_0
        JMS PUTCHR
        
        / Print units digit
        CLA
        TAD SIXBIT_VAL
        AND OCT_MASK_UNITS      / Mask bits 0-2 (units)
        TAD DIGIT_0
        JMS PUTCHR
        
        JMP I PRINT_SIXBIT

/------------------------------------------------------------------------------
/ Print null-terminated string
/------------------------------------------------------------------------------
PRINT_STRING,   0
        DCA STRING_PTR
PRINT_LOOP,
        CLA
        TAD I STRING_PTR
        SZA                     / Null terminator?
        JMP PRINT_CHAR
        JMP I PRINT_STRING      / Done
PRINT_CHAR,
        JMS PUTCHR
        ISZ STRING_PTR
        JMP PRINT_LOOP

/------------------------------------------------------------------------------
/ Print CRLF
/------------------------------------------------------------------------------
PRINT_CRLF,     0
        CLA
        TAD CHAR_CR
        JMS PUTCHR
        CLA
        TAD CHAR_LF
        JMS PUTCHR
        JMP I PRINT_CRLF

/------------------------------------------------------------------------------
/ Print character to teleprinter
/------------------------------------------------------------------------------
PUTCHR,         0
        DCA CHAR_TEMP
TTY_WAIT,
        IOT 6041                / Wait for teleprinter ready
        JMP TTY_WAIT
        CLA
        TAD CHAR_TEMP
        IOT 6046                / Transmit
        JMP I PUTCHR

/------------------------------------------------------------------------------
/ Data and constants
/------------------------------------------------------------------------------
STRING_PTR,     0
CHAR_TEMP,      0

CHAR_SPACE,     040
CHAR_CR,        015
CHAR_LF,        012
CHAR_EOT,       004
DIGIT_0,        060

NEG_A,          07677           / -'A' (65 decimal)
NEG_Z,          07646           / -'Z' (90 decimal)
NEG_EOT,        07774           / -EOT (4 decimal)

ONE,            0001
OCT_MASK_TENS,  0070            / Bits 3-5
OCT_MASK_UNITS, 0007            / Bits 0-2

INVALID_MSG,    INVALID_TEXT

INVALID_TEXT,
        "I"
        "N"
        "V"
        "A"
        "L"
        "I"
        "D"
        0000

$