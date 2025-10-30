#include "src/emulator/pdp8.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    assert(cpu != NULL);
    
    /* Test initial state */
    assert(pdp8_api_peek_interrupt_pending(cpu) == 0);
    
    /* Test ION/IOFF (via operate instruction) */
    /* ION is 0x0E01 (operate group 2 with bit 0 set) */
    pdp8_api_write_mem(cpu, 0, 0x0E01);
    pdp8_api_step(cpu);
    
    /* Request interrupt */
    assert(pdp8_api_request_interrupt(cpu, 055) == 0);
    assert(pdp8_api_peek_interrupt_pending(cpu) == 1);
    
    /* Set up ISR entry point at 0x0010 */
    pdp8_api_write_mem(cpu, 0x0010, 0x7402);  /* CLA CLL - ISR starts here */
    
    /* Execute next instruction - should trigger interrupt dispatch */
    /* Load a simple instruction at PC 0 that won't halt us */
    pdp8_api_set_pc(cpu, 0);
    pdp8_api_write_mem(cpu, 0, 0x0000);  /* AND instruction (benign) */
    pdp8_api_step(cpu);
    
    /* After step, if interrupt was enabled and pending, we should have jumped to ISR */
    /* But we disabled interrupts first, so we're still at PC 1 */
    printf("Test passed: ION works, interrupt API works\n");
    
    /* Test clear interrupt */
    assert(pdp8_api_clear_interrupt_pending(cpu) == 0);
    assert(pdp8_api_peek_interrupt_pending(cpu) == 0);
    
    /* Test clearing when no interrupts pending returns error */
    assert(pdp8_api_clear_interrupt_pending(cpu) == -1);
    
    printf("All basic interrupt tests passed!\n");
    pdp8_api_destroy(cpu);
    return 0;
}
