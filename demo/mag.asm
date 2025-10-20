/ mag.asm
/ Copy 30 words from address 0200 to magtape unit 1 (append-only)
/ Uses ZP:20 for word count N (negative count for ISZ loop)
/ Uses ZP:17 for start address (auto-increment on indirect access)

        *0200                   / Program origin

START,  CLA CLL                 / Clear AC and Link
        IAC                     / AC = 1
        IOT 6701                / Select mag tape unit 1

        / Initialize - copy 30 words starting at 0200
        TAD NEG_COUNT           / Load -30 (to copy 30 words)
        DCA 0020               / Store count at ZP:20
        TAD SRC_ADDR            / Load source address 0200
        DCA 0017             / Store at ZP:17 (auto-increment location)
        
        / Main copy loop
COPY_LOOP,
        ISZ 0020                / Increment count, skip when zero
        JMP WAIT_READY          /
        JMP COPY_DONE           / Done when count reaches zero
        
        / Wait for magtape ready
WAIT_READY,
        IOT 6710                / Skip if magtape ready
        JMP WAIT_READY          / Wait until ready
        
        / Read word from memory and write to tape
        TAD I 0017           / Load word from memory (auto-increments SRC_PTR)
        IOT 6704                / Write AC to magtape unit 1
        
        JMP COPY_LOOP           / Continue loop

COPY_DONE,
        / Signal completion
        CLA CLL
        TAD DONE_MSG            / Load completion indicator
        IOT 6704                / Write completion marker to tape
        IOT 6720                / Rewind to close and flush file
        HLT                     / Halt program

/ Constants and variables
NEG_COUNT,  -30                 / Negative count (copy 30 words)  
SRC_ADDR,   0200                / Source address to start copying from
DONE_MSG,   7777                / Completion marker (all ones)

/ Zero page locations
COUNT,      0020                / Word count at ZP:20
SRC_PTR,    0017                / Source pointer at ZP:17 (auto-increment)

/ Test data at 0400
        *0400
TEST_DATA,
        0001                    / Test word 1
        0002                    / Test word 2
        0003                    / Test word 3  
        0004                    / Test word 4
        0005                    / Test word 5