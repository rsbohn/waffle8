/ stack-reverse.asm
/ Reverse a string using stack operations

        *0200

START,  CLA CLL
        TAD STACK_BASE
        DCA SP
        
        / Push each character of the string
        CLA
        TAD STRING_PTR
        TAD NEG1                / Adjust for auto-increment
        DCA PTR
        
PUSH_LOOP,
        TAD I PTR               / Get character (auto-increment)
        SZA                     / Check for null terminator
        JMP DO_PUSH
        JMP POP_START           / Done pushing, start popping
        
DO_PUSH,
        JMS PUSH                / Push character
        ISZ PTR                 / Already incremented by I PTR
        7000                    / NOP
        JMP PUSH_LOOP

POP_START,
        / Pop characters and store in output buffer
        CLA
        TAD OUTPUT_PTR
        TAD NEG1
        DCA PTR
        
POP_LOOP,
        / Check if stack is empty
        CLA
        TAD SP
        TAD NEG_STACK_BASE
        SZA
        JMP DO_POP
        JMP DONE                / Stack empty, we're done
        
DO_POP,
        JMS POP                 / Pop character
        DCA I PTR               / Store in output (auto-increment)
        ISZ PTR
        7000                    / NOP
        JMP POP_LOOP

DONE,   / Add null terminator to output
        CLA
        DCA I PTR
        
        HLT

/------------------------------------------------------------
/ PUSH and POP
/------------------------------------------------------------
PUSH,   0
        DCA I SP
        ISZ SP
        7000                    / NOP
        JMP I PUSH

POP,    0
        CLA
        TAD SP
        TAD NEG1
        DCA SP
        TAD I SP
        JMP I POP

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,              0
STACK_BASE,      0300
NEG_STACK_BASE,  7500
NEG1,            7777
PTR,             0

STRING_PTR,      STRING
OUTPUT_PTR,      OUTPUT

        *0240
STRING,
        0110                    / H
        0145                    / e
        0154                    / l
        0154                    / l
        0157                    / o
        0000                    / null terminator

        *0250
OUTPUT, 0                       / Output buffer (6 words + null)
        0
        0
        0
        0
        0
        0

        *0300
STACK,  0
