    * 200
/ Fill 05000..05400 (256 words) with 040 (space)
START,
LOOP,   CLA CLL            / Clear AC and Link
        TAD VAL            / AC <- value to store (040)
        DCA I PTR          / Store AC into memory pointed at by PTR
        ISZ PTR            / Increment pointer (point to next word)
        ISZ CNT            / Increment counter; when it wraps to 0 skip next
        JMP LOOP           / Repeat until CNT wraps to zero
        HLT                / Done - halt

/* Data: initialize pointer, counter and value */
PTR,    05000
CNT,    07400              / 07400 + 0400 = 10000 -> wraps to 0 after 0400 (256) increments
VAL,    0040
NP,     0000
LC,     07700

    * 0220
LPRINT, CLA CLL
        TAD LC
        DCA NP
LPLOOP,
        IOT 6041
        JMP .-1
        CLL CLA
        TAD I PTR
        IOT 6046
        ISZ PTR
        ISZ NP
        JMP LPLOOP
        JMS CRLF
        HLT
        JMP LPRINT      / do it again

CR,     0015
LF,     0012
CRLF,   0000
        CLA CLL
        IOT 6041
        JMP .-1
        TAD CR
        IOT 6046
        CLA CLL
        IOT 6041
        JMP .-1
        TAD LF
        IOT 6046
        JMS I CRLF

