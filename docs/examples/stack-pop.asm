/ stack-pop.asm
/ Demonstrates PUSH and POP operations

        *0200

START,  CLA CLL
        TAD STACK_BASE          / Initialize stack pointer
        DCA SP
        
        / Push three values
        TAD VALUE1
        JMS PUSH
        TAD VALUE2
        JMS PUSH
        TAD VALUE3
        JMS PUSH
        
        / Pop three values and store them
        JMS POP
        DCA RESULT3
        JMS POP
        DCA RESULT2
        JMS POP
        DCA RESULT1
        
        HLT

/------------------------------------------------------------
/ PUSH - Push AC onto stack
/------------------------------------------------------------
PUSH,   0
        DCA I SP
        ISZ SP
        7000                    / NOP
        JMP I PUSH

/------------------------------------------------------------
/ POP - Pop value from stack into AC
/ Entry: Stack must not be empty
/ Exit: AC contains popped value, SP decremented
/------------------------------------------------------------
POP,    0
        CLA                     / Clear AC
        TAD SP                  / Load current SP
        TAD NEG1                / Subtract 1
        DCA SP                  / Store decremented SP
        TAD I SP                / Load value from stack
        JMP I POP               / Return with value in AC

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,         0
STACK_BASE, 0300
NEG1,       7777                / -1 in two's complement

VALUE1,     0111                / 'I' in octal ASCII
VALUE2,     0110                / 'H' in octal ASCII  
VALUE3,     0041                / '!' in octal ASCII

RESULT1,    0
RESULT2,    0
RESULT3,    0

        *0300
STACK,  0
