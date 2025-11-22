#ifndef PDP8_PAPER_TAPE_PUNCH_H
#define PDP8_PAPER_TAPE_PUNCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;
typedef struct pdp8_paper_tape_punch pdp8_paper_tape_punch_t;
typedef void (*pdp8_paper_tape_punch_output_callback)(uint8_t ch, void *context);

#define PDP8_PAPER_TAPE_PUNCH_DEVICE_CODE 002u
#define PDP8_PAPER_TAPE_PUNCH_IOT_BASE \
    (06000u | ((PDP8_PAPER_TAPE_PUNCH_DEVICE_CODE & 0x3Fu) << 3))

#define PDP8_PAPER_TAPE_PUNCH_BIT_SKIP 0x1u
#define PDP8_PAPER_TAPE_PUNCH_BIT_CLEAR 0x2u
#define PDP8_PAPER_TAPE_PUNCH_BIT_PUNCH 0x4u

#define PDP8_PAPER_TAPE_PUNCH_INSTR(bits) \
    (PDP8_PAPER_TAPE_PUNCH_IOT_BASE | (uint16_t)((bits) & 0x7u))

#define PDP8_PAPER_TAPE_PUNCH_PSF \
    PDP8_PAPER_TAPE_PUNCH_INSTR(PDP8_PAPER_TAPE_PUNCH_BIT_SKIP)
#define PDP8_PAPER_TAPE_PUNCH_PCF \
    PDP8_PAPER_TAPE_PUNCH_INSTR(PDP8_PAPER_TAPE_PUNCH_BIT_CLEAR)
#define PDP8_PAPER_TAPE_PUNCH_PLS \
    PDP8_PAPER_TAPE_PUNCH_INSTR(PDP8_PAPER_TAPE_PUNCH_BIT_PUNCH)
#define PDP8_PAPER_TAPE_PUNCH_PPC \
    PDP8_PAPER_TAPE_PUNCH_INSTR(PDP8_PAPER_TAPE_PUNCH_BIT_CLEAR | \
                                PDP8_PAPER_TAPE_PUNCH_BIT_PUNCH)

pdp8_paper_tape_punch_t *pdp8_paper_tape_punch_create(void);
void pdp8_paper_tape_punch_destroy(pdp8_paper_tape_punch_t *punch);
int pdp8_paper_tape_punch_attach(pdp8_t *cpu, pdp8_paper_tape_punch_t *punch);

int pdp8_paper_tape_punch_set_stream(pdp8_paper_tape_punch_t *punch, FILE *stream);
int pdp8_paper_tape_punch_set_output_path(pdp8_paper_tape_punch_t *punch, const char *path);
int pdp8_paper_tape_punch_set_output_callback(pdp8_paper_tape_punch_t *punch,
                                              pdp8_paper_tape_punch_output_callback callback,
                                              void *context);
size_t pdp8_paper_tape_punch_bytes_written(const pdp8_paper_tape_punch_t *punch);

#ifdef __cplusplus
}
#endif

#endif /* PDP8_PAPER_TAPE_PUNCH_H */
