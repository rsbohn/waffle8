*0200
/ Read and print magtape label from unit 1, record 0
/ Label is first 3 words (6 SIXBIT characters, 2 per word)
/ SIXBIT encoding: 01-32='A'-'Z', 33-42='0'-'9', 00=space

START,
    CLA              / Clear AC
    TAD UNIT1        / Load unit 1 selector
    IOT 06701        / GO - select unit 1
    
    IOT 06720        / REWIND - go to record 0
    
    / Wait for ready
WAIT,
    IOT 06710        / SKIP if ready
    JMP WAIT         / Not ready, wait
    
    / Read 3 words of label (6 SIXBIT chars)
    CLA
    IOT 06702        / READ word 1 into AC
    DCA WORD1        / Save it
    
    IOT 06702        / READ word 2
    DCA WORD2        / Save it
    
    IOT 06702        / READ word 3
    DCA WORD3        / Save it
    
    / Print char 1 from WORD1 (bits 11-6)
    CLA
    TAD WORD1        / Load word 1
    RTR              / Rotate right 6 times
    RTR
    RTR
    AND MASK77       / Mask to 6 bits
    JMS CNVRT        / Convert SIXBIT to ASCII
    IOT 06044        / Print
    
    / Print char 2 from WORD1 (bits 5-0)
    CLA
    TAD WORD1
    AND MASK77       / Mask to 6 bits
    JMS CNVRT        / Convert
    IOT 06044
    
    / Print char 3 from WORD2 (bits 11-6)
    CLA
    TAD WORD2
    RTR
    RTR
    RTR
    AND MASK77
    JMS CNVRT
    IOT 06044
    
    / Print char 4 from WORD2 (bits 5-0)
    CLA
    TAD WORD2
    AND MASK77
    JMS CNVRT
    IOT 06044
    
    / Print char 5 from WORD3 (bits 11-6)
    CLA
    TAD WORD3
    RTR
    RTR
    RTR
    AND MASK77
    JMS CNVRT
    IOT 06044
    
    / Print char 6 from WORD3 (bits 5-0)
    CLA
    TAD WORD3
    AND MASK77
    JMS CNVRT
    IOT 06044
    
    / Print newline
    CLA
    TAD CRLF
    IOT 06044
    CLA
    TAD LINEF
    IOT 06044
    
    HLT              / Done

/ SIXBIT to ASCII conversion subroutine
/ Input: AC contains SIXBIT character (0-77)
/ Output: AC contains ASCII character
/ Modifies: AC only
CNVRT, 0000          / Return address
    DCA TEMP         / Save SIXBIT value
    TAD TEMP
    SZA              / Skip if zero (space)
    JMP NOTSPC
    CLA
    TAD SPC          / Load ASCII space
    JMP I CNVRT      / Return
NOTSPC,
    TAD TEMP
    TAD M33          / Subtract 33 (check if digit)
    SPA              / Skip if positive or zero (digit range)
    JMP LETTER
    / It's a digit (33-42 → '0'-'9')
    CLA
    TAD TEMP
    TAD DIG0         / Add offset for '0' - 33
    JMP I CNVRT
LETTER,
    / It's a letter (01-32 → 'A'-'Z')
    CLA
    TAD TEMP
    TAD LETA         / Add offset for 'A' - 1
    JMP I CNVRT

/ Data area
UNIT1,   0001        / Unit 1 selector
WORD1,   0000        / Storage for label word 1
WORD2,   0000        / Storage for label word 2
WORD3,   0000        / Storage for label word 3
MASK77,  0077        / Mask for 6 bits
TEMP,    0000        / Temp storage
M33,     7745        / -33 in 12-bit two's complement
DIG0,    0017        / '0' - 33 = 48 - 33 = 15
LETA,    0100        / 'A' - 1 = 65 - 1 = 64
SPC,     0040        / ASCII space
CRLF,    0015        / Carriage return
LINEF,   0012        / Line feed