/ Watchdog Ticker

                * 200
START,          CLL CLA
                TAD WDMODE
                IOT 6552
            
SPIN,           IOT 6551
                JMP SPIN
                CLL CLA
                TAD MPTR
                JMS PUTS
                JMP SPIN

MARKER,
                7777
                7777
                7777

PUTS,           0               / PUTS print a string to the teleprinter
                DCA STR_PTR
PUTS_LOOP,      TAD I STR_PTR
                SZA
                JMP PUTS_CHAR
                JMP I PUTS
PUTS_CHAR,      JMS PUTCH
                ISZ STR_PTR
                JMP PUTS_LOOP

PUTCH,          0               / PUTCH - Block until KL8E ready, then transmit character in AC
                DCA CHBUF
PUTCH_WAIT,     IOT 6041        / Skip if teleprinter ready
                JMP PUTCH_WAIT
                TAD CHBUF
                IOT 6046        / Print character
                CLA CLL
                JMP I PUTCH

/ Variables
STR_PTR,        0
CHBUF,          0
WDMODE,         7020            / Watchdog mode setting
MPTR,           MESSAGE
MESSAGE,        "T";"i";"c";"k";"!";012;015
                0               / Null terminator