#include "paper_tape_device.h"

#include "paper_tape.h"
#include "pdp8.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct pdp8_paper_tape_device {
    pdp8_paper_tape *image;
    const pdp8_paper_tape_block *current;
    size_t index;
    bool ready;
};

static void paper_tape_device_reset(pdp8_paper_tape_device_t *device) {
    if (!device) {
        return;
    }
    device->current = NULL;
    device->index = 0u;
    device->ready = false;
}

static void paper_tape_device_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    pdp8_paper_tape_device_t *device = (pdp8_paper_tape_device_t *)context;
    if (!device) {
        return;
    }

    uint16_t microcode = instruction & 0x7u;

    if (microcode & PDP8_PAPER_TAPE_BIT_SKIP) {
        if (device->ready) {
            pdp8_api_request_skip(cpu);
        }
    }

    if (microcode & PDP8_PAPER_TAPE_BIT_SELECT) {
        paper_tape_device_reset(device);
        if (!device->image) {
            return;
        }
        uint16_t requested_block = (uint16_t)(pdp8_api_get_ac(cpu) & 0x0FFFu);
        const pdp8_paper_tape_block *block = pdp8_paper_tape_find(device->image, requested_block);
        if (!block) {
            return;
        }
        device->current = block;
        device->index = 0u;
        device->ready = (block->word_count > 0u);
    }

    if (microcode & PDP8_PAPER_TAPE_BIT_READ) {
        if (!device->current || device->index >= device->current->word_count) {
            device->ready = false;
            return;
        }
        uint16_t word = device->current->words[device->index++] & 0x0FFFu;
        pdp8_api_set_ac(cpu, word);
        device->ready = (device->index < device->current->word_count);
    }
}

pdp8_paper_tape_device_t *pdp8_paper_tape_device_create(void) {
    pdp8_paper_tape_device_t *device = (pdp8_paper_tape_device_t *)calloc(1, sizeof(pdp8_paper_tape_device_t));
    if (!device) {
        return NULL;
    }
    paper_tape_device_reset(device);
    device->image = NULL;
    return device;
}

void pdp8_paper_tape_device_destroy(pdp8_paper_tape_device_t *device) {
    if (!device) {
        return;
    }
    if (device->image) {
        pdp8_paper_tape_destroy(device->image);
    }
    free(device);
}

int pdp8_paper_tape_device_attach(pdp8_t *cpu, pdp8_paper_tape_device_t *device) {
    if (!cpu || !device) {
        return -1;
    }
    return pdp8_api_register_iot(cpu, (uint8_t)PDP8_PAPER_TAPE_DEVICE_CODE, paper_tape_device_iot, device);
}

int pdp8_paper_tape_device_load(pdp8_paper_tape_device_t *device, const char *path) {
    if (!device || !path) {
        return -1;
    }
    pdp8_paper_tape *image = NULL;
    if (pdp8_paper_tape_load(path, &image) != 0) {
        return -1;
    }
    if (device->image) {
        pdp8_paper_tape_destroy(device->image);
    }
    device->image = image;
    paper_tape_device_reset(device);
    return 0;
}

const char *pdp8_paper_tape_device_label(const pdp8_paper_tape_device_t *device) {
    if (!device || !device->image) {
        return NULL;
    }
    return device->image->label;
}
