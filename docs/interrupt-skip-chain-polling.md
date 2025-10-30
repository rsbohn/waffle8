# Interrupt Device Polling with Skip Chains

## Overview

With a single interrupt line (not per-device vectors), the interrupt service routine must identify which device(s) need service. The PDP-8 solution: **skip chain polling using ISK instructions**.

## The ISK Instruction: "Interrupt Skip if Flag"

Each device has its own ISK instruction that:
1. **Checks** if the device has a pending interrupt/status flag
2. **Skips** the next instruction if the flag is set
3. **Clears** the flag (typically)

### Examples

```
Device Code | ISK Opcode | Meaning
────────────┼────────────┼─────────────────────────
  03 (KL8E) │    6031    │ Skip if keyboard ready
  04 (KL8E) │    6041    │ Skip if printer ready
  55 (WD)   │    6551    │ Skip if watchdog expired
  60 (LP)   │    6601    │ Skip if printer ready
  70 (MT)   │    6701    │ Skip if magtape ready
```

## Skip Chain Pattern

The ISR uses a **skip chain** to poll each device and dispatch to the appropriate handler:

```asm
        * 0010
INTR_HANDLER,
        CLA CLL             ; Clear and prepare
        
        / Try device 1 (KL8E keyboard, device 03)
        ISK 6031            ; Skip if keyboard interrupt
        JMP TRY_DEVICE2     ; No - try next
        JMS HANDLE_KBD      ; Yes - handle keyboard
        
TRY_DEVICE2,
        / Try device 2 (KL8E printer, device 04)
        ISK 6041            ; Skip if printer interrupt  
        JMP TRY_DEVICE3     ; No - try next
        JMS HANDLE_PRINTER  ; Yes - handle printer
        
TRY_DEVICE3,
        / Try device 3 (Watchdog, device 55)
        ISK 6551            ; Skip if watchdog interrupt
        JMP NO_DEVICE       ; No devices interrupted?
        JMS HANDLE_WD       ; Yes - handle watchdog
        
NO_DEVICE,
        / All devices polled, none had flag set
        / (This shouldn't happen if interrupt was pending)
        
        / Restore context and return
        CLA
        TAD 0006            ; Restore AC
        DCA TEMP
        TAD 0007            ; Restore PC  
        DCA 0011            ; Return address
        
        ION                 ; Re-enable interrupts
        JMP I 0011          ; Return to interrupted code
```

## How It Works: Step by Step

### Scenario: Keyboard interrupt during critical section

```
Timeline:

Time 0.0s: CPU executing CRITICAL section with IOFF
           interrupt_enable = false
           interrupt_pending = 0

Time 0.1s: User presses key
           Keyboard device calls:
             pdp8_api_request_interrupt(cpu, 03)  // device code 03
           
           Result:
             interrupt_pending = 1  (now there's a pending interrupt)
             (but interrupt_enable is still false, so not delivered yet)

Time 0.2s: CPU finishes critical section, executes ION
           interrupt_enable = true

Time 0.3s: Next instruction completes
           pdp8_api_step() checks:
             if (interrupt_enable && interrupt_pending > 0)
           
           YES! So:
             - Save AC, PC, LINK to 0006-0008
             - interrupt_pending--  (now = 0)
             - interrupt_enable = false  (prevent nesting)
             - pc = 0x0010  (jump to ISR)

Time 0.4s: CPU starts executing ISR at address 0x0010
           
           ISK 6031            ; Check if keyboard has flag
           (YES - it does, because keyboard triggered the interrupt)
           
           This instruction SKIPS the JMP
           
           JMS HANDLE_KBD      ; Handle keyboard
           (This line is skipped, so we jump here)
           
           Handler reads keyboard, processes input, returns

Time 0.5s: ISR restores context
           - Loads AC from 0006
           - Loads PC from 0007
           - Executes ION (re-enable interrupts)
           - Jumps back to interrupted code

Time 0.6s: Resumed code continues normally
```

## Why Skip Chains Work

### Advantage 1: Single Interrupt Line
```
Real hardware:  One physical interrupt line (goes high when ANY device needs service)
Our emulator:   One interrupt_pending counter (non-zero when ANY device needs service)
Result:         Both work the same way - ISR must poll all devices
```

### Advantage 2: Software Flexibility
```asm
/ Standard order (check common devices first)
ISK 6031        ; Keyboard first (fast, common)
JMP NO_KEYBOARD
JMS HANDLE_KBD

NO_KEYBOARD,
ISK 6041        ; Printer next
JMP NO_PRINTER
JMS HANDLE_PRINTER

/ OR: Priority order (handle critical devices first)
ISK 6551        ; Watchdog first (highest priority)
JMP NO_WD
JMS HANDLE_WD

NO_WD,
ISK 6031        ; Keyboard next
JMP NO_KEYBOARD
JMS HANDLE_KBD
```

### Advantage 3: Handles Multiple Simultaneous Interrupts
```
Scenario: Keyboard AND printer both interrupt during critical section

Time 0.1s: Keyboard event
           interrupt_pending = 1

Time 0.2s: Printer event  
           interrupt_pending = 2

Time 0.3s: ION executed, ISR dispatched
           - Save context
           - interrupt_pending = 1  (decrement once)
           - Jump to ISR

Time 0.4s: ISR polls devices
           ISK 6031        ; Keyboard flag? YES
           JMS HANDLE_KBD
           
           HANDLE_KBD returns
           
           ISK 6041        ; Printer flag? YES
           JMS HANDLE_PRINTER
           
           HANDLE_PRINTER returns

Time 0.5s: ISR finishes, restores context, executes ION

Time 0.6s: Next instruction completes
           interrupt_pending still > 0!
           So ISR dispatches AGAIN
           
           This time it polls again - but now only one device
           has pending work (the one that triggered the SECOND
           interrupt while we were in the first ISR)
```

## Implementation Notes

### For Each Device: Add ISK Support to IOT Handler

In the device's IOT handler, track a "has_pending_interrupt" flag:

```c
static bool keyboard_interrupt_pending = false;

void kl8e_keyboard_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    uint8_t micro = (uint8_t)(instruction & 0x7u);
    
    // KSF = 0o6031 = skip if keyboard flag (interrupt pending)
    if (micro == 0o031) {
        if (keyboard_interrupt_pending) {
            pdp8_api_request_skip(cpu);  // Skip next instruction
            keyboard_interrupt_pending = false;  // Clear flag
        }
    }
    // ... other micro operations ...
}
```

When keyboard data arrives (via tick handler or external input):
```c
if (data_available) {
    keyboard_data = read_input();
    keyboard_interrupt_pending = true;
    
    if (interrupt_enabled_globally) {
        pdp8_api_request_interrupt(cpu, 03);  // device code 03
    }
}
```

### ISR Structure in Assembly

```asm
        * 0010
INTR,   CLA CLL
        
        / Poll and dispatch sequence
        ISK 6031    ; Device 1?
        JMP CHK2
        JMS SVC1    ; Service device 1
        
CHK2,   ISK 6041    ; Device 2?
        JMP CHK3
        JMS SVC2    ; Service device 2
        
CHK3,   ISK 6551    ; Device 3?
        JMP DONE
        JMS SVC3    ; Service device 3
        
        / Service routines inline or JMP to elsewhere
SVC1,   0
        / Handle device 1
        / ...
        JMP I SVC1
        
SVC2,   0
        / Handle device 2
        / ...
        JMP I SVC2
        
SVC3,   0
        / Handle device 3
        / ...
        JMP I SVC3

DONE,   / Restore and return
        CLA
        TAD 0006    ; Restore AC
        DCA 0010    ; Temp save
        TAD 0007    ; Restore PC
        DCA 0011
        
        ION         ; Re-enable interrupts
        JMP I 0011  ; Return to code
```

## Comparison: Queue vs Single Line with Polling

| Aspect | Per-Device Queue | Single Line + Skip Chain |
|--------|------------------|-------------------------|
| **Interrupt line** | Per-device or multiplexed | Single physical line |
| **Device identification** | From queue entry | Determined by ISK polling |
| **Multiple device interrupts** | Queue holds all | ISR runs repeatedly (once per interrupt) |
| **Worst case dispatch latency** | O(queue_depth) | O(devices_polled) |
| **Memory overhead** | Higher (queue structure) | Lower (just counter) |
| **Matches real PDP-8** | No (PDP-8 had single line) | Yes ✅ |
| **Software flexibility** | Less (queue order fixed) | More (skip chain order configurable) |
| **Simplicity** | Queue management required | Just increment/decrement counter |

## Typical Device Polling Order

In a real ISR, you'd poll in priority order or frequency order:

```
1. Highest priority / most critical:
   - Watchdog timer (must be fast)
   - Power failure detector
   
2. High frequency:
   - Keyboard (user input)
   - Serial I/O
   
3. Medium frequency:
   - Line printer
   - Paper tape punch
   
4. Lower priority:
   - Magtape
   - Disk (if present)
```

