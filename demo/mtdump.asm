/ mag-dump.asm - Dump magtape contents in octal
/ Reads from tape unit 0 and prints 8 words per line in octal

        *0200

START,  CLA CLL                 / Select unit 0
        IOT 6701                / Select unit
        IOT 6720                / Rewind to start
        
        CLA CLL
        TAD WCOUNT
        DCA COUNTER             / Initialize words-per-line counter

READ_LOOP,
        IOT 6710                / Skip if tape ready
        JMP READ_DONE           / End of tape
        
        IOT 6702                / Read word into AC
        JMS PRINT_OCTAL         / Print it in octal
        
        CLA CLL
        TAD SPACE
        JMS PRINT_CHAR          / Print space separator
        
        / Decrement counter
        ISZ COUNTER
        JMP READ_LOOP           / Not zero, continue same line
        
        / Counter wrapped to zero - print newline
        CLA CLL
        TAD NEWLINE
        JMS PRINT_CHAR
        
        / Reset counter
        CLA CLL
        TAD WCOUNT
        DCA COUNTER
        JMP READ_LOOP

READ_DONE,
        CLA CLL
        TAD NEWLINE
        JMS PRINT_CHAR          / Final newline
        HLT

/ Print word in AC as 4 octal digits
PRINT_OCTAL, 0
        DCA TEMP                / Save word
        
        / Print digit 3 (bits 9-11)
        CLA CLL
        TAD TEMP
        CLL RAR                 / Rotate right 3 times
        RAR
        RAR
        CLL RAR                 / 3 more = 6 total
        RAR
        RAR
        CLL RAR                 / 3 more = 9 total
        RAR
        RAR
        AND MASK3               / Keep only lower 3 bits
        TAD ZERO
        JMS PRINT_CHAR
        
        / Print digit 2 (bits 6-8)
        CLA CLL
        TAD TEMP
        CLL RAR                 / Rotate right 6 times
        RAR
        RAR
        CLL RAR
        RAR
        RAR
        AND MASK3
        TAD ZERO
        JMS PRINT_CHAR
        
        / Print digit 1 (bits 3-5)
        CLA CLL
        TAD TEMP
        CLL RAR                 / Rotate right 3 times
        RAR
        RAR
        AND MASK3
        TAD ZERO
        JMS PRINT_CHAR
        
        / Print digit 0 (bits 0-2)
        CLA CLL
        TAD TEMP
        AND MASK3
        TAD ZERO
        JMS PRINT_CHAR
        
        JMP I PRINT_OCTAL

/ Print character in AC
PRINT_CHAR, 0
        DCA PTEMP               / Save character
PRINT_WAIT,
        IOT 6601                / Skip when printer ready
        JMP PRINT_WAIT          / Wait if not ready
        CLA CLL
        TAD PTEMP
        IOT 6606                / Print character
        JMP I PRINT_CHAR

/ Data
TEMP,    0
PTEMP,   0
COUNTER, 0
WCOUNT,  7770                   / -8 in octal (will wrap to 0 after 8 words)
MASK3,   0007                   / Mask for 3 bits
ZERO,    0060                   / ASCII '0'
SPACE,   0040                   / ASCII space
NEWLINE, 0012                   / ASCII newline (LF)

$