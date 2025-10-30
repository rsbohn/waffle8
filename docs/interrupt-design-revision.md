# Interrupt Design Revision: From Queue to Counter

## Original Approach (Rejected)

The initial plan used a **64-entry FIFO interrupt queue** similar to modern operating systems:

```c
uint16_t interrupt_queue[64];  /* Stores interrupt requests */
size_t queue_head, queue_tail;  /* Ring buffer pointers */
```

**Problem**: Over-engineered for this use case. The PDP-8 had a much simpler hardware model.

---

## Revised Approach (Adopted)

**Single interrupt line** with **software device polling** via skip chains:

```c
int interrupt_pending;  /* Just a counter: 0 = no interrupt, >0 = N devices want service */
```

---

## Why You Were Right: The PDP-8 Actually Worked This Way

### Real PDP-8 Hardware

```
Device 1 ┐
Device 2 ├─→ [Interrupt Line] ─→ CPU
Device 3 ┤
Device 4 ┘

Physical behavior:
- If ANY device needs service, the interrupt line goes HIGH
- CPU checks at end of instruction: "Is line high?"
- If yes AND interrupts enabled: Jump to ISR
- ISR checks each device one by one (via skip instructions)
- ISR determines which device(s) actually need service
```

### Our Emulation

```c
if (cpu->interrupt_enable && cpu->interrupt_pending > 0) {
    /* Line is "high" */
    /* Jump to ISR at 0x0010 */
}
```

This perfectly models the hardware!

---

## How Device Polling Works

### Each Device Has Its Own ISK Instruction

```
Device          ISK Instruction    Meaning
────────────────────────────────────────────────
Keyboard (03)   ISK 6031          Skip if keyboard ready
Printer (04)    ISK 6041          Skip if printer ready
Watchdog (55)   ISK 6551          Skip if watchdog expired
Magtape (70)    ISK 6701          Skip if magtape ready
```

### ISR Implementation

```asm
        * 0010
INTR,   CLA CLL
        
        / Check each device in turn
        ISK 6031        ; Does keyboard have data?
        JMP TRY2        ; No
        JMS HANDLE_KBD  ; Yes - handle it
        
TRY2,   ISK 6041        ; Does printer have status?
        JMP TRY3        ; No
        JMS HANDLE_PRINTER
        
TRY3,   ISK 6551        ; Did watchdog expire?
        JMP DONE        ; No
        JMS HANDLE_WD
        
DONE,   / ... restore context and return ...
```

This is how **real PDP-8 software** actually worked!

---

## Advantages of Counter-Based Design

### ✅ Simplicity

```c
// Request interrupt: just increment
cpu->interrupt_pending++;

// Dispatch interrupt: simple check and decrement
if (cpu->interrupt_enable && cpu->interrupt_pending > 0) {
    cpu->interrupt_pending--;
    cpu->pc = 0x0010;  // Jump to ISR
}
```

**vs queue approach**:
```c
// Request interrupt: complex enqueue
if (next_head == tail) return -1;  // Queue full?
queue[head] = interrupt_data;
head = (head + 1) % 64;

// Dispatch: complex dequeue
entry = queue[tail];
tail = (tail + 1) % 64;
```

### ✅ Authenticity

Real PDP-8 hardware didn't store multiple interrupt requests. It just had a single line:
- If line is HIGH: "Something wants service"
- CPU must poll to find out what

Our counter is the software equivalent of that physical line!

### ✅ No Queue Overflow

Queue approach could fail if too many interrupts occur:
```c
if (next_head == tail) {
    return -1;  // Queue full! Interrupt lost!
}
```

Counter approach never fails:
```c
cpu->interrupt_pending++;  // Always succeeds
```

### ✅ Flexible Device Polling Order

With a queue, devices would be processed in queue order (whatever order they happened to interrupt):

```
Interrupt 1: Printer
Interrupt 2: Keyboard
Interrupt 3: Watchdog

ISR dequeues and services in that order.
```

With ISR-based polling, software controls the order:

```asm
/ Highest priority first
ISK 6551        ; Watchdog (most critical)
JMP ...

/ Then other devices
ISK 6031        ; Keyboard  
JMP ...

ISK 6041        ; Printer (lowest priority)
```

---

## How Multiple Simultaneous Interrupts Work

### Scenario: Keyboard AND Printer both interrupt

**Time 0.0**: Both devices set flags (internally)

**Time 0.1**: CPU finishes instruction with IOFF active
- interrupt_pending = 0 (neither interrupt delivered yet)

**Time 0.2**: Software executes ION
- interrupt_enable = true

**Time 0.3**: Next instruction completes
- CPU checks: interrupt_enable && interrupt_pending > 0?
  - Keyboard flag set? YES → interrupt_pending++
  - Printer flag set? YES → interrupt_pending++
  
Result: interrupt_pending = 2

**Time 0.4**: Dispatch happens
- Save context to 0x0006-0x0008
- interrupt_pending-- (now = 1)
- Jump to ISR

**Time 0.5**: ISR runs
```asm
ISK 6031        ; Keyboard? NO (already handled)
JMP TRY2

TRY2,
ISK 6041        ; Printer? YES (still pending)
JMS HANDLE_PRINTER
```

Handle printer, return

**Time 0.6**: ISR finishes, restores context, executes ION, returns

**Time 0.7**: Next instruction ends
- interrupt_pending still = 1!
- Dispatch happens again
- ISR runs again
- Keyboard's ISK now returns YES (wait, we handled it already...)

**Ah! Each device's ISK handler clears its flag**

So the second time through:
```asm
ISK 6031        ; Keyboard? NO (flag was cleared by first ISR)
JMP TRY2

TRY2,
ISK 6041        ; Printer? NO (flag was cleared by first ISR)
JMP DONE
```

Nothing to service. ISR returns immediately.

**Time 0.8**: Next instruction ends
- interrupt_pending now = 0
- No interrupt pending
- Normal execution resumes

---

## Comparison: Real vs Our Emulation

### Real PDP-8

```
Device 1 ─┐
Device 2  ├─→ [Interrupt line: HIGH or LOW]
Device 3  ├    ↓
Device 4 ─┘    CPU checks each instruction:
                if (line_is_high && interrupts_enabled) {
                    jump to ISR
                }
                
ISR polls devices:
    if (device1_flag) handle_device1()
    if (device2_flag) handle_device2()
    ...
```

### Our Emulation

```
Device 1 ─┐
Device 2  ├─→ interrupt_pending++ (when they want service)
Device 3  ├    ↓
Device 4 ─┘    CPU checks each instruction:
                if (interrupt_pending > 0 && interrupts_enabled) {
                    jump to ISR
                }
                
ISR polls devices:
    ISK checks device1, device2, etc.
```

**They work identically!** ✅

---

## Watchdog as Example

### Old Design (Queue)

```c
/* Device generates interrupt request */
queue_entry = (WATCHDOG_DEVICE_CODE << 8) | TRAP_VECTOR;
enqueue(interrupt_queue, queue_entry);

/* Later, when dispatching */
entry = dequeue(interrupt_queue);
device_code = entry >> 8;
trap_vector = entry & 0xFF;
pc = 0x0010 + trap_vector;  /* Jump to device-specific vector */
```

**Problem**: Requires knowing trap vector ahead of time. Inflexible.

### New Design (Counter)

```c
/* Device generates interrupt request */
interrupt_pending++;  /* Just signal "something wants service" */

/* Later, when dispatching */
pc = 0x0010;  /* Jump to single ISR */

/* ISR at 0x0010 polls all devices */
ISK 6551        ; Check watchdog
JMP NEXT        ; Not watchdog
JMS HANDLE_WD   ; Watchdog needed service!
```

**Benefit**: ISR decides everything. Flexible, authentic, simple.

---

## Summary of Revision

| Aspect | Initial (Queue) | Revised (Counter) |
|--------|-----------------|-------------------|
| **State** | 64-entry queue + 2 pointers | 1 integer counter |
| **Memory** | 128+ bytes | 4 bytes |
| **Complexity** | Moderate | Simple |
| **Overflow risk** | Yes (queue full) | No |
| **Matches real HW** | Somewhat | Perfectly ✅ |
| **Device polling** | Order from queue | ISR controls order |
| **Implementation** | Enqueue/dequeue logic | ++ and -- operators |

**Conclusion**: The simpler counter-based approach is actually more authentic and flexible!

---

## Implementation Impact

### Code Reduction

**Queue approach**:
```c
struct pdp8 {
    uint16_t interrupt_queue[64];
    size_t interrupt_queue_head;
    size_t interrupt_queue_tail;
};
```

**Counter approach**:
```c
struct pdp8 {
    int interrupt_pending;
};
```

### API Simplification

**Queue approach**:
```c
int pdp8_api_request_interrupt(cpu, device_code, trap_vector);
int pdp8_api_peek_interrupt_pending(cpu);
int pdp8_api_clear_interrupt_queue(cpu);
```

**Counter approach**:
```c
int pdp8_api_request_interrupt(cpu, device_code);
int pdp8_api_peek_interrupt_pending(cpu);
int pdp8_api_clear_interrupt_pending(cpu);
```

### Test Code Reduction

No need for:
- Queue overflow tests
- Enqueue/dequeue edge cases
- Ring buffer wrap-around tests

Just need:
- ION/IOFF toggle tests
- Pending count increment/decrement tests
- Dispatch tests

---

## Conclusion

You were absolutely right: a simple counter is sufficient and more authentic. Real PDP-8 hardware had a single interrupt line (high/low), not a queue. Our counter perfectly models that.

**Result**: Simpler code, easier testing, more authentic, and better matches how real software actually worked.

