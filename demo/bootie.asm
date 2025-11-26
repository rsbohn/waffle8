/ Minimal TC08 boot helper.
/ Load block held at location 0020 (defaults to 0000) from dt0 into 0600.

*0020
BLOCK,  0000           / Override this word to pick a different block number

*4000
BOOT,   CLA CLL
        06762           / DCR: reset controller, set ready
        CLA CLL
        TAD     DEST    / Transfer address -> AC
        06771           / DTXA: latch transfer base
        CLA CLL
        TAD     BLOCK   / Block number from 0020
        06766           / DTLB: start 256-word transfer
WAIT,   06764           / DTSF: skip when ready (transfer finished)
        JMP     WAIT
        HLT

DEST,   00600           / Destination address for DMA
