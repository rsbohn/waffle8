PDP-8 Assembly Listing
Source: -
Assembled: 2025-10-30 07:22:43
Origins: 1600

ADDR  WORD  SYMBOL/OPCODE      ; SOURCE
----  ----  ------------------  ----------------------------------------
1600  7300  CLL                 ; CLL CLA
1601  1410  TAD                 ; TAD I 10
1602  3411  DCA                 ; DCA I 11
1603  2012  ISZ                 ; ISZ 12
1604  5200  JMP                 ; JMP 1600
1605  7402  HLT                 ; HLT

Totals: 6 words, 0 errors

/ src=2000 dest=0200 count=7770 (-010)
dep 10 2000 0200 7770