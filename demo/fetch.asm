/ fetch.asm
/ Simple "neofetch" style hardware report for Waffle8.
/ Prints a tiny ASCII banner plus host details, then times one full RTC minute
/ (address 07760) to estimate how many measurement loops execute per minute.
/ Because the RTC only exposes minutes, the speed sample takes roughly 60–120
/ seconds (waiting for the next minute boundary and timing the subsequent one).

	*0040
/------------------------------------------------------------
/ Zero-page storage and constants
/------------------------------------------------------------
LIST_PTR,	0
TMP_PTR,	0
STR_PTR,	0
CHBUF,	0
OCT_TMP,	0
RTC_VALUE,	0
RTC_LAST,	0
RTC_LAST_NEG,	0
RTC_START,	0
RTC_START_NEG,	0
RTC_CUR,	0
SPEED_LO,	0
SPEED_MI,	0
SPEED_HI,	0

RTC_ADDR_PTR,	07760
ART_TABLE_PTR,	ART_TABLE
STATIC_TABLE_PTR,	STATIC_TABLE
RTC_LABEL_PTR,	RTC_LABEL
RTC_SUFFIX_PTR,	RTC_SUFFIX
SPEED_WAIT_PTR,	SPEED_WAIT
SPEED_LABEL_PTR,	SPEED_LABEL
SPEED_SUFFIX_PTR,	SPEED_SUFFIX

ASCII_ZERO,	0060
QUOTE_CHAR,	0047
K0007,	0007
K0070,	0070
K0700,	0700
K7000,	7000

READ_RTC_VEC,	READ_RTC
MEASURE_VEC,	MEASURE_SPEED

	*0200
START,	CLA CLL
	TAD ART_TABLE_PTR
	JMS PRINT_TABLE

	CLA CLL
	TAD STATIC_TABLE_PTR
	JMS PRINT_TABLE

	JMS PRINT_RTC_LINE
	JMS PRINT_SPEED_LINE
	HLT

/------------------------------------------------------------
/ PRINT_TABLE - Print all strings referenced by a pointer table.
/ Entry: AC = address of table containing string addresses, terminated by 0.
/------------------------------------------------------------
PRINT_TABLE,	0
	DCA LIST_PTR
PT_LOOP,	TAD I LIST_PTR
	SZA
	JMP PT_PRINT
	JMP I PRINT_TABLE
PT_PRINT,	DCA TMP_PTR
	CLA CLL
	TAD TMP_PTR
	JMS PUTS
	ISZ LIST_PTR
	JMP PT_LOOP

/------------------------------------------------------------
/ PRINT_RTC_LINE - Show current RTC minutes in octal.
/------------------------------------------------------------
PRINT_RTC_LINE,	0
	CLA CLL
	TAD RTC_LABEL_PTR
	JMS PUTS
	JMS I READ_RTC_VEC
	DCA RTC_VALUE
	CLA CLL
	TAD RTC_VALUE
	JMS PRINT_OCTAL
	CLA CLL
	TAD RTC_SUFFIX_PTR
	JMS PUTS
	JMP I PRINT_RTC_LINE

/------------------------------------------------------------
/ PRINT_SPEED_LINE - Measure and print loop count per RTC minute.
/------------------------------------------------------------
PRINT_SPEED_LINE,	0
	CLA CLL
	TAD SPEED_WAIT_PTR
	JMS PUTS
	JMS I MEASURE_VEC
	CLA CLL
	TAD SPEED_LABEL_PTR
	JMS PUTS
	JMS PRINT_SPEED_VALUE
	CLA CLL
	TAD SPEED_SUFFIX_PTR
	JMS PUTS
	JMP I PRINT_SPEED_LINE

/------------------------------------------------------------
/ PRINT_SPEED_VALUE - Emit SPEED_HI'MID'LO as octal groups.
/------------------------------------------------------------
PRINT_SPEED_VALUE,	0
	CLA CLL
	TAD SPEED_HI
	JMS PRINT_OCTAL
	CLA CLL
	TAD QUOTE_CHAR
	JMS PUTCH
	CLA CLL
	TAD SPEED_MI
	JMS PRINT_OCTAL
	CLA CLL
	TAD QUOTE_CHAR
	JMS PUTCH
	CLA CLL
	TAD SPEED_LO
	JMS PRINT_OCTAL
	JMP I PRINT_SPEED_VALUE

/------------------------------------------------------------
/ PRINT_OCTAL - Print AC as four-digit octal.
/------------------------------------------------------------
PRINT_OCTAL,	0
	DCA OCT_TMP

	TAD OCT_TMP
	AND K7000
	RAR
	RAR
	RAR
	RAR
	RAR
	RAR
	RAR
	RAR
	RAR
	TAD ASCII_ZERO
	JMS PUTCH

	TAD OCT_TMP
	AND K0700
	RAR
	RAR
	RAR
	RAR
	RAR
	RAR
	TAD ASCII_ZERO
	JMS PUTCH

	TAD OCT_TMP
	AND K0070
	RAR
	RAR
	RAR
	TAD ASCII_ZERO
	JMS PUTCH

	TAD OCT_TMP
	AND K0007
	TAD ASCII_ZERO
	JMS PUTCH
	JMP I PRINT_OCTAL

/------------------------------------------------------------
/ PUTS - Print null-terminated string via PUTCH.
/------------------------------------------------------------
PUTS,	0
	DCA STR_PTR
PUTS_LOOP,	TAD I STR_PTR
	SZA
	JMP PUTS_CHAR
	JMP I PUTS
PUTS_CHAR,	JMS PUTCH
	ISZ STR_PTR
	JMP PUTS_LOOP

/------------------------------------------------------------
/ PUTCH - Block until KL8E ready, then transmit character in AC.
/------------------------------------------------------------
PUTCH,	0
	DCA CHBUF
PUTCH_WAIT,	IOT 6041
	JMP PUTCH_WAIT
	TAD CHBUF
	IOT 6046
	CLA CLL
	JMP I PUTCH

/------------------------------------------------------------
/ READ_RTC - Return minutes since midnight from 07760.
/------------------------------------------------------------
READ_RTC,	0
	TAD I RTC_ADDR_PTR
	JMP I READ_RTC

/------------------------------------------------------------
/ MEASURE_SPEED - Count loop iterations across one RTC minute.
/------------------------------------------------------------
	*0400
MEASURE_SPEED,	0
	JMS I READ_RTC_VEC
	DCA RTC_LAST
	CLA CLL
	TAD RTC_LAST
	CMA
	IAC
	DCA RTC_LAST_NEG

WAIT_BOUNDARY,	JMS I READ_RTC_VEC
	DCA RTC_CUR
	CLA CLL
	TAD RTC_CUR
	TAD RTC_LAST_NEG
	SZA
	JMP BEGIN_SAMPLE
	JMP WAIT_BOUNDARY

BEGIN_SAMPLE,	DCA RTC_START
	CLA CLL
	TAD RTC_START
	CMA
	IAC
	DCA RTC_START_NEG

	CLA CLL
	DCA SPEED_LO
	DCA SPEED_MI
	DCA SPEED_HI

SAMPLE_LOOP,	JMS INC_SPEED
	JMS I READ_RTC_VEC
	DCA RTC_CUR
	CLA CLL
	TAD RTC_CUR
	TAD RTC_START_NEG
	SZA
	JMP SAMPLE_DONE
	JMP SAMPLE_LOOP

SAMPLE_DONE,	JMP I MEASURE_SPEED

/------------------------------------------------------------
/ INC_SPEED - Increment SPEED_LO/MI/HI as 36-bit counter.
/------------------------------------------------------------
INC_SPEED,	0
	ISZ SPEED_LO
	JMP INC_RET
	ISZ SPEED_MI
	JMP INC_RET
	ISZ SPEED_HI
	JMP INC_RET
INC_RET,	JMP I INC_SPEED

/------------------------------------------------------------
/ Pointer tables
/------------------------------------------------------------
	*0600
ART_TABLE,
	ART_LINE1
	ART_LINE2
	ART_LINE3
	ART_LINE4
	0

STATIC_TABLE,
	STATIC_LINE0
	STATIC_LINE1
	STATIC_LINE2
	STATIC_LINE3
	0

/------------------------------------------------------------
/ Strings
/------------------------------------------------------------
ART_LINE1,
	"W";"A";"F";"F";"L";"E";"8";0015;0012;0000

ART_LINE2,
	"[";" ";"P";"D";"P";"-";"8";" ";"]";0015;0012;0000

ART_LINE3,
	"=";"=";"=";"=";"=";"=";"=";"=";"=";"=";0015;0012
	0000

ART_LINE4,
	"m";"i";"n";"i";" ";"f";"e";"t";"c";"h";0015;0012
	0000

STATIC_LINE0,
	0015;0012;0000

STATIC_LINE1,
	"O";"S";" ";" ";" ";" ";" ";" ";":";" ";"M";"o"
	"n";"i";"t";"o";"r";" ";"R";"O";"M";0015;0012;0000

STATIC_LINE2,
	"B";"o";"a";"r";"d";" ";" ";" ";":";" ";"H";"o"
	"s";"t";" ";"S";"i";"m";"u";"l";"a";"t";"o";"r"
	0015;0012;0000

STATIC_LINE3,
	"D";"e";"m";"o";" ";" ";" ";" ";":";" ";"d";"e"
	"m";"o";0057;"f";"e";"t";"c";"h";".";"a";"s";"m"
	0015;0012;0000

RTC_LABEL,
	"R";"T";"C";" ";" ";" ";" ";" ";":";" ";0000

RTC_SUFFIX,
	" ";"m";"i";"n";"u";"t";"e";"s";" ";"s";"i";"n"
	"c";"e";" ";"m";"i";"d";"n";"i";"g";"h";"t";0015
	0012;0000

SPEED_WAIT,
	"S";"p";"e";"e";"d";" ";" ";" ";":";" ";"c";"a"
	"l";"i";"b";"r";"a";"t";"i";"n";"g";" ";"f";"o"
	"r";" ";"o";"n";"e";" ";"m";"i";"n";"u";"t";"e"
	".";".";".";0015;0012;0000

SPEED_LABEL,
	"S";"p";"e";"e";"d";" ";" ";" ";":";" ";0000

SPEED_SUFFIX,
	" ";"l";"o";"o";"p";"s";0057;"m";"i";"n";" ";"("
	"o";"c";"t";"a";"l";" ";"h";"i";0047;"m";"i";"d"
	0047;"l";"o";")";0015;0012;0000

	$
