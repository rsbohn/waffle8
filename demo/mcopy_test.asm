/ Test program to copy 0344 words from 0200 to 4000 using mcopy routine
/ This demonstrates using the mcopy subroutine

        *0100                   / Start at address 0100

START,  CLA CLL
        
        / Set up COUNT = -0344 (negative of words to copy)
        TAD NEG_COUNT           / Load -0344
        DCA COUNT               / Store in COUNT location (0012)
        
        / Set up SRC = 0200 (source address)
        TAD SRC_ADDR            / Load 0200  
        DCA SRC                 / Store in SRC location (0010)
        
        / Set up DEST = 4000 (destination address)
        TAD DEST_ADDR           / Load 4000
        DCA DEST                / Store in DEST location (0011)
        
        / Call the memory copy routine
        JMS I MCOPY_PTR         / Call mcopy indirectly
        
        / Copy completed
        HLT                     / Halt when done

/ Constants
NEG_COUNT,  -0344               / Negative count (copy 228 decimal words)
SRC_ADDR,   0200                / Source address (octal 200)
DEST_ADDR,  4000                / Destination address (octal 4000)
MCOPY_PTR,  0300                / Pointer to mcopy routine

/ Zero page variable addresses (same as in mcopy.asm)
COUNT,      0012                / word count location
SRC,        0010                / source address pointer location  
DEST,       0011                / destination address pointer location

/ Memory copy routine (include the mcopy code)
        *0300
MCOPY,  0                       / return addr goes here
        CLA CLL
MCLOOP, ISZ COUNT               / increment count, skip when zero
        JMP I MCOPY             / exit directly when count reaches zero
        TAD I SRC               / load word from source (auto-increments SRC)
        DCA I DEST              / store word to destination (auto-increments DEST)
        JMP MCLOOP              / continue loop