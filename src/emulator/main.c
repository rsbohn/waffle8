#define _POSIX_C_SOURCE 199309L
#include "pdp8.h"
#include "pdp8_board.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PDP8_WORD_MASK 0x0FFFu
#define PDP8_LINK_MASK 0x01u
#define PDP8_OPCODE_MASK 0x0E00u /* octal 07000 */
#define PDP8_INDIRECT_MASK 0x0100u /* octal 00400 */
#define PDP8_PAGE_MASK 0x0080u     /* octal 00200 */
#define PDP8_OFFSET_MASK 0x007Fu   /* octal 00177 */
#define PDP8_AUTO_INCREMENT_START 0x0008u /* octal 0010 */
#define PDP8_AUTO_INCREMENT_END   0x000Fu /* octal 0017 */

struct pdp8 {
    uint16_t *memory;
    size_t memory_words;
    uint16_t pc;
    uint16_t ac;
    uint8_t link;
    uint16_t switch_register;
    bool halted;
    bool skip_pending;
    bool interrupt_enable;
    int interrupt_pending;
    pdp8_iot_handler iot_handlers[64];
    void *iot_contexts[64];
    pdp8_tick_handler tick_handlers[64];
    void *tick_contexts[64];
    const pdp8_board_spec *board;
};

static int ensure_memory_capacity(pdp8_t *cpu, size_t memory_words) {
    if (!cpu) {
        return -1;
    }
    if (memory_words == 0) {
        memory_words = cpu->memory_words ? cpu->memory_words : 4096u;
    }

    size_t old_words = cpu->memory_words;
    uint16_t *new_memory = (uint16_t *)realloc(cpu->memory, memory_words * sizeof(uint16_t));
    if (!new_memory) {
        return -1;
    }

    if (memory_words > old_words) {
        memset(new_memory + old_words, 0, (memory_words - old_words) * sizeof(uint16_t));
    }

    cpu->memory = new_memory;
    cpu->memory_words = memory_words;

    if (cpu->memory_words) {
        cpu->pc = (uint16_t)(cpu->pc % cpu->memory_words);
    }

    return 0;
}

static uint16_t mask_word(uint16_t value) {
    return value & PDP8_WORD_MASK;
}

static uint16_t normalise_address(const pdp8_t *cpu, uint16_t address) {
    if (cpu->memory_words == 0) {
        return 0;
    }
    return address % (uint16_t)cpu->memory_words;
}

static uint16_t fetch_effective_address(pdp8_t *cpu, uint16_t instruction) {
    uint16_t page_base = (instruction & PDP8_PAGE_MASK) ? (cpu->pc & (uint16_t)~PDP8_OFFSET_MASK) : 0u;
    uint16_t offset = instruction & PDP8_OFFSET_MASK;
    uint16_t address = normalise_address(cpu, page_base | offset);

    if (instruction & PDP8_INDIRECT_MASK) {
        if (address >= PDP8_AUTO_INCREMENT_START && address <= PDP8_AUTO_INCREMENT_END) {
            cpu->memory[address] = mask_word(cpu->memory[address] + 1u);
        }
        address = normalise_address(cpu, cpu->memory[address]);
    }
    return address;
}

static void apply_skip(pdp8_t *cpu) {
    if (cpu->skip_pending) {
        cpu->pc = normalise_address(cpu, cpu->pc + 1u);
        cpu->skip_pending = false;
    }
}

static void operate_group1(pdp8_t *cpu, uint16_t instruction) {
    if (instruction & 0x0080u) { /* CLA */
        cpu->ac = 0;
    }
    if (instruction & 0x0040u) { /* CLL */
        cpu->link = 0;
    }
    if (instruction & 0x0020u) { /* CMA */
        cpu->ac = mask_word(~cpu->ac);
    }
    if (instruction & 0x0010u) { /* CML */
        cpu->link = (uint8_t)(cpu->link ^ 1u);
    }

    bool rotate_twice = false;
    bool rotate_right = false;
    bool rotate_left = false;

    if (instruction & 0x0002u) {
        if ((instruction & (0x0008u | 0x0004u)) == 0u) {
            /* Only the 0x0002 bit set means byte swap */
            uint16_t high = (cpu->ac & 0x03Fu) << 6;
            uint16_t low = (cpu->ac >> 6) & 0x003Fu;
            cpu->ac = high | low;
        } else {
            rotate_twice = true;
        }
    }
    if (instruction & 0x0008u) {
        rotate_right = true;
    }
    if (instruction & 0x0004u) {
        rotate_left = true;
    }

    if (rotate_right && rotate_left) {
        rotate_right = false;
        rotate_left = false;
    }

    if (rotate_right || rotate_left) {
        unsigned rotations = rotate_twice ? 2u : 1u;
        uint16_t combined = ((uint16_t)cpu->link << 12) | cpu->ac;
        while (rotations--) {
            if (rotate_right) {
                uint16_t carry = combined & 0x0001u;
                combined = (combined >> 1) | (carry << 12);
            } else {
                uint16_t carry = (combined >> 12) & 0x0001u;
                combined = ((combined << 1) & 0x1FFFu) | carry;
            }
        }
        cpu->link = (uint8_t)((combined >> 12) & 0x0001u);
        cpu->ac = mask_word(combined);
    }

    if (instruction & 0x0001u) { /* IAC */
        uint16_t combined = ((uint16_t)cpu->link << 12) | cpu->ac;
        combined = (combined + 1u) & 0x1FFFu;
        cpu->link = (uint8_t)((combined >> 12) & 0x1u);
        cpu->ac = mask_word(combined);
    }
}

static void operate_group2(pdp8_t *cpu, uint16_t instruction) {
    if (instruction & 0x0080u) { /* CLA */
        cpu->ac = 0;
    }

    bool sense = (instruction & 0x0008u) != 0u;
    bool conditions[3];
    conditions[0] = (instruction & 0x0040u) && (cpu->ac & 0x0800u); /* SMA */
    conditions[1] = (instruction & 0x0020u) && (cpu->ac == 0);       /* SZA */
    conditions[2] = (instruction & 0x0010u) && (cpu->link != 0);     /* SNL */

    bool any = false;
    for (size_t i = 0; i < 3; ++i) {
        any |= conditions[i];
    }

    if (instruction & 0x0004u) { /* OSR */
        cpu->ac = mask_word(cpu->ac | cpu->switch_register);
    }
    if (instruction & 0x0002u) { /* HLT */
        cpu->halted = true;
    }
    if (instruction & 0x0001u) { /* ION */
        cpu->interrupt_enable = true;
    }
    /* IOFF is implicit: if bit 0x0001 is not set, interrupts remain unchanged */

    bool skip = sense ? !any : any;
    if (skip) {
        cpu->skip_pending = true;
    }
}

static void execute_memory_reference(pdp8_t *cpu, uint16_t instruction) {
    uint16_t opcode = instruction & PDP8_OPCODE_MASK;
    uint16_t address = fetch_effective_address(cpu, instruction);

    switch (opcode) {
    case 0x0000u: /* AND */
        cpu->ac = mask_word(cpu->ac & cpu->memory[address]);
        break;
    case 0x0200u: { /* TAD */
        uint16_t value = cpu->memory[address];
        uint16_t sum = cpu->ac + value;
        if (sum & 0x1000u) {
            cpu->link ^= 1u;
        }
        cpu->ac = mask_word(sum);
        break;
    }
    case 0x0400u: { /* ISZ */
        cpu->memory[address] = mask_word(cpu->memory[address] + 1u);
        if (cpu->memory[address] == 0u) {
            cpu->skip_pending = true;
        }
        break;
    }
    case 0x0600u: /* DCA */
        cpu->memory[address] = mask_word(cpu->ac);
        cpu->ac = 0;
        break;
    case 0x0800u: { /* JMS */
        cpu->memory[address] = mask_word(cpu->pc);
        cpu->pc = normalise_address(cpu, address + 1u);
        break;
    }
    case 0x0A00u: /* JMP */
        cpu->pc = normalise_address(cpu, address);
        break;
    default:
        break;
    }
}

static void execute_iot(pdp8_t *cpu, uint16_t instruction) {
    uint8_t device = (uint8_t)((instruction >> 3) & 0x3Fu);
    pdp8_iot_handler handler = cpu->iot_handlers[device];
    if (handler) {
        handler(cpu, instruction, cpu->iot_contexts[device]);
    }
}

static void execute_operate(pdp8_t *cpu, uint16_t instruction) {
    if ((instruction & 0x0100u) == 0u) {
        operate_group1(cpu, instruction);
    } else {
        operate_group2(cpu, instruction);
    }
}

pdp8_t *pdp8_api_create(size_t memory_size) {
    pdp8_t *cpu = (pdp8_t *)calloc(1, sizeof(pdp8_t));
    if (!cpu) {
        return NULL;
    }

    if (ensure_memory_capacity(cpu, memory_size ? memory_size : 4096u) != 0) {
        free(cpu);
        return NULL;
    }

    return cpu;
}

void pdp8_api_destroy(pdp8_t *cpu) {
    if (!cpu) {
        return;
    }
    free(cpu->memory);
    free(cpu);
}

void pdp8_api_reset(pdp8_t *cpu) {
    if (!cpu) {
        return;
    }
    cpu->pc = 0;
    cpu->ac = 0;
    cpu->link = 0;
    cpu->halted = false;
    cpu->skip_pending = false;
    cpu->interrupt_enable = false;
    cpu->interrupt_pending = 0;
    if (cpu->memory && cpu->memory_words) {
        memset(cpu->memory, 0, cpu->memory_words * sizeof(uint16_t));
    }

    if (cpu->board && cpu->board->rom_image && cpu->board->rom_words) {
        size_t words = cpu->board->rom_words;
        if (words > cpu->memory_words) {
            words = cpu->memory_words;
        }
        for (size_t i = 0; i < words; ++i) {
            cpu->memory[i] = mask_word(cpu->board->rom_image[i]);
        }
    }
}

void pdp8_api_set_halt(pdp8_t *cpu) {
    if (!cpu) {
        return;
    }
    cpu->halted = true;
}

void pdp8_api_clear_halt(pdp8_t *cpu) {
    if (!cpu) {
        return;
    }
    cpu->halted = false;
}

int pdp8_api_step(pdp8_t *cpu) {
    if (!cpu || cpu->halted || cpu->memory_words == 0) {
        return 0;
    }

    uint16_t instruction = cpu->memory[cpu->pc];
    cpu->pc = normalise_address(cpu, cpu->pc + 1u);

    uint16_t opcode = instruction & PDP8_OPCODE_MASK;
    switch (opcode) {
    case 0x0000u:
    case 0x0200u:
    case 0x0400u:
    case 0x0600u:
    case 0x0800u:
    case 0x0A00u:
        execute_memory_reference(cpu, instruction);
        break;
    case 0x0C00u:
        execute_iot(cpu, instruction);
        break;
    case 0x0E00u:
        execute_operate(cpu, instruction);
        break;
    default:
        break;
    }

    apply_skip(cpu);

    /* Interrupt dispatch: check for pending interrupt after instruction execution */
    if (cpu->interrupt_enable && cpu->interrupt_pending > 0) {
        /* Save context: AC at octal 0006, PC at octal 0007, LINK at octal 0010 */
        cpu->memory[006] = cpu->ac;
        cpu->memory[007] = cpu->pc;
        cpu->memory[010] = (uint16_t)cpu->link;
        
        /* Decrement pending count and disable interrupts */
        cpu->interrupt_pending--;
        cpu->interrupt_enable = false;
        
        /* Jump to interrupt service routine at octal 0020 */
        cpu->pc = 020;
    }

    /* call registered tick handlers with current monotonic time (ns) */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        for (uint8_t i = 0; i < 64u; ++i) {
            pdp8_tick_handler th = cpu->tick_handlers[i];
            if (th) {
                th(cpu, cpu->tick_contexts[i], now_ns);
            }
        }
    }
    return 1;
}

int pdp8_api_run(pdp8_t *cpu, size_t max_cycles) {
    if (!cpu) {
        return -1;
    }
    size_t executed = 0;
    while (executed < max_cycles) {
        if (cpu->halted) {
            break;
        }
        if (pdp8_api_step(cpu) == 0) {
            break;
        }
        ++executed;
    }
    return (int)executed;
}

int pdp8_api_attach_board(pdp8_t *cpu, const pdp8_board_spec *spec) {
    if (!cpu || !spec) {
        return -1;
    }

    if (ensure_memory_capacity(cpu, spec->memory_words) != 0) {
        return -1;
    }

    cpu->board = spec;
    pdp8_api_reset(cpu);
    return 0;
}

const pdp8_board_spec *pdp8_api_get_board(const pdp8_t *cpu) {
    return cpu ? cpu->board : NULL;
}

uint16_t pdp8_api_get_ac(const pdp8_t *cpu) {
    return cpu ? cpu->ac : 0u;
}

void pdp8_api_set_ac(pdp8_t *cpu, uint16_t value) {
    if (!cpu) {
        return;
    }
    cpu->ac = mask_word(value);
}

uint16_t pdp8_api_get_pc(const pdp8_t *cpu) {
    return cpu ? cpu->pc : 0u;
}

void pdp8_api_set_pc(pdp8_t *cpu, uint16_t value) {
    if (!cpu) {
        return;
    }
    cpu->pc = normalise_address(cpu, value);
}

uint8_t pdp8_api_get_link(const pdp8_t *cpu) {
    return cpu ? cpu->link & PDP8_LINK_MASK : 0u;
}

void pdp8_api_set_link(pdp8_t *cpu, uint8_t value) {
    if (!cpu) {
        return;
    }
    cpu->link = value & PDP8_LINK_MASK;
}

int pdp8_api_write_mem(pdp8_t *cpu, uint16_t address, uint16_t value) {
    if (!cpu || cpu->memory_words == 0) {
        return -1;
    }
    cpu->memory[normalise_address(cpu, address)] = mask_word(value);
    return 0;
}

uint16_t pdp8_api_read_mem(const pdp8_t *cpu, uint16_t address) {
    if (!cpu || cpu->memory_words == 0) {
        return 0u;
    }
    return cpu->memory[normalise_address(cpu, address)] & PDP8_WORD_MASK;
}

int pdp8_api_load(pdp8_t *cpu, const uint16_t *words, size_t count, uint16_t start_address) {
    if (!cpu || !words) {
        return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        pdp8_api_write_mem(cpu, start_address + (uint16_t)i, words[i]);
    }
    return 0;
}

int pdp8_api_register_iot(pdp8_t *cpu, uint8_t device_code, pdp8_iot_handler handler, void *context) {
    if (!cpu || device_code >= 64u) {
        return -1;
    }
    cpu->iot_handlers[device_code] = handler;
    cpu->iot_contexts[device_code] = context;
    return 0;
}

int pdp8_api_register_tick(pdp8_t *cpu, uint8_t device_code, pdp8_tick_handler handler, void *context) {
    if (!cpu || device_code >= 64u) {
        return -1;
    }
    cpu->tick_handlers[device_code] = handler;
    cpu->tick_contexts[device_code] = context;
    return 0;
}

void pdp8_api_request_skip(pdp8_t *cpu) {
    if (!cpu) {
        return;
    }
    cpu->skip_pending = true;
}

void pdp8_api_set_switch_register(pdp8_t *cpu, uint16_t value) {
    if (!cpu) {
        return;
    }
    cpu->switch_register = mask_word(value);
}

uint16_t pdp8_api_get_switch_register(const pdp8_t *cpu) {
    return cpu ? cpu->switch_register : 0u;
}

int pdp8_api_is_halted(const pdp8_t *cpu) {
    return (cpu && cpu->halted) ? 1 : 0;
}

int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code) {
    if (!cpu) {
        return -1;
    }
    (void)device_code;  /* Parameter for logging/debugging purposes */
    cpu->interrupt_pending++;
    return 0;
}

int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu) {
    if (!cpu) {
        return -1;
    }
    return cpu->interrupt_pending;
}

int pdp8_api_clear_interrupt_pending(pdp8_t *cpu) {
    if (!cpu) {
        return -1;
    }
    if (cpu->interrupt_pending > 0) {
        cpu->interrupt_pending--;
        return 0;
    }
    return -1;
}

int pdp8_api_is_interrupt_enabled(const pdp8_t *cpu) {
    if (!cpu) {
        return -1;
    }
    return cpu->interrupt_enable ? 1 : 0;
}
