#ifndef PDP8_H
#define PDP8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;

typedef void (*pdp8_iot_handler)(pdp8_t *cpu, uint16_t instruction, void *context);

pdp8_t *pdp8_api_create(size_t memory_size);
void pdp8_api_destroy(pdp8_t *cpu);
void pdp8_api_reset(pdp8_t *cpu);
void pdp8_api_clear_halt(pdp8_t *cpu);
int pdp8_api_step(pdp8_t *cpu);
int pdp8_api_run(pdp8_t *cpu, size_t max_cycles);
uint16_t pdp8_api_get_ac(const pdp8_t *cpu);
void pdp8_api_set_ac(pdp8_t *cpu, uint16_t value);
uint16_t pdp8_api_get_pc(const pdp8_t *cpu);
void pdp8_api_set_pc(pdp8_t *cpu, uint16_t value);
uint8_t pdp8_api_get_link(const pdp8_t *cpu);
void pdp8_api_set_link(pdp8_t *cpu, uint8_t value);
int pdp8_api_write_mem(pdp8_t *cpu, uint16_t address, uint16_t value);
uint16_t pdp8_api_read_mem(const pdp8_t *cpu, uint16_t address);
int pdp8_api_load(pdp8_t *cpu, const uint16_t *words, size_t count, uint16_t start_address);
int pdp8_api_register_iot(pdp8_t *cpu, uint8_t device_code, pdp8_iot_handler handler, void *context);
void pdp8_api_request_skip(pdp8_t *cpu);
void pdp8_api_set_switch_register(pdp8_t *cpu, uint16_t value);
uint16_t pdp8_api_get_switch_register(const pdp8_t *cpu);
int pdp8_api_is_halted(const pdp8_t *cpu);

#ifdef __cplusplus
}
#endif

#endif
