#ifndef PDP8_INTERRUPT_CONTROL_H
#define PDP8_INTERRUPT_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;

/* Device 00: Interrupt Control (standard PDP-8 IOT-based interrupt management)
 *
 * This device handles the standard interrupt enable/disable/skip instructions:
 *   6000 octal = IOFF (Interrupt OFF) - disable interrupts
 *   6001 octal = ION  (Interrupt ON)  - enable interrupts
 *   6002 octal = SKON (Skip if Interrupt ON) - skip next instruction if ION enabled
 *
 * These are in addition to Group 2 Operate (7400/7401) which also support ION/IOFF.
 * Both methods control the same interrupt_enable flag in the CPU.
 */

#define PDP8_INTERRUPT_CONTROL_DEVICE_CODE 000u
#define PDP8_INTERRUPT_CONTROL_IOT_BASE (06000u | ((PDP8_INTERRUPT_CONTROL_DEVICE_CODE & 0x3Fu) << 3))

/* IOT function encodings */
#define PDP8_INTERRUPT_CONTROL_FUNC_NOP   0x0u  /* 6000 - no operation (or IOFF) */
#define PDP8_INTERRUPT_CONTROL_FUNC_ION   0x1u  /* 6001 - Interrupt ON */
#define PDP8_INTERRUPT_CONTROL_FUNC_SKON  0x2u  /* 6002 - Skip if Interrupt ON */

/* Instruction constructors */
#define PDP8_INTERRUPT_CONTROL_IOFF  (PDP8_INTERRUPT_CONTROL_IOT_BASE | 0x0u)  /* 6000 */
#define PDP8_INTERRUPT_CONTROL_ION   (PDP8_INTERRUPT_CONTROL_IOT_BASE | 0x1u)  /* 6001 */
#define PDP8_INTERRUPT_CONTROL_SKON  (PDP8_INTERRUPT_CONTROL_IOT_BASE | 0x2u)  /* 6002 */

/* Attach interrupt control device to CPU. Returns 0 on success, -1 on error. */
int pdp8_interrupt_control_attach(pdp8_t *cpu);

#ifdef __cplusplus
}
#endif

#endif
