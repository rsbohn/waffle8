# Why an Interrupt Queue? Design Rationale

## The Problem: Asynchronous Device Events

The core issue is **timing mismatch** between when devices want to signal the CPU and when the CPU is ready to handle interrupts.

### Real-World Scenario

Imagine the watchdog timer:

```
Timeline:
─────────────────────────────────────────────────────────────
Time 0.0s: User code disables interrupts (IOFF)
           cpu->interrupt_enable = false
           
Time 0.1s: Watchdog timer expires!
           Device wants to signal interrupt
           But cpu->interrupt_enable is false...
           
           Without queue: Interrupt is LOST ❌
           
Time 0.2s: User code enables interrupts (ION)
           cpu->interrupt_enable = true
           
Time 0.3s: Next instruction executes
           ← When should watchdog interrupt happen?
           
Time 0.4s: Another device expiry occurs
           Another interrupt lost? ❌
```

## Solution 1: Reject the Interrupt (Simple but Broken)

```c
// Naive approach - DOESN'T WORK
if (cpu->interrupt_enable) {
    // Deliver interrupt immediately
    dispatch_interrupt();
} else {
    // Ignore it - TOO DANGEROUS
    return -1;  // "Sorry, can't accept interrupt now"
}
```

**Problems**:
- Device loses the event forever
- Critical signals (watchdog, I/O completion) disappear
- Software can't reliably communicate with hardware
- Real PDP-8 hardware DOESN'T work this way

## Solution 2: The Interrupt Queue (Correct)

```c
// Queue-based approach - CORRECT
int pdp8_api_request_interrupt(cpu, device_code, trap_vector) {
    // Always accept the request
    enqueue_interrupt(device_code, trap_vector);
    return 0;  // Success - will deliver when interrupts enabled
}
```

**Why this is better**:
- ✅ Devices never lose interrupts
- ✅ Queue acts as a "pending mailbox"
- ✅ Interrupts deliver reliably when ION is executed
- ✅ Matches real PDP-8 hardware behavior
- ✅ Software controls interrupt timing with ION/IOFF

## How the Queue Works

### Example: Watchdog expiry during critical section

```asm
START,      CLA CLL
            IOFF            ; DISABLE interrupts
            
CRITICAL,   / ... critical section (no interrupts allowed) ...
            DCA 0040        ; Update state safely
            DCA 0041        ; (no device can interrupt here)
            DCA 0042
            
            ION             ; ENABLE interrupts
            
            / At this point, the CPU checks:
            / "Hey, are there any pending interrupts?"
            / If watchdog expired during CRITICAL,
            / it was queued and now gets delivered!
            
            HLT
```

### Queue Timeline

```
State 1: IOFF (interrupt_enable = false)
         interrupt_queue = [ ]  (empty)

Event: Watchdog expires
       pdp8_api_request_interrupt(55, 0) called
       
State 2: IOFF (interrupt_enable = false)
         interrupt_queue = [WATCHDOG_EVENT]  (queued!)

State 3: User executes ION
         interrupt_enable = true
         
State 4: Next instruction ends
         pdp8_api_step() checks queue:
         "interrupt_enable is true, queue has entry!"
         
         → DISPATCH INTERRUPT NOW
         → Save context, jump to ISR
         
         interrupt_queue = [ ]  (empty again)
```

## Comparison: With vs Without Queue

### WITHOUT Queue (Broken)

```
Device:         ┌─────────────────────┐
                │ Timer fires every   │
                │ 0.1 seconds         │
                └─────────────────────┘
                      ↓
CPU:     ┌──────────────────────────────────────┐
         │ if (interrupt_enable) {              │
         │   dispatch_interrupt()               │
         │ } else {                             │
         │   // Interrupt is LOST ❌            │
         │   return -1                          │
         │ }                                    │
         └──────────────────────────────────────┘

Problem: If IOFF during timer event → event vanishes
         Watchdog can't reliably interrupt for halt/reset
```

### WITH Queue (Correct)

```
Device:         ┌─────────────────────┐
                │ Timer fires every   │
                │ 0.1 seconds         │
                └─────────────────────┘
                      ↓
CPU Queue:      ┌─────────────────────────────┐
                │ [ Event ]                   │
                │ [ Event ]                   │
                │ [ Event ]                   │
                │   (pending mailbox)         │
                └─────────────────────────────┘
                      ↓
CPU at ION:     ┌──────────────────────────────────────┐
                │ if (interrupt_enable &&              │
                │     queue not empty) {               │
                │   dispatch_queued_interrupt()        │
                │ }                                    │
                │                                      │
                │ ALL queued interrupts GUARANTEED     │
                │ to be delivered ✅                   │
                └──────────────────────────────────────┘
```

## Real PDP-8 Hardware Behavior

On real PDP-8 systems, interrupt requests work similarly:

1. **Device hardware asserts interrupt line** (sets voltage)
2. **CPU checks at end of instruction** if interrupts are enabled
3. **If disabled**: CPU ignores it, but device keeps signal asserted
4. **If enabled**: CPU services the interrupt
5. **Result**: Interrupts are never lost; they wait for permission

Our queue **simulates** this persistent signal with a software structure.

## Multiple Interrupts: Why the Queue Can Hold More Than One

```
Scenario: Multiple devices expire during critical section

Time 0.0s: IOFF
           interrupt_queue = [ ]

Time 0.1s: Watchdog expires
           queue = [WATCHDOG]

Time 0.2s: Line printer finishes character
           queue = [WATCHDOG, LINEPRINTER]

Time 0.3s: Magtape read completes
           queue = [WATCHDOG, LINEPRINTER, MAGTAPE]

Time 0.4s: ION - Enable interrupts
           
Time 0.5s: End of instruction - Check queue
           Dequeue WATCHDOG → dispatch → save context
           
Time 0.6s: ISR executes ION again
           At end of that instruction: queue still has 2 more!
           Dequeue LINEPRINTER → dispatch
           
Time 0.7s: Another ISR runs
           Dequeue MAGTAPE → dispatch
```

**Without a queue**, you'd lose LINEPRINTER and MAGTAPE events!

## The Queue Size

We chose **64 entries** because:

1. **PDP-8 has 64 device codes (0-63)**
   - Worst case: one interrupt from each device
   - 64 queue slots = sufficient capacity

2. **Reasonable memory footprint**
   - 64 × 2 bytes = 128 bytes (negligible)
   - Much smaller than main memory (4K words = 8KB)

3. **Simple wraparound arithmetic**
   - `size % 64` = just check 6 bits
   - Efficient circular buffer

4. **No dynamic allocation**
   - Fixed array = no malloc/free
   - Predictable performance
   - No memory fragmentation

## Why Not Other Approaches?

### ❌ Approach 1: "Just deliver immediately"
**Problem**: Ignores IOFF - breaks software that assumes IOFF protects critical sections

### ❌ Approach 2: "Keep one interrupt pending"
**Problem**: Multiple devices can expire during critical section - you lose events

### ❌ Approach 3: "Dedicated interrupt per device"
**Problem**: Requires 64 separate flags - same memory use as queue, but less flexible

### ❌ Approach 4: "Priority queue with level masking"
**Problem**: Over-engineered for this use case; real PDP-8 had 4 levels but modern systems rarely need it

### ✅ Approach 5: "Simple FIFO queue" (What we chose)
**Advantages**:
- Handles multiple simultaneous device interrupts
- Preserves device events reliably
- Matches real hardware semantics
- Simple implementation
- Minimal memory overhead
- Works with existing ION/IOFF control model

## Connection to Existing "skip_pending" Mechanism

The CPU already has a similar concept:

```c
struct pdp8 {
    bool skip_pending;  // Delay skip for next instruction
};
```

The ISZ instruction can set `skip_pending = true`, and at the end of the instruction, `apply_skip()` processes it. 

**Interrupt queue extends this pattern**:
- `skip_pending`: Single boolean flag (next instruction skips)
- `interrupt_queue`: Multiple entries (multiple interrupts queue)
- Both use same principle: **defer processing to end of instruction**

## Summary

The interrupt queue is essential because:

1. **Devices are asynchronous** - they can signal interrupt at any time
2. **Software controls interrupt timing** - IOFF/ION defer delivery
3. **Events must not be lost** - queue buffers them
4. **Multiple devices exist** - need to queue multiple events
5. **Real hardware works this way** - we're emulating actual PDP-8 behavior

**Without it**: Interrupts would be unreliable and unpredictable ❌

**With it**: Interrupts behave like real hardware - predictable, reliable, and controllable ✅

