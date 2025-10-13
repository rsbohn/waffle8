#include "pdp8_board.h"

#include "pdp8.h"

static const pdp8_board_spec host_simulator_spec = {
    .name = "Host Simulator",
    .mcu = "N/A",
    .core_clock_hz = 0u,
    .tick_hz = 60u,
    .memory_words = 4096u,
    .ram_bytes = 0u,
    .flash_bytes = 0u,
    .rom_words = 0u,
    .rom_image = NULL,
};

static const pdp8_board_spec adafruit_fruit_jam_spec = {
    .name = "Adafruit Fruit Jam",
    .mcu = "RP2350",
    .core_clock_hz = 200000000u,
    .tick_hz = 60u,
    .memory_words = 4096u,
    .ram_bytes = 512u * 1024u,
    .flash_bytes = 16u * 1024u * 1024u,
    .rom_words = 0u,
    .rom_image = NULL,
};

const pdp8_board_spec *pdp8_board_host_simulator(void) {
    return &host_simulator_spec;
}

const pdp8_board_spec *pdp8_board_adafruit_fruit_jam(void) {
    return &adafruit_fruit_jam_spec;
}

pdp8_t *pdp8_api_create_for_board(const pdp8_board_spec *spec) {
    if (!spec) {
        return NULL;
    }

    pdp8_t *cpu = pdp8_api_create(spec->memory_words);
    if (!cpu) {
        return NULL;
    }

    if (pdp8_api_attach_board(cpu, spec) != 0) {
        pdp8_api_destroy(cpu);
        return NULL;
    }

    return cpu;
}
