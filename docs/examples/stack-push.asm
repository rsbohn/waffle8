/ stack-push.asm
/ Demonstrates basic PUSH operation

        *0200

START,  CLA CLL
        TAD STACK_BASE          / Initialize stack pointer
        DCA SP
        
        / Push three values onto the stack
        CLA
        TAD VALUE1              / Load first value
        JMS PUSH                / Push it
        
        TAD VALUE2              / Load second value  
        JMS PUSH                / Push it
        
        TAD VALUE3              / Load third value
        JMS PUSH                / Push it
        
        HLT                     / Done

/------------------------------------------------------------
/ PUSH - Push AC onto stack
/ Entry: AC contains value to push
/ Exit: AC cleared, SP incremented
/------------------------------------------------------------
PUSH,   0
        DCA I SP                / Store AC at address in SP
        ISZ SP                  / Increment stack pointer
        7000                    / NOP - ISZ doesn't skip here (SP != 0)
        JMP I PUSH              / Return

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,         0                   / Stack pointer
STACK_BASE, 0300                / Base of stack area

VALUE1,     0001                / Test value 1
VALUE2,     0002                / Test value 2
VALUE3,     0003                / Test value 3

        *0300                   / Stack storage area
STACK,  0                       / Stack starts here
