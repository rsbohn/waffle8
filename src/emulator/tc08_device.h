#ifndef PDP8_TC08_DEVICE_H
#define PDP8_TC08_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pdp8;
typedef struct pdp8 pdp8_t;
typedef struct pdp8_tc08_device pdp8_tc08_device_t;

#define PDP8_TC08_DEVICE_CODE 076u
#define PDP8_TC08_DEVICE_CODE_ALT 077u
#define PDP8_TC08_IOT_BASE (06000u | ((uint16_t)(PDP8_TC08_DEVICE_CODE & 0x3Fu) << 3))

#define PDP8_TC08_INSTR(bits) (PDP8_TC08_IOT_BASE | (uint16_t)((bits) & 0x3Fu))

// TC08 IOT function codes (octal)
#define TC08_DCR 0x02u    // Device Clear/Reset (device 076)
#define TC08_DTSF 0x04u   // Data Transfer Skip if ready (device 076)
#define TC08_DTLB 0x06u   // Load block number and start (device 076)
#define TC08_DTXA 0x01u   // Load transfer address (device 077)
#define TC08_GO  0x07u    // Start operation (GO) - unused; transfer begins on DTLB in our minimal model

typedef struct {
    uint16_t status;
    uint16_t data;
    uint16_t command;
    // Add more fields as needed for tape position, error, etc.
} tc08_regs_t;

typedef struct {
    char path[256];
    uint16_t *image;
    size_t image_words;
    bool writable;
} tc08_unit_t;

struct pdp8_tc08_device {
    tc08_regs_t regs;
    uint16_t transfer_addr;
    uint16_t block_num;
    uint16_t status; /* bit0 ready, bit1 error */
    tc08_unit_t units[2];
};

pdp8_tc08_device_t *pdp8_tc08_device_create(void);
void pdp8_tc08_device_destroy(pdp8_tc08_device_t *device);
int pdp8_tc08_device_attach(pdp8_t *cpu, pdp8_tc08_device_t *device);

#ifdef __cplusplus
}
#endif

#endif // PDP8_TC08_DEVICE_H
