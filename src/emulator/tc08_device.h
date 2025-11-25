#ifndef PDP8_TC08_DEVICE_H
#define PDP8_TC08_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pdp8;
typedef struct pdp8 pdp8_t;
typedef struct pdp8_tc08_device pdp8_tc08_device_t;

#define PDP8_TC08_DEVICE_CODE 074u
#define PDP8_TC08_IOT_BASE (06000u | ((uint16_t)(PDP8_TC08_DEVICE_CODE & 0x3Fu) << 3))

#define PDP8_TC08_INSTR(bits) (PDP8_TC08_IOT_BASE | (uint16_t)((bits) & 0x3Fu))

// TC08 IOT function codes (octal)
#define TC08_DCR 0x01u  // Device Clear/Reset
#define TC08_DSR 0x02u  // Device Status Register (to AC)
#define TC08_DLR 0x04u  // Data Register Load (from AC)
#define TC08_DSRD 0x05u // Data Register Store (to AC)
#define TC08_SKS 0x06u  // Skip on Status
#define TC08_GO  0x07u  // Start operation (GO)

typedef struct {
	uint16_t status;
	uint16_t data;
	uint16_t command;
	// Add more fields as needed for tape position, error, etc.
} tc08_regs_t;

struct pdp8_tc08_device {
	tc08_regs_t regs;
	// Add more fields for tape image, state, etc.
};

pdp8_tc08_device_t *pdp8_tc08_device_create(void);
void pdp8_tc08_device_destroy(pdp8_tc08_device_t *device);
int pdp8_tc08_device_attach(pdp8_t *cpu, pdp8_tc08_device_t *device);

#ifdef __cplusplus
}
#endif

#endif // PDP8_TC08_DEVICE_H
