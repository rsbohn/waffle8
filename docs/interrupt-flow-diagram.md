# Interrupt Flow Diagrams

## 1. Interrupt Request and Queue Flow

```
Device (e.g., Watchdog)
    │
    ├─ Expiry occurs
    │
    └─► pdp8_api_request_interrupt(cpu, device_code, trap_vector)
            │
            ├─ Check queue not full
            │
            └─► Enqueue entry: (device_code << 8) | trap_vector
                    │
                    └─► interrupt_queue[interrupt_queue_head]
                            │
                            └─► Increment head pointer (mod 64)


Interrupt Queue (FIFO)
┌───────────────────────────────────┐
│ interrupt_queue[0..63]            │
│                                   │
│  head──▶ [empty]      [entry]     │
│         [entry]       [entry]     │
│         [entry] ◀──tail           │
│         [empty]       [empty]     │
└───────────────────────────────────┘
         Enqueue end    Dequeue end
```

## 2. Instruction Execution and Interrupt Dispatch

```
pdp8_api_step()
    │
    ├─ Fetch instruction from memory[PC]
    │
    ├─ Increment PC
    │
    ├─ Decode opcode (memory reference / IOT / operate)
    │
    ├─ Execute instruction
    │   ├─ (Could be ION/IOFF affecting interrupt_enable flag)
    │   └─ (Could modify AC, PC, memory, etc.)
    │
    ├─ apply_skip()
    │   └─ If skip pending: PC += 1
    │
    ├─ ┌─────────────────────────────────────────────────────┐
    │  │ === NEW: INTERRUPT SERVICE DISPATCH ===             │
    │  │                                                     │
    │  │ If (interrupt_enable && queue not empty):          │
    │  │   ├─ Dequeue entry from interrupt_queue[tail]      │
    │  │   │   └─ Extract device_code and trap_vector       │
    │  │   │                                                 │
    │  │   ├─ Save context:                                 │
    │  │   │   ├─ memory[0x0006] = AC                       │
    │  │   │   ├─ memory[0x0007] = PC                       │
    │  │   │   └─ memory[0x0008] = LINK                     │
    │  │   │                                                 │
    │  │   ├─ Disable interrupts: interrupt_enable = false  │
    │  │   │                                                 │
    │  │   └─ Jump to trap vector:                          │
    │  │       PC = 0x0010 + trap_vector                    │
    │  │                                                     │
    │  └─────────────────────────────────────────────────────┘
    │
    ├─ Call tick handlers (for watchdog, timers, etc.)
    │
    └─ Return 1 (success)
```

## 3. ION/IOFF Instruction Execution

```
Operate Group 2 Instruction (opcode 0x0E00)
    │
    ├─ Bit fields in 12-bit instruction:
    │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
    │  │ 11  │ 10  │  9  │  8  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │
    │  ├─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
    │  │  0  │  1  │  1  │  1  │  -  │CLA  │ SMA │ SZA │ SNL │OSR  │HLT  │ION  │
    │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
    │      opcode 0x0E        Group 2 operates
    │
    ├─ CLA (Clear AC)   - Bit 6
    ├─ SMA (Skip if Minus) - Bit 5
    ├─ SZA (Skip if Zero) - Bit 4
    ├─ SNL (Skip if Link) - Bit 3
    ├─ OSR (Or Switch Register) - Bit 2
    ├─ HLT (Halt) - Bit 1
    │
    └─ ION (Interrupt ON) - Bit 0  ◀────────── NEW
        │
        └─ execute_operate() → operate_group2()
            │
            ├─ Check: if (instruction & 0x0001)
            │       └─ cpu->interrupt_enable = true
            │
            └─ IOFF is implicit (not setting bit 0)


Examples:
    ION:   0o6001 = 0x0E01  (Group 2 + ION bit)
    IOFF:  0o6000 = 0x0E00  (Group 2, no ION bit)
    HLTX:  0o6002 = 0x0E02  (HLT without ION)
```

## 4. Interrupt Handler Assembly Pattern

```asm
        * 0000
RESET,  JMP START           ; Reset vector

        * 0010
INTR_HDL,                   ; Interrupt entry point
        CLA CLL             ; Clear everything
        
        / At this point, interrupted context is:
        /   AC in address 0x0006
        /   PC in address 0x0007  (points to next instruction after interrupt)
        /   LINK in address 0x0008
        
        / ... handle interrupt ...
        / (increment counter, set flags, etc.)
        
        / Restore context and return
        CLA
        TAD 0006            ; Restore AC
        DCA /TEMP           ; Save if needed
        TAD 0007            ; Restore PC
        DCA 0011            ; Setup return address
        
        ION                 ; Re-enable interrupts
        JMP I 0011          ; Return to interrupted code


        * 0200
START,  CLA
        ION                 ; Enable interrupts
        TAD 0040
        DCA 0040
        JMS WAIT            ; Wait for interrupt
        HLT
```

## 5. Watchdog Interrupt Mode Sequence

```
Watchdog Device Timer
    │
    ├─ Countdown in deciseconds (0.1s units)
    ├─ When remaining_ds reaches 0:
    │
    └─► Check watchdog_cmd:
        │
        ├─ WD_CMD_HALT_* 
        │   └─► pdp8_api_set_halt(cpu)
        │
        ├─ WD_CMD_RESET_*
        │   └─► pdp8_api_reset(cpu)
        │
        └─ WD_CMD_INTERRUPT_ONE_SHOT
        │   └─► pdp8_api_request_interrupt(cpu, 55, 0)
        │           │
        │           └─► [Queued - will dispatch when ION executed]
        │
        └─ WD_CMD_INTERRUPT_PERIODIC
            └─► pdp8_api_request_interrupt(cpu, 55, 0)
                │
                └─► [Queued - will dispatch when ION executed]
                └─► [Countdown restarts if periodic]


Watchdog Control Register (AC value for write):
┌─────────────┬──────────────────────────────┐
│ Bits 11-9   │ Bits 8-0                     │
├─────────────┼──────────────────────────────┤
│ CMD (3 bits)│ COUNT (9 bits, deciseconds) │
└─────────────┴──────────────────────────────┘
     3        2         1      0

CMD values:
    0 = DISABLE
    1 = RESET (one-shot)
    2 = RESET (periodic)
    3 = HALT (one-shot)
    4 = HALT (periodic)
    5 = INTERRUPT (one-shot)    ◀────── NEW
    6 = INTERRUPT (periodic)    ◀────── NEW
    7 = Reserved
```

## 6. Monitor Configuration Flow

```
pdp8.config (configuration file)
    │
    ├─ [watchdog]
    │  ├─ enabled = 1
    │  ├─ mode = "interrupt"        ◀────── NEW
    │  ├─ periodic = 1
    │  └─ default_count = 50
    │
    └─► monitor_platform_posix.c: load_device_config()
            │
            ├─ Parse: watchdog.mode = "interrupt"
            │
            └─► struct monitor_config:
                    watchdog_mode = "interrupt"
                    
                        │
                        └─► monitor.c: initialize_watchdog()
                                │
                                ├─ Create watchdog device
                                │
                                └─ If mode == "interrupt":
                                    └─ Watchdog will call:
                                        pdp8_api_request_interrupt()
                                        on expiry
```

## 7. State Machine: Interrupt Handling

```
                    ┌────────────────────────────┐
                    │  INTERRUPTS DISABLED       │
                    │  (interrupt_enable = false)│
                    │  Queue accepts requests    │
                    └──────────────┬─────────────┘
                                   │
                    Device requests interrupt
                                   │
                                   ▼
            ┌──────────────────────────────────────┐
            │ Entry enqueued in interrupt_queue[]  │
            │ (blocking if queue full)             │
            └──────────────────────┬───────────────┘
                                   │
                  User code executes ION
                                   │
                                   ▼
                    ┌────────────────────────────┐
                    │  INTERRUPTS ENABLED        │
                    │  (interrupt_enable = true) │
                    │                            │
                    │ At end of instruction:     │
                    │  - Check queue            │
                    │  - If entry present:      │
                    │    - Save context         │
                    │    - Set interrupt_enable │
                    │      = false              │
                    │    - Jump to ISR          │
                    └──────────────┬─────────────┘
                                   │
                                   ▼
                    ┌────────────────────────────┐
                    │  IN INTERRUPT SERVICE      │
                    │  (interrupt_enable = false)│
                    │                            │
                    │ - Devices can request more │
                    │   interrupts (enqueued)    │
                    │ - ISR can dispatch device  │
                    │   and re-enable ION       │
                    └──────────────┬─────────────┘
                                   │
                  ISR executes ION to re-enable
                                   │
                                   ▼
                    ┌────────────────────────────┐
                    │  INTERRUPTS ENABLED AGAIN  │
                    │  - Return from ISR         │
                    │  - Next instruction checks │
                    │    queue for more entries  │
                    │  - If present: dispatch    │
                    │  - If not: continue       │
                    └────────────────────────────┘
```

## 8. Context Save Layout

```
Fixed memory locations for interrupt context:
┌──────────────────────────────────────────┐
│ Address │ Content                        │
├──────────────────────────────────────────┤
│ 0x0006  │ Interrupted AC (12-bit value) │
│ 0x0007  │ Interrupted PC (12-bit addr)  │
│ 0x0008  │ Interrupted LINK (1-bit)      │
│ 0x0009  │ (available for ISR use)       │
│ 0x000A  │ (available for ISR use)       │
└──────────────────────────────────────────┘

Trap Vector Base: 0x0010 + offset
┌──────────────────────────────────────────┐
│ Address │ Purpose                        │
├──────────────────────────────────────────┤
│ 0x0010  │ Interrupt vector 0 (device 0) │
│ 0x0011  │ Interrupt vector 1 (device 1) │
│ 0x0012  │ Interrupt vector 2 (device 2) │
│   ...   │                                │
│ 0x0017  │ Interrupt vector 7 (device 7) │
└──────────────────────────────────────────┘

Typical ISR returns via:
    TAD 0007        ; Get saved PC (resume address)
    DCA 0011        ; Put in temp location
    ION             ; Re-enable interrupts
    JMP I 0011      ; Jump to resume address
```

