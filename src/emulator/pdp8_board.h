#ifndef PDP8_BOARD_H
#define PDP8_BOARD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;

typedef struct {
    const char *name;
    const char *mcu;
    uint32_t core_clock_hz;
    uint32_t tick_hz;
    size_t memory_words;
    size_t ram_bytes;
    size_t flash_bytes;
    size_t rom_words;
    const uint16_t *rom_image;
} pdp8_board_spec;

const pdp8_board_spec *pdp8_board_host_simulator(void);
const pdp8_board_spec *pdp8_board_adafruit_fruit_jam(void);

pdp8_t *pdp8_api_create_for_board(const pdp8_board_spec *spec);
int pdp8_api_attach_board(pdp8_t *cpu, const pdp8_board_spec *spec);
const pdp8_board_spec *pdp8_api_get_board(const pdp8_t *cpu);

#ifdef __cplusplus
}
#endif

#endif
