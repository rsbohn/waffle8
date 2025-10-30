        / Interrupt-Driven I/O Example for PDP-8 Emulator
        / Demonstrates interrupt handling with software device polling via skip chains
        /
        / PDP-8 Interrupt Model:
        / - Single hardware interrupt line (not per-device)
        / - CPU saves context at 0x0006-0x0008 when interrupt dispatches
        / - Jumps to ISR at 0x0010
        / - ISR must poll devices using ISK (Interrupt Skip If flag set)
        / - ISR disables interrupts, so multiple IRQs need ISR to run repeatedly
        /
        / Context Save Layout (automatic, done by CPU):
        / 0x0006 (octal 0006): AC (accumulator value before interrupt)
        / 0x0007 (octal 0007): PC (return address = PC at end of interrupted instruction)
        / 0x0008 (octal 0010): LINK (link register)
        /
        / Device ISK Instructions (examples):
        / Keyboard (device 03): ISK 6031 -> skip if keyboard flag set
        / Printer  (device 04): ISK 6041 -> skip if printer ready
        / Watchdog (device 55): ISK 6551 -> skip if watchdog expired
        /
        / Key Instructions:
        / ION (0xF01) = operate group 2, bit 0=1, enables interrupts
        / IOFF (0xF00) = operate group 2, bit 0=0, disables interrupts
        / ISK 6551 = Check watchdog expired flag

        / === RESET VECTOR (locations 0000-0001) ===
        * 0000
        0240            / JMP I 20  - Jump indirectly through location 0020
        
        * 0002
        / (space reserved for boot parameters)
        * 0006
        / (space reserved for interrupt context)
        * 0010
INTR,   CLA CLL         / Start of ISR: clear AC and LINK
        
        / === DEVICE POLLING ===
        / Poll watchdog: does it have an interrupt pending?
        ISK 6551        / Check watchdog flag (device 55, ISK operation)
        JMP POLL2       / Device flag NOT set, try next
        JMS HANDLE_WD   / Device flag IS set, call handler
        
POLL2,  / Poll next device (keyboard example)
        ISK 6031        / Check keyboard flag (device 03, ISK operation)
        JMP POLL3       / Keyboard not ready, try next
        JMS HANDLE_KBD  / Keyboard has data, handle it
        
POLL3,  / More device checks could go here
        
        / === CONTEXT RESTORE AND RETURN ===
DONE,   
        / CPU saved context automatically when jumping to ISR at 0x0010:
        / - AC saved at 0x0006
        / - PC saved at 0x0007 (address of next instruction after interrupt)
        / - LINK saved at 0x0008
        / - Interrupts were DISABLED
        /
        / To return, we re-enable interrupts and jump back
        
        ION             / Re-enable interrupts
        TAD I 6         / Load saved AC from address 0x0006 (indirect addressing)
        RAL             / Restore AC into accumulator (already there but show explicitly)
        JMP I 7         / Jump indirectly through address 0x0007 (saved PC)
        
HANDLE_WD,      / Watchdog interrupt handler
        / In a real application, would:
        / - Clear watchdog flag (ISK 6551 or IOT read)
        / - Increment a counter
        / - Re-enable watchdog timer if periodic mode
        / - Do periodic maintenance work
        / For this example, we just handle and return
        DCA TICKS       / Record that we got an interrupt
        JMP DONE        / Return to main ISR exit
        
HANDLE_KBD,     / Keyboard interrupt handler (example)
        / Would read keyboard character and store in buffer
        DCA INBUF       / Store keyboard data
        JMP DONE        / Return to main ISR exit
        
        / === MAIN PROGRAM ===
        * 0100
START,  CLA CLL         / Clear AC and LINK
        ION             / 0xF01 Enable interrupts (group 2 operate)
        
LOOP,   / Main program loop
        TAD COUNTER     / Load counter
        TAD =1          / Add 1
        DCA COUNTER     / Store back
        
        / Simulate some work, then wait for interrupt
        NOP             / In real program, would do useful work
        NOP             / while interrupt handler runs in background
        NOP
        
        JMP LOOP        / Continue looping
        
        / === MAIN PROGRAM DATA ===
        * 0200
COUNTER, 0              / Counter value for loop
        
        / === ISR DATA ===
        * 0250
TICKS,  0               / Interrupt tick counter
INBUF,  0               / Input buffer for keyboard data
        
        
