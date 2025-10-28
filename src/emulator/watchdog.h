#ifndef PDP8_WATCHDOG_H
#define PDP8_WATCHDOG_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PDP8_WATCHDOG_DEVICE_CODE 055u
#define PDP8_WATCHDOG_IOT_BASE (06000u | ((PDP8_WATCHDOG_DEVICE_CODE & 0x3Fu) << 3))
#define PDP8_WATCHDOG_BIT_WRITE 0x1u /* write control register */
#define PDP8_WATCHDOG_BIT_READ 0x2u  /* read control register */
#define PDP8_WATCHDOG_BIT_RESTART 0x4u /* restart counter */
#define PDP8_WATCHDOG_INSTR(bits) (PDP8_WATCHDOG_IOT_BASE | (uint16_t)((bits) & 0x7u))

/* Command encodings for control register (3-bit field) */
#define PDP8_WD_CMD_DISABLE 0
#define PDP8_WD_CMD_RESET_ONE_SHOT 1
#define PDP8_WD_CMD_RESET_PERIODIC 2
#define PDP8_WD_CMD_HALT_ONE_SHOT 3
#define PDP8_WD_CMD_HALT_PERIODIC 4

typedef struct pdp8_watchdog pdp8_watchdog_t;
typedef struct pdp8 pdp8_t;

pdp8_watchdog_t *pdp8_watchdog_create(void);
void pdp8_watchdog_destroy(pdp8_watchdog_t *wd);
int pdp8_watchdog_attach(pdp8_t *cpu, pdp8_watchdog_t *wd);

#ifdef __cplusplus
}
#endif

#endif
