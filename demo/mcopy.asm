/ Copy COUNT words from [ZP:10] SRC to [ZP:11] DEST ascending
/ COUNT should be negative (e.g., -5 to copy 5 words)
/ SRC and DEST should contain source and destination addresses
/ Auto-increment addressing handles pointer advancement automatically

            *0300
MCOPY,      0               / return addr goes here
            CLA CLL
MCLOOP,
            ISZ COUNT       / increment count, skip when zero
            JMP I MCOPY     / exit directly when count reaches zero
            TAD I SRC       / load word from source (auto-increments SRC)
            DCA I DEST      / store word to destination (auto-increments DEST)
            JMP MCLOOP      / continue loop

/ Zero page variable addresses
COUNT,      0012            / word count location
SRC,        0010            / source address pointer location  
DEST,       0011            / destination address pointer location
