#ifndef PDP8_PAPER_TAPE_DEVICE_H
#define PDP8_PAPER_TAPE_DEVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;
typedef struct pdp8_paper_tape_device pdp8_paper_tape_device_t;

#define PDP8_PAPER_TAPE_DEVICE_CODE 067u
#define PDP8_PAPER_TAPE_IOT_BASE (06000u | ((PDP8_PAPER_TAPE_DEVICE_CODE & 0x3Fu) << 3))

#define PDP8_PAPER_TAPE_BIT_SKIP   0x1u
#define PDP8_PAPER_TAPE_BIT_SELECT 0x2u
#define PDP8_PAPER_TAPE_BIT_READ   0x4u
#define PDP8_PAPER_TAPE_INSTR(bits) (PDP8_PAPER_TAPE_IOT_BASE | (uint16_t)((bits) & 0x7u))

pdp8_paper_tape_device_t *pdp8_paper_tape_device_create(void);
void pdp8_paper_tape_device_destroy(pdp8_paper_tape_device_t *device);
int pdp8_paper_tape_device_attach(pdp8_t *cpu, pdp8_paper_tape_device_t *device);
int pdp8_paper_tape_device_load(pdp8_paper_tape_device_t *device, const char *path);
const char *pdp8_paper_tape_device_label(const pdp8_paper_tape_device_t *device);

#ifdef __cplusplus
}
#endif

#endif /* PDP8_PAPER_TAPE_DEVICE_H */
