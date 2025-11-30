#include "tc08_device.h"
#include "pdp8.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TC08_FRAME_WORDS 129u       /* physical frame words (last word checksum) */
#define TC08_FRAME_DATA_WORDS 128u  /* data words per frame moved via IOT */
#define TC08_BLOCK_MASK 0x3FFu
#define TC08_UNIT_SHIFT 10u
#define TC08_UNIT_MASK 0x1u
#define TC08_WRITE_FLAG 0x800u
#define TC08_UNIT_COUNT 2u

static size_t tc08_block_base(uint16_t block) {
    return (size_t)block * TC08_FRAME_WORDS;
}

static void tc08_unit_init(tc08_unit_t *unit,
                           const char *path,
                           bool writable,
                           bool create_if_missing) {
    if (!unit) {
        return;
    }
    unit->image = NULL;
    unit->image_words = 0;
    unit->writable = writable;
    unit->path[0] = '\0';

    if (path && *path) {
        snprintf(unit->path, sizeof unit->path, "%s", path);
    }

    FILE *fp = fopen(unit->path, "rb");
    if (!fp && writable && create_if_missing && unit->path[0] != '\0') {
        fp = fopen(unit->path, "w+b");
    }
    if (!fp) {
        return;
    }

    if (fseek(fp, 0, SEEK_END) == 0) {
        long sz = ftell(fp);
        if (sz > 0) {
            rewind(fp);
            size_t words = (size_t)sz / 2u;
            unit->image = calloc(words, sizeof(uint16_t));
            if (unit->image) {
                unit->image_words = words;
                for (size_t i = 0; i < words; ++i) {
                    uint8_t buf[2];
                    if (fread(buf, 1, 2, fp) != 2) {
                        unit->image_words = i;
                        break;
                    }
                    unit->image[i] = ((uint16_t)buf[1] << 8 | (uint16_t)buf[0]) & 0x0FFFu;
                }
            }
        }
    }
    fclose(fp);
}

static void tc08_unit_free(tc08_unit_t *unit) {
    if (!unit) {
        return;
    }
    free(unit->image);
    unit->image = NULL;
    unit->image_words = 0;
}

static int tc08_unit_flush(const tc08_unit_t *unit) {
    if (!unit || !unit->writable || !unit->path[0] || !unit->image) {
        return -1;
    }
    FILE *fp = fopen(unit->path, "r+b");
    if (!fp) {
        fp = fopen(unit->path, "w+b");
    }
    if (!fp) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    for (size_t i = 0; i < unit->image_words; ++i) {
        uint8_t buf[2];
        buf[0] = (uint8_t)(unit->image[i] & 0xFFu);
        buf[1] = (uint8_t)((unit->image[i] >> 8) & 0xFFu);
        if (fwrite(buf, 1, 2, fp) != 2) {
            fclose(fp);
            return -1;
        }
    }
    if (fflush(fp) != 0) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        return -1;
    }
    return 0;
}

static int tc08_unit_ensure_capacity(tc08_unit_t *unit, uint16_t block) {
    if (!unit) {
        return -1;
    }
    size_t required_words = tc08_block_base((uint16_t)(block + 1u));
    if (required_words <= unit->image_words) {
        return 0;
    }
    uint16_t *new_image = realloc(unit->image, required_words * sizeof(uint16_t));
    if (!new_image) {
        return -1;
    }
    for (size_t i = unit->image_words; i < required_words; ++i) {
        new_image[i] = 0u;
    }
    unit->image = new_image;
    unit->image_words = required_words;
    return 0;
}

static void tc08_device_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    pdp8_tc08_device_t *dev = (pdp8_tc08_device_t *)context;
    uint8_t func = instruction & 0x7;
    uint16_t ac = pdp8_api_get_ac(cpu);
    uint8_t device = (uint8_t)((instruction >> 3) & 0x3Fu);
    switch (func) {
        case TC08_DCR: /* Clear/reset (device 076) */
            dev->status = 1u; /* Ready by default after reset */
            dev->transfer_addr = 0;
            dev->block_num = 0;
            break;
        case TC08_DTXA: /* Load transfer address from AC (device 077 func 1) */
            if (device == PDP8_TC08_DEVICE_CODE_ALT) {
                dev->transfer_addr = ac & 0xFFFu;
            }
            break;
        case TC08_DTSF: /* Skip if ready (device 076 func 4) */
            /* Bootstrap polls this for “ready”; treat power-up as ready. */
            dev->status = 1u;
            pdp8_api_set_ac(cpu, 1u);
            pdp8_api_request_skip(cpu);
            break;
        case TC08_DTLB: { /* Load block number and start transfer (device 076 func 6) */
            uint16_t block = ac & TC08_BLOCK_MASK;       /* bits 0-9: block */
            uint8_t unit_index = (uint8_t)((ac >> TC08_UNIT_SHIFT) & TC08_UNIT_MASK); /* bit 10: unit */
            bool write_mode = (ac & TC08_WRITE_FLAG) != 0u; /* bit 11: write when set */
            dev->block_num = block;
            dev->status &= ~3u; /* clear ready/error */
            size_t mem_words = pdp8_api_get_memory_words(cpu);
            if (unit_index >= TC08_UNIT_COUNT || mem_words == 0u) {
                dev->status |= 2u;
                break;
            }
            tc08_unit_t *unit = &dev->units[unit_index];
            if (write_mode) {
                if (!unit->writable) {
                    dev->status |= 2u;
                    break;
                }
                if (tc08_unit_ensure_capacity(unit, block) != 0) {
                    dev->status |= 2u;
                    break;
                }
                size_t base = tc08_block_base(block);
                for (size_t i = 0; i < TC08_FRAME_DATA_WORDS; ++i) {
                    uint16_t src = (uint16_t)((dev->transfer_addr + (uint16_t)i) % mem_words);
                    unit->image[base + i] = pdp8_api_read_mem(cpu, src) & 0x0FFFu;
                }
                /* checksum word */
                unit->image[base + TC08_FRAME_DATA_WORDS] = 0u; /* checksum placeholder */
                if (tc08_unit_flush(unit) != 0) {
                    dev->status |= 2u;
                    break;
                }
            } else {
                if (!unit->image || block >= unit->image_words / TC08_FRAME_WORDS) {
                    dev->status |= 2u; /* error */
                    break;
                }
                size_t base = tc08_block_base(block);
                for (size_t i = 0; i < TC08_FRAME_DATA_WORDS; ++i) {
                    uint16_t dest = (uint16_t)((dev->transfer_addr + (uint16_t)i) % mem_words);
                    pdp8_api_write_mem(cpu, dest, unit->image[base + i]);
                }
            }
            dev->status |= 1u; /* ready */
            pdp8_api_request_skip(cpu); /* mimic skip-if-ready after transfer */
            break;
        }
        default:
            break;
    }
}

pdp8_tc08_device_t *pdp8_tc08_device_create(void) {
    pdp8_tc08_device_t *dev = calloc(1, sizeof(pdp8_tc08_device_t));
    if (!dev) {
        return NULL;
    }

    const char *path0 = getenv("TC08_IMAGE0");
    if (!path0 || *path0 == '\0') {
        path0 = "media/boot-tc08.tu56";
    }
    const char *path1 = getenv("TC08_IMAGE1");
    if (!path1 || *path1 == '\0') {
        path1 = "magtape/tc08-unit1.tu56";
    }

    tc08_unit_init(&dev->units[0], path0, false, false);
    tc08_unit_init(&dev->units[1], path1, true, true);

    dev->status = 1u; /* ready after power-up/reset */
    dev->transfer_addr = 0;
    dev->block_num = 0;
    return dev;
}

void pdp8_tc08_device_destroy(pdp8_tc08_device_t *device) {
    if (device) {
        tc08_unit_free(&device->units[0]);
        tc08_unit_free(&device->units[1]);
        free(device);
    }
}

int pdp8_tc08_device_attach(pdp8_t *cpu, pdp8_tc08_device_t *device) {
    if (!cpu || !device) return -1;
    if (pdp8_api_register_iot(cpu, PDP8_TC08_DEVICE_CODE, tc08_device_iot, device) != 0) {
        return -1;
    }
    /* Register the alternate device code (077) for DTXA */
    if (pdp8_api_register_iot(cpu, PDP8_TC08_DEVICE_CODE_ALT, tc08_device_iot, device) != 0) {
        return -1;
    }
    return 0;
}
