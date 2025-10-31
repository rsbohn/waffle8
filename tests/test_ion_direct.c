#include <stdio.h>
#include <stdint.h>
#include "src/emulator/pdp8.h"
#include "src/emulator/interrupt_control.h"

int main() {
    // Create CPU
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        fprintf(stderr, "Failed to create CPU\n");
        return 1;
    }
    printf("CPU created\n");

    // Attach interrupt control device
    int result = pdp8_interrupt_control_attach(cpu);
    printf("Interrupt control attach result: %d\n", result);
    if (result != 0) {
        fprintf(stderr, "Failed to attach interrupt control device\n");
        pdp8_api_destroy(cpu);
        return 1;
    }

    // Test 1: Initial state - ION should be off
    printf("\n=== Test 1: Initial State ===\n");
    int ion_before = pdp8_api_is_interrupt_enabled(cpu);
    printf("ION before: %d (expected 0)\n", ion_before);

    // Test 2: Execute ION (6001 octal)
    printf("\n=== Test 2: ION (6001 octal = 0x0C01 hex) ===\n");
    pdp8_api_set_pc(cpu, 0);
    pdp8_api_write_mem(cpu, 0, 0x0C01);  // 6001 ION
    pdp8_api_step(cpu);
    int ion_after = pdp8_api_is_interrupt_enabled(cpu);
    printf("ION after ION instruction: %d (expected 1)\n", ion_after);

    // Test 3: Execute IOFF (6000 octal)
    printf("\n=== Test 3: IOFF (6000 octal = 0x0C00 hex) ===\n");
    pdp8_api_set_pc(cpu, 0);
    pdp8_api_write_mem(cpu, 0, 0x0C00);  // 6000 IOFF
    pdp8_api_step(cpu);
    int ioff_result = pdp8_api_is_interrupt_enabled(cpu);
    printf("ION after IOFF instruction: %d (expected 0)\n", ioff_result);

    // Test 4: Check SKON behavior
    printf("\n=== Test 4: SKON (6002 octal) ===\n");
    pdp8_api_set_pc(cpu, 0);
    pdp8_api_write_mem(cpu, 0, 0x0C02);  // 6002 SKON
    uint16_t pc_before = pdp8_api_get_pc(cpu);
    pdp8_api_step(cpu);
    uint16_t pc_after = pdp8_api_get_pc(cpu);
    printf("PC before SKON: 0x%04x\n", pc_before);
    printf("PC after SKON: 0x%04x\n", pc_after);
    printf("PC should have incremented by 2 (skip)\n");

    pdp8_api_destroy(cpu);
    printf("\nDone.\n");
    return 0;
}
