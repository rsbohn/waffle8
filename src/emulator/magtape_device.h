#ifndef PDP8_MAGTAPE_DEVICE_H
#define PDP8_MAGTAPE_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pdp8;
typedef struct pdp8 pdp8_t;
typedef struct pdp8_magtape_device pdp8_magtape_device_t;

#define PDP8_MAGTAPE_DEVICE_CODE 070u
#define PDP8_MAGTAPE_IOT_BASE \
    (06000u | ((uint16_t)(PDP8_MAGTAPE_DEVICE_CODE & 0x3Fu) << 3))
#define PDP8_MAGTAPE_BIT_GO 0x01u
#define PDP8_MAGTAPE_BIT_READ 0x02u
#define PDP8_MAGTAPE_BIT_WRITE 0x04u
#define PDP8_MAGTAPE_BIT_SKIP 0x08u
#define PDP8_MAGTAPE_BIT_REWIND 0x10u
#define PDP8_MAGTAPE_BIT_SENSE 0x20u
#define PDP8_MAGTAPE_INSTR(bits) \
    (PDP8_MAGTAPE_IOT_BASE | (uint16_t)((bits) & 0x3Fu))

#define PDP8_MAGTAPE_SIXBIT_PER_WORD 2u
#define PDP8_MAGTAPE_SIXBIT_WORDS(char_count) \
    (((char_count) + PDP8_MAGTAPE_SIXBIT_PER_WORD - 1u) / PDP8_MAGTAPE_SIXBIT_PER_WORD)
#define PDP8_MAGTAPE_HEADER_LABEL_CHARS 6u
#define PDP8_MAGTAPE_HEADER_LABEL_WORDS \
    PDP8_MAGTAPE_SIXBIT_WORDS(PDP8_MAGTAPE_HEADER_LABEL_CHARS)
#define PDP8_MAGTAPE_HEADER_DATA_FORMAT_CHARS 6u
#define PDP8_MAGTAPE_HEADER_DATA_FORMAT_WORDS \
    PDP8_MAGTAPE_SIXBIT_WORDS(PDP8_MAGTAPE_HEADER_DATA_FORMAT_CHARS)

struct pdp8_magtape_record_header {
    uint16_t label[PDP8_MAGTAPE_HEADER_LABEL_WORDS];
    uint16_t data_format[PDP8_MAGTAPE_HEADER_DATA_FORMAT_WORDS];
};

struct pdp8_magtape_unit_params {
    unsigned unit_number;
    const char *path;
    bool write_protected;
};

struct pdp8_magtape_unit_status {
    bool configured;
    unsigned unit_number;
    const char *path;
    const char *current_record;
    size_t record_index;
    size_t record_count;
    size_t word_position;
    size_t word_count;
    bool ready;
    bool write_protected;
    bool end_of_record;
    bool end_of_tape;
    bool error;
    bool partial_record;
};

pdp8_magtape_device_t *pdp8_magtape_device_create(void);
void pdp8_magtape_device_destroy(pdp8_magtape_device_t *device);
int pdp8_magtape_device_attach(pdp8_t *cpu, pdp8_magtape_device_t *device);
int pdp8_magtape_device_configure_unit(pdp8_magtape_device_t *device,
                                       const struct pdp8_magtape_unit_params *params);
int pdp8_magtape_device_rewind(pdp8_magtape_device_t *device, unsigned unit);
int pdp8_magtape_device_force_new_record(pdp8_magtape_device_t *device, unsigned unit);
int pdp8_magtape_device_get_status(const pdp8_magtape_device_t *device,
                                   unsigned unit,
                                   struct pdp8_magtape_unit_status *out_status);

#ifdef __cplusplus
}
#endif

#endif
