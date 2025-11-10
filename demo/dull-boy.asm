/ dull-boy.asm
/ Print "All work and no play make Jack a dull boy." on the console repeatedly.
/ Inlines the core PUTS/PUTCH routines so the entire demo lives on page 1 (0200).

        *0200                   / Program origin

START,  CLA CLL                 / Clear AC and Link before entering loop
        TAD WD_CTRL             / load watchdog control word into AC
        IOT 06551              / IOT: watchdog WRITE (device 055 octal, write bit = 1)
        CLA

LOOP,
        CLA                     / Reset AC before loading string pointer
        TAD MESSAGE_PTR         / Load pointer to message
        JMS PUTS                / Print the message
        IOT 06554              / IOT: watchdog RESTART (device 055 octal, restart bit = 4)
        JMP LOOP                / Repeat forever

/------------------------------------------------------------
/ PUTS - Print null-terminated string via PUTCH
/ Entry: AC contains pointer to string
/------------------------------------------------------------
PUTS,   0
        DCA STR_PTR             / Save pointer and clear AC
PUTS_LOOP,
        TAD I STR_PTR           / Load next character
        SZA                     / Null terminator ends string
        JMP PUTS_CHAR
        JMP I PUTS              / Return when terminator reached
PUTS_CHAR,
        JMS PUTCH               / Output character in AC
        /JMS SLOWLY              / Delay between characters for dramatic effect
        ISZ STR_PTR             / Advance pointer
        JMP PUTS_LOOP           / Continue until terminator

/------------------------------------------------------------
/ PUTCH - Block until KL8E ready, then transmit character in AC
/------------------------------------------------------------
PUTCH,  0
        DCA CHBUF               / Save character, AC cleared
PUTCH_WAIT,
        IOT 6041                / Skip when teleprinter ready
        JMP PUTCH_WAIT          / Wait for ready
        TAD CHBUF               / Reload character
        IOT 6046                / Transmit via teleprinter
        CLA                     / Leave AC clear for callers
        JMP I PUTCH

/------------------------------------------------------------
/ SLOWLY - Busy-wait delay to slow visible output
/------------------------------------------------------------
SLOWLY, 0
        TAD DELAY_OUTER_INIT    / Reload outer loop counter (-32)
        DCA DELAY_OUTER
SLOWLY_OUTER,
        TAD DELAY_INNER_INIT    / Reload inner loop counter (-256)
        DCA DELAY_INNER
SLOWLY_INNER,
        ISZ DELAY_INNER         / Count up toward zero
        JMP SLOWLY_INNER        / Loop until inner counter reaches 0
        ISZ DELAY_OUTER         / Advance outer counter
        JMP SLOWLY_OUTER        / Repeat outer loop while non-zero
        JMP I SLOWLY            / Done delaying

/------------------------------------------------------------
/ Storage and message data (same page to avoid page crossings)
/------------------------------------------------------------
STR_PTR,    0000                / Working pointer for PUTS
CHBUF,      0000                / Character buffer for PUTCH
DELAY_OUTER,0000                / Outer loop workspace for SLOWLY
DELAY_INNER,0000                / Inner loop workspace for SLOWLY
MESSAGE_PTR, MESSAGE            / Pointer constant for message
DELAY_OUTER_INIT, 07740         / -32 for outer loop iterations
DELAY_INNER_INIT, 07400         / -256 for inner loop iterations

/ Message text terminated with CR/LF and null
MESSAGE,
        "A";"l";"l";" ";"w";"o";"r";"k";" ";"a";"n";"d";" ";
        "n";"o";" ";"p";"l";"a";"y";" ";"m";"a";"k";"e";" ";
        "J";"a";"c";"k";" ";"a";" ";"d";"u";"l";"l";" ";"b";"o";"y";".";
        0015;0012;0000

WD_CTRL, 03100                 / control: CMD=3 (HALT one-shot), COUNT=5 deciseconds

        $
