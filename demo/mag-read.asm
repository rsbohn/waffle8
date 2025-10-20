/ Print record from magtape unit 0
/ Read program back from magtape unit 0

        * 0100
READ_BACK,
        CLA CLL             / Select unit 0 (demo files)
        IOT 6701            / Select unit  
        IOT 6720            / Rewind to start
READ_LOOP,
        IOT 6710            / Skip if ready
        JMP READ_DONE       / End of tape
        IOT 6702            / Read word into AC
        / send to PRN
PRINT_WAIT,
        IOT 6601                / Skip when printer ready
        JMP PRINT_WAIT          / Wait if not ready
        IOT 6606                / Clear ready + print character
        JMP READ_LOOP           / Continue
READ_DONE,
        HLT