/ echo.asm
/ Echo KL8E keyboard input to teleprinter until EOT (004) received.

        *0200

LOOP,   IOT 6031            / KSF: skip if keyboard flag set
        JMP LOOP            / Wait for input
        IOT 6036            / KRB: read character into AC, clear flag
        DCA CH              / Save character

        CLA
        TAD CH
        TAD NEG_EOT
        SZA                 / Zero if character == EOT
        JMP CHECK_CR
        JMP DONE

CHECK_CR,
        CLA
        TAD CH
        TAD NEG_CR
        SZA                 / Zero if character == CR
        JMP OUTPUT
        JMS PUTCH           / Output CR
        CLA
        TAD LF
        DCA CH
        JMS PUTCH           / Output LF
        JMP LOOP

OUTPUT, JMS PUTCH
        JMP LOOP

DONE,   HLT

PUTCH,  0
PUTCH_WAIT,
        IOT 6041            / TSF: skip if teleprinter ready
        JMP PUTCH_WAIT
        CLA
        TAD CH
        IOT 6046            / TLS: transmit character
        JMP I PUTCH

CH,     0
NEG_EOT, 7774               / -004 (EOT)
NEG_CR, 7763                / -015 (CR)
LF,     0012
