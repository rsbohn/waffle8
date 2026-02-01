/ stack-params.asm
/ Passing parameters via stack

        *0200

START,  CLA CLL
        TAD STACK_BASE
        DCA SP
        
        / Calculate (5 + 3) using ADD_NUMS subroutine
        TAD NUM1                / First parameter
        JMS PUSH
        TAD NUM2                / Second parameter
        JMS PUSH
        
        JMS ADD_NUMS            / Call subroutine
        
        JMS POP                 / Get result
        DCA RESULT
        
        HLT

/------------------------------------------------------------
/ ADD_NUMS - Add two numbers from stack, push result
/ Stack on entry: [bottom] ... param1 param2 [top]
/ Stack on exit:  [bottom] ... result [top]
/------------------------------------------------------------
ADD_NUMS, 0
        / Pop second parameter
        JMS POP
        DCA PARAM2
        
        / Pop first parameter  
        JMS POP
        DCA PARAM1
        
        / Add them
        CLA
        TAD PARAM1
        TAD PARAM2
        
        / Push result
        JMS PUSH
        
        JMP I ADD_NUMS

/------------------------------------------------------------
/ PUSH and POP subroutines
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
SP,         0
STACK_BASE, 0300
NEG1,       7777

NUM1,       0005
NUM2,       0003
RESULT,     0

PARAM1,     0
PARAM2,     0

        *0300
STACK,  0
