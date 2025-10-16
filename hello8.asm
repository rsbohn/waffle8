/ Fill memory range 1000-1007 with octal 0210
/
*4000
        CLA             / AC = 0
        TAD     START   / AC = 01000 (target base)
        DCA     PTR     / PTR = 01000
        TAD     COUNTINIT / AC = -8 (loop count)
        DCA     COUNT   / COUNT = -8
LOOP,   CLA
        TAD     VALUE   / AC = fill pattern
        DCA I   PTR     / store at *PTR, AC cleared
        ISZ     PTR     / advance to next address
        ISZ     COUNT   / increment count, skip when zero
        JMP     LOOP
        HLT

/ Constants and workspace (same page to avoid page crossings)
VALUE,      0210
START,      01000
COUNTINIT,  07770      / -8 in two's complement
PTR,        0000
COUNT,      0000

/ Monitor load sequence (octal words):
/ reset
/ load 4000 7200 01221 03223 01222 03224 7200 01220 03623 02223 02224 05205 07402
/ load 4020 00210 01000 07770 00000 00000
/ load 0020 04000
/ load 0000 05420
/ run 100
/ mem 1000 10
