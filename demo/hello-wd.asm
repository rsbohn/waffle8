 / hello-wd.asm
 / Demo: use watchdog peripheral to trigger a HALT after a short timeout.
 /
 / This program demonstrates writing the watchdog control register using
 / the IOT interface. The watchdog device is expected to be present in
 / the monitor configuration (see pdp8.config). Note: the `iot` key in
 / the config is documentation only; the monitor attaches the watchdog
 / to the built-in device code defined in the emulator headers.

        *0200
START,  CLA CLL                 / clear AC and link
        TAD WD_CTRL             / load watchdog control word into AC
        06551                  / IOT: watchdog WRITE (device 055 octal, write bit = 1)

        / busy loop - continue executing instructions until watchdog HALTs
LOOP,   ISZ  TEMP               / increment TEMP to show activity
        JMP  LOOP

WD_CTRL, 03005                 / control: CMD=3 (HALT one-shot), COUNT=5 deciseconds
TEMP,    00000                 / activity counter
