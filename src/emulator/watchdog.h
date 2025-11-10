#ifndef PDP8_WATCHDOG_H
#define PDP8_WATCHDOG_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PDP8_WATCHDOG_DEVICE_CODE 055u
#define PDP8_WATCHDOG_IOT_BASE (06000u | ((PDP8_WATCHDOG_DEVICE_CODE & 0x3Fu) << 3))

/* Discrete IOT function codes (microcode field) */
#define PDP8_WATCHDOG_FUNC_NOP     0x0u /* 6550 - no operation */
#define PDP8_WATCHDOG_FUNC_ISK     0x1u /* 6551 - Interrupt Skip if expired flag set */
#define PDP8_WATCHDOG_FUNC_WRITE   0x2u /* 6552 - write control register from AC */
#define PDP8_WATCHDOG_FUNC_READ    0x3u /* 6553 - read control register into AC */
#define PDP8_WATCHDOG_FUNC_RESTART 0x4u /* 6554 - restart counter */

/* Instruction constructors */
#define PDP8_WATCHDOG_INSTR(func) (PDP8_WATCHDOG_IOT_BASE | (uint16_t)((func) & 0x7u))
#define PDP8_WATCHDOG_ISK  PDP8_WATCHDOG_INSTR(PDP8_WATCHDOG_FUNC_ISK)     /* 6551 */
#define PDP8_WATCHDOG_WRITE PDP8_WATCHDOG_INSTR(PDP8_WATCHDOG_FUNC_WRITE)  /* 6552 */
#define PDP8_WATCHDOG_READ  PDP8_WATCHDOG_INSTR(PDP8_WATCHDOG_FUNC_READ)   /* 6553 */
#define PDP8_WATCHDOG_RESTART PDP8_WATCHDOG_INSTR(PDP8_WATCHDOG_FUNC_RESTART) /* 6554 */

/* Command encodings for control register (3-bit field) */
#define PDP8_WD_CMD_DISABLE 0
#define PDP8_WD_CMD_RESET_ONE_SHOT 1
#define PDP8_WD_CMD_RESET_PERIODIC 2
#define PDP8_WD_CMD_HALT_ONE_SHOT 3
#define PDP8_WD_CMD_HALT_PERIODIC 4
#define PDP8_WD_CMD_INTERRUPT_ONE_SHOT 5
#define PDP8_WD_CMD_INTERRUPT_PERIODIC 6
#define PDP8_WD_CMD_TICK_PERIODIC 7

typedef struct pdp8_watchdog pdp8_watchdog_t;
typedef struct pdp8 pdp8_t;

struct pdp8_watchdog_status {
	int enabled;           /* non-zero if counting */
	int expired;           /* non-zero if last expiry fired */
	int cmd;               /* raw command value (0..7) */
	int configured_count;  /* countdown in deciseconds (0..511) */
	int remaining_ds;      /* remaining time in deciseconds, -1 if not running */
};

/* Retrieve runtime status for a watchdog instance. Returns 0 on success. */
int pdp8_watchdog_get_status(const pdp8_watchdog_t *wd, struct pdp8_watchdog_status *out_status);

pdp8_watchdog_t *pdp8_watchdog_create(void);
void pdp8_watchdog_destroy(pdp8_watchdog_t *wd);
int pdp8_watchdog_attach(pdp8_t *cpu, pdp8_watchdog_t *wd);

#ifdef __cplusplus
}
#endif

#endif
