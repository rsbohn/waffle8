/ Print memory locations 0000-0007 in octal
/ Outputs to paper tape punch (PTP) device

        *0200           / Start at location 0200

START,  CLA CLL         / Clear AC and Link
        DCA TEMP        / Start at address 0
        
        / Print address header "0000:"
        TAD ZERO        / Print '0'
        JMS PUTC
        JMS PUTC
        JMS PUTC
        JMS PUTC
        TAD COLON       / Print ':'
        JMS PUTC
        
        TAD M0010       / Load -8 (counter)
        DCA COUNT       / Store counter
LOOP,   TAD I TEMP      / Load value from memory at address
        JMS PRTOC       / Print in octal
        ISZ TEMP        / Increment address
        ISZ COUNT       / Increment counter, skip if zero
        JMP LOOP        / Continue
        
        TAD NLCHAR      / Print final newline
        JMS PUTC
        HLT             / Done - halt

/ Print octal number in AC (4 digits)
PRTOC,  0               / Return address
        DCA VAL         / Save value
        TAD SPCHAR      / Print space first
        JMS PUTC
        
        / Print digit 1 (bits 11-9)
        CLA
        TAD VAL
        AND M7000       / Get bits 11-9
        RAR             / Rotate right
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        TAD ZERO        / Make ASCII
        JMS PUTC
        
        / Print digit 2 (bits 8-6)
        CLA
        TAD VAL
        AND M0700       / Get bits 8-6
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        TAD ZERO
        JMS PUTC
        
        / Print digit 3 (bits 5-3)
        CLA
        TAD VAL
        AND M0070       / Get bits 5-3
        RAR
        RAR
        RAR
        TAD ZERO
        JMS PUTC
        
        / Print digit 4 (bits 2-0)
        CLA
        TAD VAL
        AND M0007       / Get bits 2-0
        TAD ZERO
        JMS PUTC
        JMP I PRTOC

/ Output character in AC via paper tape punch
PUTC,   0               / Return address
        DCA CHAR        / Save character
PUTC1,  IOT 6021        / PSF: Skip if flag set (ready)
        JMP PUTC1       / Wait
        TAD CHAR        / Get character
        IOT 6026        / PPC: Clear flag and punch character
        CLA
        JMP I PUTC

TEMP,   0               / Temporary storage
COUNT,  0               / Loop counter
VAL,    0               / Value to print
CHAR,   0               / Character buffer
M0010,  7770            / -8 in octal (two's complement)
M7000,  7000
M0700,  0700
M0070,  0070
M0007,  0007
ZERO,   0060            / ASCII '0'
COLON,  0072            / ASCII ':'
SPCHAR, 0040            / ASCII space
NLCHAR, 0212            / ASCII newline (LF)

