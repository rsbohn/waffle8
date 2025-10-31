#include "interrupt_control.h"
#include "pdp8.h"

#include <string.h>

static void interrupt_control_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    (void)context;  /* Unused */
    if (!cpu) return;

    uint8_t func = (uint8_t)(instruction & 0x7u);

    switch (func) {
    case 0x0u: /* IOFF - Interrupt OFF */
        pdp8_api_set_interrupt_enable(cpu, 0);
        break;

    case 0x1u: /* ION - Interrupt ON */
        pdp8_api_set_interrupt_enable(cpu, 1);
        break;

    case 0x2u: /* SKON - Skip if Interrupt ON */
        if (pdp8_api_is_interrupt_enabled(cpu)) {
            pdp8_api_request_skip(cpu);
        }
        break;

    case 0x3u:
    case 0x4u:
    case 0x5u:
    case 0x6u:
    case 0x7u:
        /* Undefined functions - silently ignore for now */
        break;
    }
}

int pdp8_interrupt_control_attach(pdp8_t *cpu) {
    if (!cpu) return -1;
    return pdp8_api_register_iot(cpu, PDP8_INTERRUPT_CONTROL_DEVICE_CODE, 
                                 interrupt_control_iot, NULL);
}
