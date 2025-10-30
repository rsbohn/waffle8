#include "src/emulator/pdp8.h"
#include <stdio.h>

int main(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    
    /* Load instructions */
    pdp8_api_write_mem(cpu, 0, 0x0F01);  /* ION (group 2) */
    pdp8_api_write_mem(cpu, 1, 0x0000);  /* AND */
    pdp8_api_write_mem(cpu, 0x0010, 0x0F02);  /* ISR: CLA CLL (group 2) */
    
    printf("Before ION: PC=%04o\n", pdp8_api_get_pc(cpu));
    pdp8_api_step(cpu);
    printf("After ION: PC=%04o\n", pdp8_api_get_pc(cpu));
    
    /* Request interrupt */
    printf("Requesting interrupt...\n");
    pdp8_api_request_interrupt(cpu, 055);
    printf("Interrupt pending: %d\n", pdp8_api_peek_interrupt_pending(cpu));
    
    /* Execute AND - should trigger dispatch */
    printf("Before AND: PC=%04o, interrupt_pending=%d\n", pdp8_api_get_pc(cpu), pdp8_api_peek_interrupt_pending(cpu));
    pdp8_api_step(cpu);
    printf("After AND: PC=%04o, interrupt_pending=%d\n", pdp8_api_get_pc(cpu), pdp8_api_peek_interrupt_pending(cpu));
    
    if (pdp8_api_get_pc(cpu) == 0x0010) {
        printf("SUCCESS: Dispatch worked! Saved PC at 0x0007: %04o\n", pdp8_api_read_mem(cpu, 0x0007));
    } else {
        printf("FAILED: Expected PC=0x0010, got %04o\n", pdp8_api_get_pc(cpu));
    }
    
    pdp8_api_destroy(cpu);
    return 0;
}
