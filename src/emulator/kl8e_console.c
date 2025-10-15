#include "kl8e_console.h"

#include "pdp8.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PDP8_KL8E_ASCII_MASK 0x7Fu

struct pdp8_buffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
};

struct pdp8_kl8e_console {
    FILE *input_stream;
    FILE *output_stream;
    uint8_t keyboard_buffer;
    bool keyboard_flag;
    struct pdp8_buffer pending_input;
    struct pdp8_buffer output_log;
    bool teleprinter_flag;
};

static void buffer_release(struct pdp8_buffer *buffer) {
    if (!buffer) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

static int buffer_reserve(struct pdp8_buffer *buffer, size_t required) {
    if (!buffer) {
        return -1;
    }
    if (required <= buffer->capacity) {
        return 0;
    }
    size_t new_capacity = buffer->capacity ? buffer->capacity : 8u;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2u) {
            new_capacity = required;
            break;
        }
        new_capacity *= 2u;
    }
    uint8_t *new_data = (uint8_t *)realloc(buffer->data, new_capacity * sizeof(uint8_t));
    if (!new_data) {
        return -1;
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return 0;
}

static int buffer_push_back(struct pdp8_buffer *buffer, uint8_t value) {
    if (!buffer) {
        return -1;
    }
    if (buffer_reserve(buffer, buffer->size + 1u) != 0) {
        return -1;
    }
    buffer->data[buffer->size++] = value;
    return 0;
}

static int buffer_pop_front(struct pdp8_buffer *buffer, uint8_t *value) {
    if (!buffer || buffer->size == 0u) {
        return 1;
    }
    if (value) {
        *value = buffer->data[0];
    }
    if (buffer->size > 1u) {
        memmove(buffer->data, buffer->data + 1u, (buffer->size - 1u) * sizeof(uint8_t));
    }
    buffer->size -= 1u;
    return 0;
}

static void keyboard_promote_pending(pdp8_kl8e_console_t *console) {
    if (!console || console->keyboard_flag) {
        return;
    }
    uint8_t next_char = 0;
    if (buffer_pop_front(&console->pending_input, &next_char) == 0) {
        console->keyboard_buffer = next_char;
        console->keyboard_flag = true;
    }
}

static void keyboard_consume_current(pdp8_kl8e_console_t *console) {
    if (!console) {
        return;
    }
    console->keyboard_flag = false;
    console->keyboard_buffer = 0;
    keyboard_promote_pending(console);
}

static void teleprinter_record_output(pdp8_kl8e_console_t *console, uint8_t ch) {
    if (!console) {
        return;
    }
    buffer_push_back(&console->output_log, ch);
    if (console->output_stream) {
        fputc((int)ch, console->output_stream);
        fflush(console->output_stream);
    }
}

static void kl8e_keyboard_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    pdp8_kl8e_console_t *console = (pdp8_kl8e_console_t *)context;
    if (!cpu || !console) {
        return;
    }

    uint8_t microcode = (uint8_t)(instruction & 0x7u);

    if (microcode == 0u) {
        keyboard_consume_current(console);
        return;
    }

    if ((microcode & 0x1u) && console->keyboard_flag) {
        pdp8_api_request_skip(cpu);
    }

    bool clear_flag = (microcode & 0x2u) != 0u;
    bool read_buffer = (microcode & 0x4u) != 0u;
    bool had_char = console->keyboard_flag;
    uint8_t current_char = console->keyboard_buffer;

    if (clear_flag) {
        pdp8_api_set_ac(cpu, 0u);
    }

    if (read_buffer && had_char) {
        uint16_t ac = pdp8_api_get_ac(cpu) & 0x0FFFu;
        ac = (uint16_t)((ac | current_char) & 0x0FFFu);
        pdp8_api_set_ac(cpu, ac);
    }

    if (clear_flag) {
        keyboard_consume_current(console);
    }
}

static void kl8e_teleprinter_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    pdp8_kl8e_console_t *console = (pdp8_kl8e_console_t *)context;
    if (!cpu || !console) {
        return;
    }

    uint8_t microcode = (uint8_t)(instruction & 0x7u);

    if ((microcode & 0x1u) && console->teleprinter_flag) {
        pdp8_api_request_skip(cpu);
    }

    if (microcode & 0x2u) {
        console->teleprinter_flag = false;
    }

    if (microcode & 0x4u) {
        uint16_t ac = pdp8_api_get_ac(cpu);
        uint8_t ch = (uint8_t)(ac & PDP8_KL8E_ASCII_MASK);
        console->teleprinter_flag = false;
        teleprinter_record_output(console, ch);
        console->teleprinter_flag = true;
    }
}

pdp8_kl8e_console_t *pdp8_kl8e_console_create(FILE *input_stream, FILE *output_stream) {
    pdp8_kl8e_console_t *console = (pdp8_kl8e_console_t *)calloc(1, sizeof(pdp8_kl8e_console_t));
    if (!console) {
        return NULL;
    }

    console->input_stream = input_stream;
    console->output_stream = output_stream ? output_stream : stdout;
    console->keyboard_buffer = 0;
    console->keyboard_flag = false;
    console->teleprinter_flag = true;

    return console;
}

void pdp8_kl8e_console_destroy(pdp8_kl8e_console_t *console) {
    if (!console) {
        return;
    }
    if (console->output_stream) {
        fflush(console->output_stream);
    }
    buffer_release(&console->pending_input);
    buffer_release(&console->output_log);
    free(console);
}

int pdp8_kl8e_console_attach(pdp8_t *cpu, pdp8_kl8e_console_t *console) {
    if (!cpu || !console) {
        return -1;
    }
    if (pdp8_api_register_iot(cpu, PDP8_KL8E_KEYBOARD_DEVICE_CODE, kl8e_keyboard_iot, console) != 0) {
        return -1;
    }
    if (pdp8_api_register_iot(cpu, PDP8_KL8E_TELEPRINTER_DEVICE_CODE, kl8e_teleprinter_iot, console) != 0) {
        pdp8_api_register_iot(cpu, PDP8_KL8E_KEYBOARD_DEVICE_CODE, NULL, NULL);
        return -1;
    }
    return 0;
}

int pdp8_kl8e_console_queue_input(pdp8_kl8e_console_t *console, uint8_t ch) {
    if (!console) {
        return -1;
    }
    uint8_t value = (uint8_t)(ch & PDP8_KL8E_ASCII_MASK);
    if (!console->keyboard_flag) {
        console->keyboard_buffer = value;
        console->keyboard_flag = true;
        return 0;
    }
    return buffer_push_back(&console->pending_input, value);
}

size_t pdp8_kl8e_console_input_pending(const pdp8_kl8e_console_t *console) {
    if (!console) {
        return 0u;
    }
    size_t pending = console->pending_input.size;
    if (console->keyboard_flag) {
        pending += 1u;
    }
    return pending;
}

size_t pdp8_kl8e_console_output_pending(const pdp8_kl8e_console_t *console) {
    return console ? console->output_log.size : 0u;
}

int pdp8_kl8e_console_pop_output(pdp8_kl8e_console_t *console, uint8_t *ch) {
    if (!console || !ch) {
        return -1;
    }
    return buffer_pop_front(&console->output_log, ch);
}

int pdp8_kl8e_console_flush(pdp8_kl8e_console_t *console) {
    if (!console) {
        return -1;
    }
    if (!console->output_stream) {
        return 0;
    }
    return fflush(console->output_stream);
}
