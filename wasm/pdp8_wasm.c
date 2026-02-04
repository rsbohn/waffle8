/* PDP-8 WebAssembly Interface
 * Provides JavaScript-callable functions to run the PDP-8 emulator in a browser
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <emscripten.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/emulator/kl8e_console.h"

/* Global state */
static pdp8_t *g_cpu = NULL;
static pdp8_kl8e_console_t *g_console = NULL;
static char *g_output_buffer = NULL;
static size_t g_output_len = 0;
static size_t g_output_capacity = 0;

/* Console output callback - accumulate output in buffer */
static void console_output_callback(uint8_t ch, void *context) {
    (void)context;
    
    if (g_output_len >= g_output_capacity) {
        size_t new_capacity = g_output_capacity ? g_output_capacity * 2 : 1024;
        char *new_buffer = realloc(g_output_buffer, new_capacity);
        if (!new_buffer) {
            return;
        }
        g_output_buffer = new_buffer;
        g_output_capacity = new_capacity;
    }
    
    g_output_buffer[g_output_len++] = (char)ch;
}

/* Embedded dull-boy ROM image (converted from S-record) */
static const uint16_t dullboy_rom[] = {
    0x0000, 0x07C0, 0x0ED9, 0x0269, 0x00D8, 0x00E8, 0x00EA, 0x9028,
    0x9086, 0x0C0D, 0x0840, 0x0A00, 0x00A5, 0x06A5, 0x0320, 0x00F8,
    0x0F0A, 0x0890, 0x0B93, 0x0813, 0x9B08, 0x0A50, 0x048B, 0x0A00,
    0x00A6, 0x0621, 0x0C95, 0x0AA6, 0x0226, 0x0C80, 0x0E93, 0x0B00,
    0x00AA, 0x02A7, 0x06AB, 0x02A8, 0x06A8, 0x0A80, 0x04A0, 0x0AA7,
    0x049E, 0x0A9B, 0x0B00, 0x0000, 0x0000, 0x0000, 0x0000, 0x00AC,
    0x00E0, 0x0F00, 0x0F00, 0x0041, 0x006C, 0x006C, 0x0020, 0x0077,
    0x006F, 0x0072, 0x006B, 0x0020, 0x0061, 0x006E, 0x0064, 0x0020,
    0x006E, 0x006F, 0x0020, 0x0070, 0x006C, 0x0061, 0x0079, 0x0020,
    0x006D, 0x0061, 0x006B, 0x0065, 0x0020, 0x004A, 0x0061, 0x0063,
    0x006B, 0x0020, 0x0061, 0x0020, 0x0064, 0x0075, 0x006C, 0x006C,
    0x0020, 0x0062, 0x006F, 0x0079, 0x002E, 0x000D, 0x000A, 0x0000,
    0x0000, 0x0C40
};

#define DULLBOY_ROM_SIZE (sizeof(dullboy_rom) / sizeof(dullboy_rom[0]))
#define DULLBOY_START_ADDRESS 0x0080

/* Initialize the emulator with dullboy program */
EMSCRIPTEN_KEEPALIVE
int pdp8_init(void) {
    if (g_cpu) {
        return 0; /* Already initialized */
    }
    
    /* Create CPU with host simulator board spec */
    const pdp8_board_spec *board = pdp8_board_host_simulator();
    g_cpu = pdp8_api_create_for_board(board);
    if (!g_cpu) {
        return -1;
    }
    
    /* Create and attach KL8E console with our output callback */
    g_console = pdp8_kl8e_console_create(NULL, NULL);
    if (!g_console) {
        pdp8_api_destroy(g_cpu);
        g_cpu = NULL;
        return -1;
    }
    
    if (pdp8_kl8e_console_set_output_callback(g_console, console_output_callback, NULL) != 0) {
        pdp8_kl8e_console_destroy(g_console);
        pdp8_api_destroy(g_cpu);
        g_console = NULL;
        g_cpu = NULL;
        return -1;
    }
    
    if (pdp8_kl8e_console_attach(g_cpu, g_console) != 0) {
        pdp8_kl8e_console_destroy(g_console);
        pdp8_api_destroy(g_cpu);
        g_console = NULL;
        g_cpu = NULL;
        return -1;
    }
    
    /* Load dullboy ROM image */
    if (pdp8_api_load(g_cpu, dullboy_rom, DULLBOY_ROM_SIZE, DULLBOY_START_ADDRESS) != 0) {
        pdp8_kl8e_console_destroy(g_console);
        pdp8_api_destroy(g_cpu);
        g_console = NULL;
        g_cpu = NULL;
        return -1;
    }
    
    /* Set PC to start of program */
    pdp8_api_set_pc(g_cpu, DULLBOY_START_ADDRESS);
    
    return 0;
}

/* Reset the emulator */
EMSCRIPTEN_KEEPALIVE
void pdp8_reset(void) {
    if (!g_cpu) {
        return;
    }
    
    pdp8_api_reset(g_cpu);
    pdp8_api_set_pc(g_cpu, DULLBOY_START_ADDRESS);
    
    /* Clear output buffer */
    g_output_len = 0;
}

/* Step the emulator for a given number of cycles */
EMSCRIPTEN_KEEPALIVE
int pdp8_step(int cycles) {
    if (!g_cpu || cycles <= 0) {
        return 0;
    }
    
    int executed = 0;
    for (int i = 0; i < cycles; i++) {
        if (pdp8_api_is_halted(g_cpu)) {
            break;
        }
        int result = pdp8_api_step(g_cpu);
        if (result == 0) {
            break;  /* Error or halt */
        }
        executed++;
    }
    
    return executed;
}

/* Check if CPU is halted */
EMSCRIPTEN_KEEPALIVE
int pdp8_is_halted(void) {
    if (!g_cpu) {
        return 1;
    }
    return pdp8_api_is_halted(g_cpu) ? 1 : 0;
}

/* Get program counter */
EMSCRIPTEN_KEEPALIVE
int pdp8_get_pc(void) {
    if (!g_cpu) {
        return 0;
    }
    return (int)pdp8_api_get_pc(g_cpu);
}

/* Get accumulator */
EMSCRIPTEN_KEEPALIVE
int pdp8_get_ac(void) {
    if (!g_cpu) {
        return 0;
    }
    return (int)pdp8_api_get_ac(g_cpu);
}

/* Get link register */
EMSCRIPTEN_KEEPALIVE
int pdp8_get_link(void) {
    if (!g_cpu) {
        return 0;
    }
    return (int)pdp8_api_get_link(g_cpu);
}

/* Get output buffer pointer (for JavaScript) */
EMSCRIPTEN_KEEPALIVE
const char *pdp8_get_output(void) {
    if (!g_output_buffer || g_output_len == 0) {
        return "";
    }
    
    /* Ensure null termination */
    if (g_output_len >= g_output_capacity) {
        char *new_buffer = realloc(g_output_buffer, g_output_capacity + 1);
        if (!new_buffer) {
            return "";
        }
        g_output_buffer = new_buffer;
        g_output_capacity++;
    }
    g_output_buffer[g_output_len] = '\0';
    
    return g_output_buffer;
}

/* Get output buffer length */
EMSCRIPTEN_KEEPALIVE
int pdp8_get_output_length(void) {
    return (int)g_output_len;
}

/* Clear output buffer */
EMSCRIPTEN_KEEPALIVE
void pdp8_clear_output(void) {
    g_output_len = 0;
}

/* Cleanup */
EMSCRIPTEN_KEEPALIVE
void pdp8_cleanup(void) {
    if (g_console) {
        pdp8_kl8e_console_destroy(g_console);
        g_console = NULL;
    }
    if (g_cpu) {
        pdp8_api_destroy(g_cpu);
        g_cpu = NULL;
    }
    if (g_output_buffer) {
        free(g_output_buffer);
        g_output_buffer = NULL;
        g_output_len = 0;
        g_output_capacity = 0;
    }
}
