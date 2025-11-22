#ifndef PDP8_KL8E_CONSOLE_H
#define PDP8_KL8E_CONSOLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdp8 pdp8_t;
typedef struct pdp8_kl8e_console pdp8_kl8e_console_t;
typedef void (*pdp8_kl8e_console_output_callback)(uint8_t ch, void *context);

#define PDP8_KL8E_KEYBOARD_DEVICE_CODE 003u
#define PDP8_KL8E_TELEPRINTER_DEVICE_CODE 004u
#define PDP8_KL8E_IOT_BASE(device) (06000u | ((uint16_t)((device) & 0x3Fu) << 3))
#define PDP8_KL8E_KEYBOARD_BIT_SKIP 0x1u
#define PDP8_KL8E_KEYBOARD_BIT_CLEAR 0x2u
#define PDP8_KL8E_KEYBOARD_BIT_READ 0x4u
#define PDP8_KL8E_TELEPRINTER_BIT_SKIP 0x1u
#define PDP8_KL8E_TELEPRINTER_BIT_CLEAR 0x2u
#define PDP8_KL8E_TELEPRINTER_BIT_LOAD 0x4u

#define PDP8_KL8E_KEYBOARD_INSTR(bits) \
    (PDP8_KL8E_IOT_BASE(PDP8_KL8E_KEYBOARD_DEVICE_CODE) | (uint16_t)((bits) & 0x7u))
#define PDP8_KL8E_TELEPRINTER_INSTR(bits) \
    (PDP8_KL8E_IOT_BASE(PDP8_KL8E_TELEPRINTER_DEVICE_CODE) | (uint16_t)((bits) & 0x7u))

pdp8_kl8e_console_t *pdp8_kl8e_console_create(FILE *input_stream, FILE *output_stream);
void pdp8_kl8e_console_destroy(pdp8_kl8e_console_t *console);
int pdp8_kl8e_console_attach(pdp8_t *cpu, pdp8_kl8e_console_t *console);
int pdp8_kl8e_console_queue_input(pdp8_kl8e_console_t *console, uint8_t ch);
size_t pdp8_kl8e_console_input_pending(const pdp8_kl8e_console_t *console);
size_t pdp8_kl8e_console_output_pending(const pdp8_kl8e_console_t *console);
int pdp8_kl8e_console_pop_output(pdp8_kl8e_console_t *console, uint8_t *ch);
int pdp8_kl8e_console_flush(pdp8_kl8e_console_t *console);
int pdp8_kl8e_console_set_output_stream(pdp8_kl8e_console_t *console, FILE *stream);
int pdp8_kl8e_console_set_output_callback(pdp8_kl8e_console_t *console,
                                          pdp8_kl8e_console_output_callback callback,
                                          void *context);

#ifdef __cplusplus
}
#endif

#endif
