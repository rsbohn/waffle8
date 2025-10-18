/ Test the core routines

    * 200
    CLA CLL
    TAD DOT_CHAR
    DCA 0005
    TAD ZERO
    DCA 0004

    JMS 0001
    JMS 0001
    CLA
    TAD 0007
    TAD NEG2
    SZA
    JMP TEST_FAIL         / if test failed (0007 != 2)
    JMP TEST_PASS

TEST_FAIL,
    CLA
    TAD HYPHEN_CHAR
    DCA 0005    / vptr character value
    TAD ZERO    
    DCA 0004    / Operation PRN_PUTCH
    JMS 0001
    HLT         / Halt indicating test failed

TEST_PASS,
    CLA
    TAD PLUS_CHAR
    DCA 0005    / vptr character value
    TAD ZERO    
    DCA 0004    / Operation PRN_PUTCH
    JMS 0001
    HLT         / Halt with AC=1 if test passed

NEG2, 7776      / -2 in octal
DOT_CHAR, 056   / ASCII '.'
PLUS_CHAR, 053  / ASCII '+' in octal (pass)
HYPHEN_CHAR,
    055         / ASCII '-' (fail)
ZERO, 0000
