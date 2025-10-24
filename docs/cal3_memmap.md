# CAL3.ASM Memory Map
# Simple calendar printer program memory layout

## Page 0 (0000-0077): Workspace and Constants
0020-0036: Variables and workspace
  0020: YEAR_VALUE     - Current year value
  0021: MONTH_VALUE    - Current month value (1-12)
  0022: MONTH_OFF      - Month offset (0-11)
  0023: YEAR_TMP       - Temporary year for printing
  0024: REM_VALUE      - Remainder value for calculations
  0025: TENS           - Tens digit counter
  0026: PRT2D_WORK     - Print 2-digit workspace
  0027: LEAD_CHAR      - Leading character for padding
  0030: DAY_TEMP       - Day temporary value
  0031: CHBUF          - Character buffer for output
  0032: PTR            - General pointer variable

0033-0037: Pointer constants
  0033: YEAR_IN_PTR    - Pointer to YEAR_IN
  0034: MONTH_IN_PTR   - Pointer to MONTH_IN
  0035: MONTH_PTRS_PTR - Pointer to month name pointers

0040-0050: Numeric constants
  0040: NEG_ONE        - -1 (07777)
  0041: NEG_10         - -10 (07766)
  0042: NEG_1900       - -1900 (04224)
  0043: NEG_2000       - -2000 (04060)
  0044: ZERO           - ASCII '0' (0060)
  0045: ONE            - 1 (0001)
  0046: TWO            - 2 (0002)
  0047: NINE           - 9 (0011)
  0050: SPACE          - ASCII space (0040)

0051-0057: Function jump tables
  0051-0052: PRT2D     - Print 2-digit number subroutine
  0053-0054: PRTYEAR   - Print year subroutine
  0055-0056: PRINT     - Print string subroutine
  0057-0060: PUTCH     - Print character subroutine

## Page 1 (0100-0177): Main Program Entry
0100-0157: START routine
  - Initialize variables
  - Load year and month values
  - Print year
  - Print space separator
  - Print month name
  - Halt

## Page 2 (0200-0277): PRT2D Implementation
0200-0247: PRT2D_IMPL
  - Print value in DAY_TEMP as two decimal digits
  - Handles tens/ones digit separation
  - Uses LEAD_CHAR for padding

## Page 3 (0300-0377): PRTYEAR Implementation
0300-0357: PRTYEAR_IMPL
  - Print YEAR_TMP in decimal format
  - Handles years 1957-2099
  - Special logic for 19xx vs 20xx years

## Page 4 (0400-0477): PRINT Implementation
0400-0417: PRINT_IMPL
  - Print null-terminated string
  - Uses PTR to traverse string
  - Calls PUTCH for each character

## Page 5 (0500-0577): PUTCH Implementation
0500-0517: PUTCH_IMPL
  - Output character to line printer (IOT 660x)
  - Waits for printer ready
  - Outputs character from CHBUF

## Page 6 (0600-0677): Input Data and Pointer Tables
0600-0601: Input slots
  0600: YEAR_IN        - Requested year input
  0601: MONTH_IN       - Requested month input

0602-0615: MONTH_NAME_PTRS
  - Array of pointers to month name strings
  - 12 entries (Jan-Dec)

## Page 7 (0700-0777): Month Name Strings
0700-0777: Month name string data
  - JAN_STR: "January"
  - FEB_STR: "February"
  - MAR_STR: "March"
  - APR_STR: "April"
  - MAY_STR: "May"
  - JUN_STR: "June"
  - JUL_STR: "July"
  - AUG_STR: "August"
  - SEP_STR: "September"
  - OCT_STR: "October"
  - NOV_STR: "November"
  - DEC_STR: "December"

## Program Flow
1. START (0100): Initialize and load input values
2. PRTYEAR: Print 4-digit year
3. PRINT: Print month name string
4. PUTCH: Character output via line printer
5. HLT: Program termination

## Memory Usage Summary
- Total program size: ~200 words
- Variables/workspace: Page 0 (0020-0077)
- Code sections: Pages 1-5 (0100-0577)
- Data storage: Pages 6-7 (0600-0777)
- Clear page-based organization for easy navigation