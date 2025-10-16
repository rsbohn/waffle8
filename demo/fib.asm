/
/ fib.asm
/ Print the first five Fibonacci numbers (1, 1, 2, 3, 5) on the KL8E console.
/

        *0200                   / Program origin

START,  CLA CLL                 / Ensure AC and link are cleared
        TAD COUNT_INIT          / Load loop counter (-5)
        DCA COUNT               / COUNT = -5 so ISZ trips after five prints

        CLA                     / Initialize previous term to 0
        DCA PREV
        TAD ONE                 / Current term starts at 1
        DCA CUR

LOOP,   TAD CUR                 / AC = current Fibonacci number
        JMS PRINT_DIGIT         / Print as single decimal digit
        JMS NEWLINE             / Terminate the line

        TAD PREV                / AC = previous term
        TAD CUR                 / AC = prev + cur (next Fibonacci number)
        DCA NEXT                / NEXT = next term, AC cleared

        TAD CUR                 / AC = current term
        DCA PREV                / PREV = current, AC cleared

        TAD NEXT                / AC = next term
        DCA CUR                 / CUR = next, AC cleared

        ISZ COUNT               / Increment loop counter; skip when done
        JMP LOOP

        HLT                     / Finished

/------------------------------------------------------------
/ PRINT_DIGIT - Print value in AC as a single decimal digit.
/ Entry: AC = value 0-9
/------------------------------------------------------------
PRINT_DIGIT, 0                  / Return address slot
        DCA TMP                 / Save value, clear AC
        TAD TMP                 / Restore value
        TAD ASCII_ZERO          / Convert to ASCII
        JMS PUTCH               / Output character
        CLA                     / Leave AC defaulted to 0
        JMP I PRINT_DIGIT       / Return

/------------------------------------------------------------
/ NEWLINE - Emit CR/LF pair on the console.
/------------------------------------------------------------
NEWLINE, 0
        TAD CR
        JMS PUTCH
        TAD LF
        JMS PUTCH
        CLA
        JMP I NEWLINE

/------------------------------------------------------------
/ PUTCH - Block until KL8E ready, then output character in AC.
/------------------------------------------------------------
PUTCH,  0
        DCA CHBUF               / Save character, AC cleared
WAIT,   IOT 6041                / Skip when teleprinter ready
        JMP WAIT
        TAD CHBUF               / Reload character
        IOT 6046                / Transmit to teleprinter
        CLA
        JMP I PUTCH

/------------------------------------------------------------
/ Storage and constants
/------------------------------------------------------------
PREV,       0000                / Previous Fibonacci term
CUR,        0000                / Current Fibonacci term
NEXT,       0000                / Next term workspace
COUNT,      0000                / Loop counter
TMP,        0000                / Scratch for PRINT_DIGIT
CHBUF,      0000                / Buffer for PUTCH

ONE,        0001
COUNT_INIT, 07773               / -5 => run loop five times
ASCII_ZERO, 0060                / ASCII '0'
CR,         0015
LF,         0012

        $                       / End of program
