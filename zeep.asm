*0200
/ Read and print magtape label from unit 1, record 0
/ Label is first 3 words (6 SIXBIT characters, 2 per word)
/ SIXBIT encoding: 01-32='A'-'Z', 32-41='0'-'9', 00=space

START,
    CLA              / Clear AC
    TAD UNIT0        / Load unit 0 selector
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

    / Print the three header words as octal (4 digits each)
    CLA
    TAD WORD1
    JMS PRINTOC      / Print WORD1 in octal (PRINTOC prints trailing space)

    CLA
    TAD WORD2
    JMS PRINTOC      / Print WORD2 in octal (PRINTOC prints trailing space)

    CLA
    TAD WORD3
    JMS PRINTOC      / Print WORD3 in octal (PRINTOC prints trailing space)

    / Print newline (CR/LF)
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
PRINTOC, 0000        / Return address -- print AC as 4 octal digits
    DCA SAVWORD      / Save the word to print

    CLA
    DCA CNT          / Clear digit counter

    / Digit for 8^3 = 512
L512,  TAD SAVWORD
    TAD M512         / subtract 512
    SPA              / if result >= 0
    JMP SKIP512
DO512, DCA SAVWORD   / store reduced value
    TAD CNT
    TAD ONE
    DCA CNT
    JMP L512
SKIP512,
    CLA
    TAD ASCII0
    TAD CNT
    IOT 06044        / print ASCII digit
    CLA
    DCA CNT

    / Digit for 8^2 = 64
L64,   TAD SAVWORD
    TAD M64
    SPA
    JMP SKIP64
DO64,  DCA SAVWORD
    TAD CNT
    TAD ONE
    DCA CNT
    JMP L64
SKIP64,
    CLA
    TAD ASCII0
    TAD CNT
    IOT 06044
    CLA
    DCA CNT

    / Digit for 8^1 = 8
L8,    TAD SAVWORD
    TAD M8
    SPA
    JMP SKIP8
DO8,   DCA SAVWORD
    TAD CNT
    TAD ONE
    DCA CNT
    JMP L8
SKIP8,
    CLA
    TAD ASCII0
    TAD CNT
    IOT 06044
    CLA
    DCA CNT

    / Digit for 8^0 = 1
L1,    TAD SAVWORD
    TAD M1
    SPA
    JMP SKIP1
DO1,   DCA SAVWORD
    TAD CNT
    TAD ONE
    DCA CNT
    JMP L1
SKIP1,
    CLA
    TAD ASCII0
    TAD CNT
    IOT 06044
    CLA
    DCA CNT

    / print a space separator after the 4 octal digits
    CLA
    TAD SPC
    IOT 06044

    JMP I PRINTOC

/ Data area
UNIT0,   0000        / Unit 0 selector
WORD1,   0000        / Storage for label word 1
WORD2,   0000        / Storage for label word 2
WORD3,   0000        / Storage for label word 3
MASK77,  0077        / Mask for 6 bits
TEMP,    0000        / Temp storage
M32,     7740        / -32 in 12-bit two's complement
DIG0,    0020        / '0' - 32 = 48 - 32 = 16
LETA,    0100        / 'A' - 1 = 65 - 1 = 64
SPC,     0040        / ASCII space
CRLF,    0015        / Carriage return
LINEF,   0012        / Line feed

/* PRINTOC support data */
SAVWORD, 0000        / saved word for PRINTOC
CNT,     0000        / digit counter
ONE,     0001        / constant 1
M512,    7000        / -512 (neg weight for 8^3)
M64,     7700        / -64  (neg weight for 8^2)
M8,      7770        / -8   (neg weight for 8^1)
M1,      7777        / -1   (neg weight for 8^0)
ASCII0,  0060        / ASCII '0'