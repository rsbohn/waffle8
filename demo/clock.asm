/ a simple clock
/ uses the RTC value at 07760
		* 0000
		JMP I 0002
		HLT
		BOOT		/ reset vector jumps into watchdog bootstrap

		* 0200
START,		CLA CLL		/ main: HOURS := NOW div5 div3 div2 div2, MINS := NOW mod60
		TAD I PRTC
		DCA TIME_NOW
		TAD TIME_NOW
		JMS DIV5
		JMS DIV3
		JMS DIV2
		JMS DIV2
		DCA HOURS
		TAD TIME_NOW
		JMS MOD60
		DCA MINS
		JMS I _PRINT
		ION		/ re-enable interrupts before idling
SPIN,		JMP SPIN	/ busy wait on watchdog

/ -------- variables -------
PRTC,		07760		/ PRTC
TIME_NOW,	0		/ TIME NOW
HOURS,		0		/ HOURS
MINS,		0		/ MINUTES
_PRINT,		PRINT


DIV2,		0
		CLL RAR
		JMP I DIV2

DIV3,		0
		DCA VALUE
		DCA QUOT
D3LOOP,		CLA CLL
		TAD VALUE
		TAD M3
		SPA
		JMP D3DONE
		DCA VALUE
		ISZ QUOT
		JMP D3LOOP
D3DONE,		CLA CLL
		TAD QUOT
		JMP I DIV3

QUOT,		0
VALUE,		0
TEMP,		0
M3,		07775
M5,		07773
DIV5,		0
		DCA TEMP
		DCA QUOT
D5LOOP,		CLA CLL
		TAD TEMP
		TAD M5
		SPA
		JMP D5DONE
		DCA TEMP
		ISZ QUOT
		JMP D5LOOP
D5DONE,		CLA CLL
		TAD QUOT
		JMP I DIV5

DIV10,		0
		JMS DIV5
		JMS DIV2
		JMP I DIV10

MOD60,		0
M60A,		TAD M60
		SMA 			/ go until AC is < 0
		JMP M60A
		TAD P60
		JMP I MOD60

M60,		07704
P60,		00074
	
		
		///// Printing
		*0400
PRINT,		0
		JMS PRCR
		/ HH : MM (octal)
		CLL CLA
		TAD I _HOURS
		JMS PRD2
		JMS PRCOLON
		CLL CLA
		TAD I _MINS
		JMS PRD2
		JMP I PRINT

MARKER,		5555
		2222
_HOURS,		HOURS
_MINS,		MINS
PRTEM,		0000	/ scratchpad
PRMASK,		0007	/ mask a single digit
CR,		0015	/ carriage return
AZERO,		"0"	/ ASCII ZERO
COLON,		":"

PRD2,		0	/ Print 2 digits
		DCA PRTEM
		TAD PRTEM
		CLL RAR		/ DIV8
		CLL RAR
		CLL RAR
		AND PRMASK	/ Just the lowest three bits
		TAD AZERO	/ 0-7 ascii
		JMS PUTCH
		CLA CLL
		TAD PRTEM
		AND PRMASK
		TAD AZERO
		JMS PUTCH
		JMP I PRD2

PRCOLON,	0		/ PRCOLON
		CLA CLL
		TAD COLON
		JMS PUTCH
		JMP I PRCOLON

PRCR,		0		/ PRCR carriage return
		CLA CLL
		TAD CR
		JMS PUTCH
		JMP I PRCR

PUTCH,		0		/ PUTCH
PUTCH0,		IOT 6601	/ wait for ready
		JMP PUTCH0
		IOT 6606	/ print
		JMP I PUTCH

		* 0310
WD_PRESET,	02070		/ watchdog: reset periodic (cmd 4), 64 deciseconds

		* 0320
BOOT,		CLA CLL		/ configure watchdog then jump into main loop
		TAD WD_PRESET
		IOT 6552	/ PDP8_WATCHDOG_WRITE
		ION
		JMP START
		
