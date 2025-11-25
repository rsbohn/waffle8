#include "tc08_device.h"
#include "pdp8.h"
#include <stdlib.h>

struct pdp8_tc08_device {
    // Already defined in header:
    // typedef struct {
    //     uint16_t status;
    //     uint16_t data;
    //     uint16_t command;
    // } tc08_regs_t;

    // struct pdp8_tc08_device defined in header
    tc08_regs_t regs;
};
};

static void tc08_device_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    static void tc08_device_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
        pdp8_tc08_device_t *dev = (pdp8_tc08_device_t *)context;
        uint8_t func = instruction & 0x7;
        switch (func) {
            case TC08_DCR: // Device Clear/Reset
                dev->regs.status = 0;
                dev->regs.data = 0;
                dev->regs.command = 0;
                break;
            case TC08_DSR: // Device Status Register (to AC)
                cpu->ac = (cpu->ac | dev->regs.status) & 0xFFFu;
                break;
            case TC08_DLR: // Data Register Load (from AC)
                dev->regs.data = cpu->ac & 0xFFFu;
                break;
            case TC08_DSRD: // Data Register Store (to AC)
                cpu->ac = (cpu->ac | dev->regs.data) & 0xFFFu;
                break;
            case TC08_SKS: // Skip on Status (example: skip if ready)
                if (dev->regs.status & 1) {
                    cpu->skip_pending = true;
                }
                break;
            case TC08_GO: // Start operation (GO)
                dev->regs.command = dev->regs.data;
                // TODO: Implement actual tape operation (read/write/seek)
                // For now, set status to ready
                dev->regs.status |= 1; // Set ready bit
                break;
            default:
                // Unrecognized IOT, ignore
                break;
        }
    }
}

pdp8_tc08_device_t *pdp8_tc08_device_create(void) {
    pdp8_tc08_device_t *dev = calloc(1, sizeof(pdp8_tc08_device_t));
    if (dev) {
        dev->regs.status = 1; // Set ready bit by default
        dev->regs.data = 0;
        dev->regs.command = 0;
    }
    return dev;
}

void pdp8_tc08_device_destroy(pdp8_tc08_device_t *device) {
    if (device) {
        // TODO: Free any allocated resources
        free(device);
    }
}

int pdp8_tc08_device_attach(pdp8_t *cpu, pdp8_tc08_device_t *device) {
    if (!cpu || !device) return -1;
    return pdp8_api_register_iot(cpu, PDP8_TC08_DEVICE_CODE, tc08_device_iot, device);
}
