/ stack-safe.asm
/ Stack with overflow and underflow detection

        *0200

START,  CLA CLL
        TAD STACK_BASE
        DCA SP
        
        / Try to push values
        TAD VAL1
        JMS PUSH_SAFE
        TAD VAL2
        JMS PUSH_SAFE
        
        / Try to pop values (should succeed)
        JMS POP_SAFE
        DCA TEMP
        JMS POP_SAFE
        DCA TEMP
        
        / Try to pop again (should fail - underflow)
        JMS POP_SAFE
        DCA TEMP
        
        HLT                     / Should not reach here if underflow handled

/------------------------------------------------------------
/ PUSH_SAFE - Push with overflow checking
/ Entry: AC contains value to push
/ Exit: AC cleared, or halts on overflow
/------------------------------------------------------------
PUSH_SAFE, 0
        DCA TEMP                / Save value temporarily
        
        / Check if SP < STACK_LIMIT
        CLA
        TAD STACK_LIMIT
        CMA                     / Complement
        IAC                     / Increment (two's complement negate)
        TAD SP                  / SP - STACK_LIMIT
        SMA SZA                 / Skip if negative or zero
        JMP OVERFLOW            / Stack full!
        
        TAD TEMP                / Restore value
        DCA I SP                / Store on stack
        ISZ SP
        7000                    / NOP
        JMP I PUSH_SAFE

OVERFLOW,
        HLT                     / Halt on overflow
        JMP OVERFLOW            / Loop if continued

/------------------------------------------------------------
/ POP_SAFE - Pop with underflow checking
/ Entry: Stack should not be empty
/ Exit: AC contains value, or halts on underflow  
/------------------------------------------------------------
POP_SAFE, 0
        / Check if SP > STACK_BASE
        CLA
        TAD SP
        TAD NEG_STACK_BASE      / SP - STACK_BASE
        SZA                     / Skip if zero (empty)
        JMP POP_OK
        JMP UNDERFLOW           / Stack empty!
        
POP_OK, CLA
        TAD SP
        TAD NEG1
        DCA SP
        TAD I SP
        JMP I POP_SAFE

UNDERFLOW,
        HLT                     / Halt on underflow
        JMP UNDERFLOW

/------------------------------------------------------------
/ Data  
/------------------------------------------------------------
SP,              0
STACK_BASE,      0300
STACK_LIMIT,     0400          / 256 words available
NEG_STACK_BASE,  7500          / -0300 in octal
NEG1,            7777          / -1

VAL1,            0123
VAL2,            0456
TEMP,            0

        *0300
STACK,  0
