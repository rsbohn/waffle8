/ print-tape.asm
/ Read a block from paper tape and print it to the console
/ 
/ Paper Tape Device (667x):
/   6671 - Skip if ready
/   6672 - Select block (AC = block number)
/   6674 - Read word into AC
/
/ KL8E Console (603x/604x):
/   6031 - Skip if keyboard ready
/   6032 - Clear AC, read keyboard
/   6041 - Skip if teleprinter ready  
/   6046 - Load AC, print character

        *200                    / Start at location 200 (octal)

START,  CLA CLL                 / Clear AC and Link
        TAD BLKNUM              / Load block number to read
        IOT 6672                / Select block on paper tape
        
        CLA                     / Ensure AC cleared before loading pointer
        TAD MSGBLK_PTR          / Print block header message
        JMS PRINT
        
        CLA
        TAD BLKNUM              / Print block number
        JMS PRTNUM
        
        CLA
        TAD MSGNL_PTR           / Print newline
        JMS PRINT

RDLOOP, IOT 6671                / Skip if tape ready
        JMP RDLOOP              / Wait for data
        
        IOT 6674                / Read word from tape
        DCA WORD                / Save the word
        
        / Print the word in octal (4 digits)
        TAD WORD                / Get word back
        JMS PRTNUM              / Print as octal number
        
        CLA
        TAD SPACE               / Print space separator
        JMS PUTCH
        
        ISZ COUNT               / Increment word counter
        TAD COUNT               / Check counter
        AND K7                  / Mask to check mod 8
        SNA                     / Skip if not zero
        JMS NEWLN               / Print newline every 8 words
        
        JMP RDLOOP              / Continue reading
        
/ Reached here when block exhausted (6671 no longer skips)
DONE,   CLA
        TAD MSGDONE_PTR         / Print completion message
        JMS PRINT
        
        CLA
        TAD COUNT               / Print total word count
        JMS PRTNUM
        
        CLA
        TAD MSGNL_PTR
        JMS PRINT
        
        HLT                     / Stop

/------------------------------------------------------------
/ PRINT - Print null-terminated string
/ Entry: AC = pointer to string
/ Uses: AC, PTR
/------------------------------------------------------------
PRINT,  0                       / Return address
        DCA PTR                 / Save string pointer
PRT1,   TAD I PTR               / Get character
        SNA                     / Skip if not null
        JMP I PRINT             / Return if null terminator
        JMS PUTCH               / Print character
        ISZ PTR                 / Advance pointer
        JMP PRT1                / Continue

/------------------------------------------------------------
/ PUTCH - Print single character to teleprinter
/ Entry: AC = character
/ Uses: AC
/------------------------------------------------------------
PUTCH,  0                       / Return address
        DCA CHBUF               / Save character
PUTCH1, IOT 6041                / Skip if teleprinter ready
        JMP PUTCH1              / Wait
        TAD CHBUF               / Get character
        IOT 6046                / Print it
        CLA
        JMP I PUTCH             / Return

/------------------------------------------------------------
/ NEWLN - Print newline
/ Uses: AC
/------------------------------------------------------------
NEWLN,  0                       / Return address
        TAD CR                  / Carriage return
        JMS PUTCH
        TAD LF                  / Line feed
        JMS PUTCH
        JMP I NEWLN

/------------------------------------------------------------
/ PRTNUM - Print 12-bit number in octal (4 digits)
/ Entry: AC = number to print
/ Uses: AC, TEMP, digit positions
/------------------------------------------------------------
PRTNUM, 0                       / Return address
        DCA TEMP                / Save number
        
        / Print digit 3 (bits 9-11)
        TAD TEMP
        AND K7000               / Mask high 3 bits
        RAR; RAR; RAR           / Rotate right 9 positions
        RAR; RAR; RAR
        RAR; RAR; RAR
        TAD ZERO                / Convert to ASCII
        JMS PUTCH
        
        / Print digit 2 (bits 6-8)
        TAD TEMP
        AND K0700               / Mask bits 6-8
        RAR; RAR; RAR           / Rotate right 6 positions
        RAR; RAR; RAR
        TAD ZERO
        JMS PUTCH
        
        / Print digit 1 (bits 3-5)
        TAD TEMP
        AND K0070               / Mask bits 3-5
        RAR; RAR; RAR           / Rotate right 3 positions
        TAD ZERO
        JMS PUTCH
        
        / Print digit 0 (bits 0-2)
        TAD TEMP
        AND K7                  / Mask low 3 bits
        TAD ZERO
        JMS PUTCH
        
        JMP I PRTNUM

/------------------------------------------------------------
/ Constants and Variables
/------------------------------------------------------------
BLKNUM, 0001                    / Block number to read (change as needed)
COUNT,  0000                    / Word counter
WORD,   0000                    / Current word buffer
PTR,    0000                    / String pointer
CHBUF,  0000                    / Character buffer
TEMP,   0000                    / Temporary storage

/ ASCII character constants
ZERO,   0060                    / ASCII '0'
SPACE,  0040                    / ASCII space
CR,     0015                    / Carriage return
LF,     0012                    / Line feed

/ Octal masks
K7,     0007                    / Mask for low 3 bits
K0070,  0070                    / Mask for bits 3-5
K0700,  0700                    / Mask for bits 6-8
K7000,  7000                    / Mask for bits 9-11

/ Message pointers
MSGBLK_PTR, MSGBLK
MSGNL_PTR, MSGNL
MSGDONE_PTR, MSGDONE

/ Messages (null-terminated strings)
MSGBLK, "R";"e";"a";"d";" ";"b";"l";"o";"c";"k";" ";0000
MSGNL,  0015;0012;0000          / CR+LF
MSGDONE,"D";"o";"n";"e";".";" ";0000

        $                       / End of program
