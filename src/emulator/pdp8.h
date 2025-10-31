#ifndef PDP8_H
#define PDP8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;

typedef void (*pdp8_iot_handler)(pdp8_t *cpu, uint16_t instruction, void *context);
typedef void (*pdp8_tick_handler)(pdp8_t *cpu, void *context, uint64_t now_ns);

pdp8_t *pdp8_api_create(size_t memory_size);
void pdp8_api_destroy(pdp8_t *cpu);
void pdp8_api_reset(pdp8_t *cpu);
void pdp8_api_set_halt(pdp8_t *cpu);
void pdp8_api_clear_halt(pdp8_t *cpu);
int pdp8_api_step(pdp8_t *cpu);
int pdp8_api_run(pdp8_t *cpu, size_t max_cycles);
uint16_t pdp8_api_get_ac(const pdp8_t *cpu);
void pdp8_api_set_ac(pdp8_t *cpu, uint16_t value);
uint16_t pdp8_api_get_pc(const pdp8_t *cpu);
void pdp8_api_set_pc(pdp8_t *cpu, uint16_t value);
uint8_t pdp8_api_get_link(const pdp8_t *cpu);
void pdp8_api_set_link(pdp8_t *cpu, uint8_t value);
int pdp8_api_write_mem(pdp8_t *cpu, uint16_t address, uint16_t value);
uint16_t pdp8_api_read_mem(const pdp8_t *cpu, uint16_t address);
int pdp8_api_load(pdp8_t *cpu, const uint16_t *words, size_t count, uint16_t start_address);
int pdp8_api_register_iot(pdp8_t *cpu, uint8_t device_code, pdp8_iot_handler handler, void *context);
int pdp8_api_register_tick(pdp8_t *cpu, uint8_t device_code, pdp8_tick_handler handler, void *context);
void pdp8_api_request_skip(pdp8_t *cpu);
void pdp8_api_set_switch_register(pdp8_t *cpu, uint16_t value);
uint16_t pdp8_api_get_switch_register(const pdp8_t *cpu);
int pdp8_api_is_halted(const pdp8_t *cpu);

/* Interrupt support - PDP-8 single interrupt line model
 *
 * The PDP-8 has one hardware interrupt line shared by all devices.
 * When a device wants service, it asserts the line. Multiple devices are
 * handled by re-triggering the ISR after each device is serviced.
 *
 * Interrupt Handler Pattern (octal addresses):
 *
 *   0020 (ISR Entry Point):
 *       ISK 6551        ; Check watchdog (device 55)
 *       JMP TRY2        ; No
 *       JMS HANDLE_WD   ; Yes, service watchdog
 *   TRY2:
 *       ISK 6031        ; Check keyboard (device 03)
 *       JMP DONE        ; No
 *       JMS HANDLE_KBD  ; Yes, service keyboard
 *   DONE:
 *       ION             ; Re-enable interrupts
 *       JMP I 0007      ; Return to interrupted instruction (saved by CPU)
 *
 * Context Save Layout (automatic, done by CPU at interrupt dispatch):
 *   0006: AC (accumulator)
 *   0007: PC (return address = next instruction after interrupt)
 *   0010: LINK
 *
 * When interrupt is pending and interrupts are enabled, CPU will:
 *   1. Save AC, PC, LINK to octal 0006-0010
 *   2. Decrement interrupt_pending counter
 *   3. Disable interrupts (ION state is cleared)
 *   4. Jump to ISR at octal 0020
 *
 * ISR must:
 *   1. Poll all devices to determine which ones need service
 *   2. Service them (clearing their request flags)
 *   3. Re-enable interrupts with ION
 *   4. Return with JMP I 0007
 */

/* Request interrupt: increment pending count.
 * Called by device drivers when they need service.
 * device_code: device identifier (for logging/debugging).
 * Returns 0 on success, -1 if cpu is NULL. */
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code);

/* Peek at interrupt pending count without modifying it.
 * Returns number of pending interrupts (0 = none), or -1 if cpu is NULL. */
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);

/* Decrement interrupt pending count (called after ISR services a device).
 * Returns 0 if successfully decremented, -1 if cpu is NULL or count was 0. */
int pdp8_api_clear_interrupt_pending(pdp8_t *cpu);

/* Query interrupt enable state.
 * Returns 1 if interrupts are enabled (ION executed), 0 if disabled (IOFF or reset),
 * or -1 if cpu is NULL. */
int pdp8_api_is_interrupt_enabled(const pdp8_t *cpu);

/* Set interrupt enable state (called by ION/IOFF instructions).
 * enable: 1 to enable interrupts, 0 to disable.
 * Returns 0 on success, -1 if cpu is NULL. */
int pdp8_api_set_interrupt_enable(pdp8_t *cpu, int enable);

#ifdef __cplusplus
}
#endif

#endif
