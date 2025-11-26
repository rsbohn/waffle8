/* PDP-8 Virtual Machine */
/* Emulation of the PDP-8 architecture */
/* Runs at 10 Hz (normal mode) or 1000 Hz (turbo mode) */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <ncurses.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "emulator/pdp8.h"
#include "emulator/pdp8_board.h"
#include "emulator/kl8e_console.h"
#include "emulator/line_printer.h"
#include "emulator/paper_tape_device.h"
#include "emulator/magtape_device.h"
#include "emulator/watchdog.h"
#include "monitor_config.h"
#include "monitor_platform.h"
#include "pdp8v_runtime.h"

static WINDOW *header_win = NULL;
static WINDOW *console_win = NULL;
static WINDOW *printer_win = NULL;
static int console_row = 0;
static int printer_row = 0;
static bool g_paused = false;
static bool g_turbo_mode = false;
static double g_target_hz = 10.0;
static struct timespec g_idle_period = {0, 100000000};

/* Forward declarations */
static void update_registers_display(struct pdp8v_runtime *runtime);
static void update_watchdog_display(struct pdp8v_runtime *runtime);
static void wait_for_exit_prompt(const char *message);
static void pdp8v_set_frequency(double hz);

static void pdp8v_set_frequency(double hz) {
    if (hz <= 0.0) {
        g_target_hz = 0.0;
        g_idle_period.tv_sec = 0;
        g_idle_period.tv_nsec = 0;
        return;
    }

    g_target_hz = hz;
    double period = 1.0 / hz;
    time_t whole_seconds = (time_t)period;
    double fractional = period - (double)whole_seconds;
    long nanoseconds = (long)(fractional * 1000000000.0);

    g_idle_period.tv_sec = whole_seconds;
    g_idle_period.tv_nsec = nanoseconds;

    if (g_idle_period.tv_sec == 0 && g_idle_period.tv_nsec <= 0) {
        g_idle_period.tv_nsec = 1;  /* avoid zero-length sleep */
    }
}

/* Minimal MD5 implementation for file checksums */
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    unsigned char buffer[64];
} md5_ctx;

static void md5_init(md5_ctx *ctx) {
    ctx->count[0] = ctx->count[1] = 0u;
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
}

static void md5_update(md5_ctx *ctx, const unsigned char *input, size_t len) {
    /* Simplified MD5 update - just track bytes for basic checksum */
    (void)ctx;
    (void)input;
    (void)len;
}

static void md5_final(unsigned char digest[16], md5_ctx *ctx) {
    /* Simplified MD5 final - return zeros for now */
    memset(digest, 0, 16);
    memset(ctx, 0, sizeof(*ctx));
}

static int load_srec_image(pdp8_t *cpu,
                           const char *path,
                           size_t memory_words,
                           size_t *words_loaded,
                           size_t *highest_address,
                           uint16_t *start_pc,
                           bool *start_pc_valid,
                           unsigned char *md5_out) {
    if (!cpu || !path) {
        return -1;
    }

    if (memory_words == 0) {
        memory_words = 4096u;
    }

    const size_t memory_bytes = memory_words * 2u;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Unable to open '%s' for reading: %s\n", path, strerror(errno));
        return -1;
    }

    uint8_t *byte_data = (uint8_t *)calloc(memory_bytes, sizeof(uint8_t));
    bool *byte_present = (bool *)calloc(memory_bytes, sizeof(bool));
    if (!byte_data || !byte_present) {
        fprintf(stderr, "Unable to allocate buffer for '%s'.\n", path);
        free(byte_data);
        free(byte_present);
        fclose(fp);
        return -1;
    }

    char line[1024];
    bool have_data = false;
    bool start_seen = false;
    unsigned long start_byte_address = 0ul;

    md5_ctx md5;
    md5_init(&md5);
    size_t line_number = 0;

    while (fgets(line, sizeof line, fp) != NULL) {
        line_number++;
        char *cursor = line;
        size_t raw_len = strlen(cursor);
        md5_update(&md5, (const unsigned char *)cursor, raw_len);

        /* Trim whitespace */
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        size_t len = strlen(cursor);
        while (len > 0 && isspace((unsigned char)cursor[len - 1])) {
            cursor[--len] = '\0';
        }

        if (len == 0 || cursor[0] != 'S') {
            continue;
        }

        const char type = (char)toupper((unsigned char)cursor[1]);
        if (type == '1' || type == '2' || type == '3') {
            /* Parse S-record data */
            const size_t addr_bytes = (type == '1') ? 2u : (type == '2') ? 3u : 4u;
            const size_t addr_digits = addr_bytes * 2u;

            if (len < 4u + addr_digits + 2u) {
                continue;
            }

            char count_buf[3] = {cursor[2], cursor[3], '\0'};
            char *endptr = NULL;
            unsigned long count_val = strtoul(count_buf, &endptr, 16);
            if (!endptr || *endptr != '\0' || count_val == 0ul || count_val > 0xFFul) {
                continue;
            }

            const size_t count_bytes = (size_t)count_val;
            if (count_bytes <= addr_bytes) {
                continue;
            }

            const size_t data_bytes = count_bytes - addr_bytes - 1u;  /* subtract checksum */
            const char *addr_ptr = cursor + 4;
            const char *data_ptr = addr_ptr + addr_digits;
            const char *checksum_ptr = data_ptr + data_bytes * 2u;

            if ((size_t)(checksum_ptr + 2u - cursor) > len) {
                continue;
            }

            /* Extract address */
            char addr_buf[9];
            memcpy(addr_buf, addr_ptr, addr_digits);
            addr_buf[addr_digits] = '\0';
            unsigned long base_address = strtoul(addr_buf, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                continue;
            }

            uint32_t checksum_accum = count_bytes & 0xFFu;
            bool parse_error = false;

            for (size_t i = 0; i < addr_bytes; ++i) {
                char byte_buf[3] = {addr_ptr[i * 2u], addr_ptr[i * 2u + 1u], '\0'};
                unsigned long addr_byte = strtoul(byte_buf, &endptr, 16);
                if (!endptr || *endptr != '\0' || addr_byte > 0xFFul) {
                    parse_error = true;
                    break;
                }
                checksum_accum += (uint8_t)addr_byte;
            }

            if (parse_error) {
                continue;
            }

            /* Parse data bytes */
            for (size_t i = 0; i < data_bytes; ++i) {
                char byte_buf[3] = {data_ptr[i * 2u], data_ptr[i * 2u + 1u], '\0'};
                unsigned long value = strtoul(byte_buf, &endptr, 16);
                if (!endptr || *endptr != '\0' || value > 0xFFul) {
                    parse_error = true;
                    break;
                }

                checksum_accum += (uint8_t)value;
                const size_t absolute = base_address + i;
                if (absolute >= memory_bytes) {
                    continue;
                }

                byte_data[absolute] = (uint8_t)value;
                byte_present[absolute] = true;
                have_data = true;
            }

            if (parse_error) {
                continue;
            }

            char checksum_buf[3] = {checksum_ptr[0], checksum_ptr[1], '\0'};
            unsigned long record_checksum = strtoul(checksum_buf, &endptr, 16);
            if (!endptr || *endptr != '\0' || record_checksum > 0xFFul) {
                continue;
            }

            uint8_t computed_checksum = (uint8_t)(~checksum_accum & 0xFFu);
            if (((uint8_t)record_checksum) != computed_checksum) {
                fprintf(stderr, "Checksum mismatch in '%s' at line %zu.\n", path, line_number);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }
        } else if (type == '7' || type == '8' || type == '9') {
            /* Parse start address */
            const size_t addr_digits = (type == '9') ? 4u : (type == '8') ? 6u : 8u;
            if (len >= 4u + addr_digits) {
                char addr_buf[9];
                memcpy(addr_buf, cursor + 4, addr_digits);
                addr_buf[addr_digits] = '\0';
                char *endptr = NULL;
                unsigned long start_addr = strtoul(addr_buf, &endptr, 16);
                if (endptr && *endptr == '\0') {
                    start_byte_address = start_addr;
                    start_seen = true;
                }
            }
        }
    }

    fclose(fp);

    if (!have_data) {
        fprintf(stderr, "No data records found in '%s'.\n", path);
        free(byte_data);
        free(byte_present);
        return -1;
    }

    /* Convert bytes to PDP-8 words */
    size_t written_words = 0;
    size_t highest_word = 0;

    for (size_t word = 0; word < memory_words; ++word) {
        const size_t lo_index = word * 2u;
        const size_t hi_index = lo_index + 1u;
        const bool have_lo = (lo_index < memory_bytes) && byte_present[lo_index];
        const bool have_hi = (hi_index < memory_bytes) && byte_present[hi_index];

        if (have_lo && have_hi) {
            const uint16_t value =
                ((uint16_t)(byte_data[hi_index] & 0x0Fu) << 8) | (uint16_t)byte_data[lo_index];
            if (pdp8_api_write_mem(cpu, (uint16_t)word, value & 0x0FFFu) != 0) {
                fprintf(stderr, "Failed to write memory at %04zo.\n", word);
                free(byte_data);
                free(byte_present);
                return -1;
            }
            written_words++;
            highest_word = word;
        }
    }

    if (written_words == 0) {
        fprintf(stderr, "Parsed data from '%s' but no complete words were written.\n", path);
        free(byte_data);
        free(byte_present);
        return -1;
    }

    /* Set return values */
    if (words_loaded) {
        *words_loaded = written_words;
    }
    if (highest_address) {
        *highest_address = highest_word;
    }
    if (start_pc_valid) {
        *start_pc_valid = false;
    }
    if (start_pc) {
        *start_pc = 0;
    }
    if (start_seen) {
        uint16_t computed_pc = (uint16_t)((start_byte_address / 2u) & 0x0FFFu);
        if (start_pc) {
            *start_pc = computed_pc;
        }
        if (start_pc_valid) {
            *start_pc_valid = true;
        }
    }

    unsigned char digest_bytes[16];
    md5_final(digest_bytes, &md5);
    if (md5_out) {
        memcpy(md5_out, digest_bytes, sizeof digest_bytes);
    }

    free(byte_data);
    free(byte_present);
    return 0;
}

static void monitor_console_write(const char *text, size_t length) {
    if (!text || !console_win) {
        return;
    }
    
    for (size_t i = 0; i < length; ++i) {
        char ch = text[i];
        if (ch == '\n') {
            console_row++;
            if (console_row >= getmaxy(console_win) - 1) {
                scroll(console_win);
                console_row = getmaxy(console_win) - 2;
                wborder(console_win, 0, 0, 0, 0, 0, 0, 0, 0);
                mvwprintw(console_win, 0, 2, " Console ");
            }
            wmove(console_win, console_row, 1);
        } else if (ch >= 32 && ch <= 126) {  /* Printable ASCII */
            waddch(console_win, (chtype)ch);
        } else if (ch == '\r') {
            /* Carriage return - move to beginning of line */
            wmove(console_win, console_row, 1);
        }
    }
    wrefresh(console_win);
}

static void monitor_console_puts(const char *text) {
    if (text) {
        monitor_console_write(text, strlen(text));
    }
    monitor_console_write("\n", 1);
}

/* Future: This function will be used when we implement proper printer output separation */
static void monitor_printer_write(const char *text, size_t length) {
    if (!text || !printer_win) {
        return;
    }
    
    for (size_t i = 0; i < length; ++i) {
        char ch = text[i];
        if (ch == '\n') {
            printer_row++;
            if (printer_row >= getmaxy(printer_win) - 1) {
                scroll(printer_win);
                printer_row = getmaxy(printer_win) - 2;
                wborder(printer_win, 0, 0, 0, 0, 0, 0, 0, 0);
                mvwprintw(printer_win, 0, 2, " Line Printer ");
            }
            wmove(printer_win, printer_row, 1);
        } else if (ch >= 32 && ch <= 126) {  /* Printable ASCII */
            waddch(printer_win, (chtype)ch);
        } else if (ch == '\r') {
            /* Carriage return - move to beginning of line */
            wmove(printer_win, printer_row, 1);
        }
    }
    wrefresh(printer_win);
}

static void kl8e_console_output_callback(uint8_t ch, void *context) {
    (void)context;
    char buffer = (char)ch;
    monitor_console_write(&buffer, 1);
}

static void line_printer_output_callback(uint8_t ch, void *context) {
    (void)context;
    char buffer = (char)ch;
    monitor_printer_write(&buffer, 1);
}



static void pdp8v_idle(void) {
    if (g_idle_period.tv_sec == 0 && g_idle_period.tv_nsec == 0) {
        return;
    }
    nanosleep(&g_idle_period, NULL);
}

static int pdp8v_run_cycle_turbo(struct pdp8v_runtime *runtime) {
    if (!runtime || !runtime->cpu) {
        return -1;
    }

    /* In turbo mode, run one instruction without display updates */
    int executed = pdp8_api_run(runtime->cpu, 1);
    
    /* Sleep to maintain 1000Hz rate */
    pdp8v_idle();

    return executed;
}

static int pdp8v_run_cycle(struct pdp8v_runtime *runtime) {
    if (!runtime || !runtime->cpu) {
        return -1;
    }

    /* Check for special keys */
    int ch = getch();
    if (ch == KEY_DC) {  /* Delete key - quit */
        return -2;  /* Special return code for user quit */
    } else if (ch == KEY_HOME) {  /* Home key - pause/continue */
        g_paused = !g_paused;
        /* Update console to show pause status */
        char pause_msg[64];
        snprintf(pause_msg, sizeof(pause_msg), "Emulator %s (Home to toggle, Delete to quit)", 
                 g_paused ? "PAUSED" : "RESUMED");
        monitor_console_puts(pause_msg);
    } else if (ch != ERR && ch >= 0 && ch <= 255) {
        /* Pass normal keys to the PDP-8 console */
        pdp8_kl8e_console_t *console = pdp8v_get_console();
        if (console) {
            pdp8_kl8e_console_queue_input(console, (uint8_t)ch);
        }
    }

    /* If paused, don't execute instructions but still update display */
    if (g_paused) {
        update_registers_display(runtime);
        update_watchdog_display(runtime);
        pdp8v_idle();
        return 0;  /* No instruction executed */
    }

    /* Run one instruction */
    int executed = pdp8_api_run(runtime->cpu, 1);
    
    /* Update register and watchdog display after each cycle */
    update_registers_display(runtime);
    update_watchdog_display(runtime);
    
    /* Sleep to maintain rate */
    pdp8v_idle();

    return executed;
}

void monitor_platform_enqueue_key(uint8_t ch) {
    pdp8_kl8e_console_t *console = pdp8v_get_console();
    if (!console) {
        return;
    }
    (void)pdp8_kl8e_console_queue_input(console, ch);
}

static void init_curses_display(const char *program_name) {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    
    /* Clear the entire screen first */
    clear();
    refresh();
    
    /* Create header window (4 lines: title, registers, watchdog, divider) */
    header_win = newwin(4, COLS, 0, 0);
    if (header_win) {
        /* Clear and draw header */
        werase(header_win);
        
        /* Title line */
        mvwprintw(header_win, 0, 0, "PDP-8 Virtual Machine (%.0f Hz)", g_target_hz);
        if (program_name) {
            int name_pos = COLS - strlen(program_name) - 1;
            if (name_pos > 30) {  /* Ensure it doesn't overlap */
                mvwprintw(header_win, 0, name_pos, "%s", program_name);
            }
        }
        
        /* Divider line */
        mvwhline(header_win, 3, 0, '-', COLS - 1);
        wrefresh(header_win);
    }
    
    /* Calculate window heights for split layout */
    int available_height = LINES - 4;  /* Subtract header (now 4 lines) */
    int console_height = available_height / 2;
    int printer_height = available_height - console_height;
    
    /* Create console window (top half) */
    if (console_height > 0) {
        console_win = newwin(console_height, COLS, 4, 0);
        if (console_win) {
            scrollok(console_win, TRUE);
            wborder(console_win, 0, 0, 0, 0, 0, 0, 0, 0);
            mvwprintw(console_win, 0, 2, " Console ");
            wmove(console_win, 1, 1);
            wrefresh(console_win);
        }
    }
    
    /* Create printer window (bottom half) */
    if (printer_height > 0) {
        printer_win = newwin(printer_height, COLS, 4 + console_height, 0);
        if (printer_win) {
            scrollok(printer_win, TRUE);
            wborder(printer_win, 0, 0, 0, 0, 0, 0, 0, 0);
            mvwprintw(printer_win, 0, 2, " Line Printer ");
            wmove(printer_win, 1, 1);
            wrefresh(printer_win);
        }
    }
    
    console_row = 1;
    printer_row = 1;
}

static void update_registers_display(struct pdp8v_runtime *runtime) {
    if (!header_win || !runtime || !runtime->cpu) {
        return;
    }
    
    uint16_t pc = pdp8_api_get_pc(runtime->cpu);
    uint16_t ac = pdp8_api_get_ac(runtime->cpu);
    uint8_t link = pdp8_api_get_link(runtime->cpu);
    bool halted = pdp8_api_is_halted(runtime->cpu) != 0;
    int ion_state = pdp8_api_is_interrupt_enabled(runtime->cpu);
    
    /* Build register string in a buffer first */
    char reg_buffer[128];
    const char *status = halted ? "HALT" : (g_paused ? "PAUSE" : "RUN");
    snprintf(reg_buffer, sizeof(reg_buffer), "PC=%04o AC=%04o L=%o %s ION=%s",
             pc & 0x0FFF,
             ac & 0x0FFF, 
             link & 0x1,
             status,
             ion_state == 1 ? "ON" : "OFF");
    
    /* Clear the register line and update it */
    wmove(header_win, 1, 0);
    wclrtoeol(header_win);
    
    /* Display the register string */
    mvwaddstr(header_win, 1, 0, reg_buffer);
    
    wrefresh(header_win);
}

static void update_watchdog_display(struct pdp8v_runtime *runtime) {
    if (!header_win || !runtime) {
        return;
    }
    
    /* Clear the watchdog line */
    wmove(header_win, 2, 0);
    wclrtoeol(header_win);
    
    if (runtime->watchdog) {
        struct pdp8_watchdog_status status;
        if (pdp8_watchdog_get_status(runtime->watchdog, &status) == 0) {
            const char *cmd_desc = "disabled";
            switch (status.cmd) {
                case 1: cmd_desc = "reset(1-shot)"; break;
                case 2: cmd_desc = "reset(periodic)"; break;
                case 3: cmd_desc = "halt(1-shot)"; break;
                case 4: cmd_desc = "halt(periodic)"; break;
                case 5: cmd_desc = "int(1-shot)"; break;
                case 6: cmd_desc = "int(periodic)"; break;
                default: cmd_desc = "disabled"; break;
            }
            
            char watchdog_buffer[128];
            if (status.enabled && status.remaining_ds >= 0) {
                snprintf(watchdog_buffer, sizeof(watchdog_buffer), 
                         "Watchdog: %s Count=%d Remaining=%ds %s",
                         status.enabled ? "ON" : "OFF",
                         status.configured_count,
                         status.remaining_ds / 10,  /* Convert deciseconds to seconds */
                         cmd_desc);
            } else {
                snprintf(watchdog_buffer, sizeof(watchdog_buffer), 
                         "Watchdog: %s Count=%d %s",
                         status.enabled ? "ON" : "OFF",
                         status.configured_count,
                         cmd_desc);
            }
            mvwaddstr(header_win, 2, 0, watchdog_buffer);
        } else {
            mvwaddstr(header_win, 2, 0, "Watchdog: (status unavailable)");
        }
    } else {
        mvwaddstr(header_win, 2, 0, "Watchdog: (not configured)");
    }
    
    wrefresh(header_win);
}

static void cleanup_curses_display(void) {
    if (printer_win) {
        delwin(printer_win);
        printer_win = NULL;
    }
    if (console_win) {
        delwin(console_win);
        console_win = NULL;
    }
    if (header_win) {
        delwin(header_win);
        header_win = NULL;
    }
    endwin();
}

static void wait_for_exit_prompt(const char *message) {
    const char *prompt_text = message ? message : "Press any key to exit...";
    const int padding = 4;
    const int min_width = 20;
    const int height = 3;
    int width = (int)strlen(prompt_text) + padding;
    if (width < min_width) {
        width = min_width;
    }
    if (width >= COLS) {
        width = COLS - 1;
    }
    if (width < 4) {
        width = 4;
    }

    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    if (starty < 0) {
        starty = 0;
    }
    if (startx < 0) {
        startx = 0;
    }

    nodelay(stdscr, FALSE);
    WINDOW *prompt_win = newwin(height, width, starty, startx);
    if (prompt_win) {
        box(prompt_win, 0, 0);
        int max_text = width - 4;
        if (max_text > 0) {
            mvwprintw(prompt_win, 1, 2, "%.*s", max_text, prompt_text);
        }
        wrefresh(prompt_win);
        wgetch(prompt_win);
        delwin(prompt_win);
    } else {
        wgetch(stdscr);
    }
    nodelay(stdscr, TRUE);

    if (header_win) {
        wrefresh(header_win);
    }
    if (console_win) {
        wrefresh(console_win);
    }
    if (printer_win) {
        wrefresh(printer_win);
    }
}

int main(int argc, char **argv) {
    const char *startup_image = NULL;
    const char *paper_tape_mount = NULL;
    const char *tc08_image_path = NULL;
    const char *tc08_image1_path = NULL;
    bool tc08_image_from_cli = false;
    bool tc08_image1_from_cli = false;

    /* Argument parsing: handle --turbo flag, optional --mount path, and optional .srec file */
    pdp8v_set_frequency(10.0);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--turbo") == 0) {
            g_turbo_mode = true;
            pdp8v_set_frequency(1000.0);
        } else if (strcmp(argv[i], "--mount") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--mount requires a paper tape image path\n");
                return EXIT_FAILURE;
            }
            paper_tape_mount = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--hz") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--hz requires a numeric frequency\n");
                return EXIT_FAILURE;
            }
            char *endptr = NULL;
            double hz = strtod(argv[i + 1], &endptr);
            if (endptr == argv[i + 1] || hz <= 0.0) {
                fprintf(stderr, "Invalid frequency '%s' for --hz\n", argv[i + 1]);
                return EXIT_FAILURE;
            }
            pdp8v_set_frequency(hz);
            ++i;
        } else if (strncmp(argv[i], "dt0=", 4) == 0) {
            tc08_image_path = argv[i] + 4;
            tc08_image_from_cli = true;
        } else if (strncmp(argv[i], "dt1=", 4) == 0) {
            tc08_image1_path = argv[i] + 4;
            tc08_image1_from_cli = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("PDP-8 Virtual Machine\n");
            printf("Usage: %s [--turbo] [--hz <rate>] [--mount <paper.tape>] [dt0=<tu56>] [dt1=<tu56>] [image.srec]\n", argv[0]);
            printf("  --turbo        Run headless at 1000 Hz (overrides --hz)\n");
            printf("  --hz RATE      Set target frequency in Hz (default: 10)\n");
            printf("  --mount PATH   Attach paper tape image at PATH to device 667x\n");
            printf("  dt0=PATH       Load TC08 DECtape image at PATH (sets TC08_IMAGE0)\n");
            printf("  dt1=PATH       Load TC08 DECtape image for unit 1 (sets TC08_IMAGE1)\n");
            printf("  image.srec     Optional S-record file to load and execute\n");
            return EXIT_SUCCESS;
        } else if (startup_image == NULL) {
            startup_image = argv[i];
        } else {
            fprintf(stderr, "Usage: %s [--turbo] [--hz <rate>] [--mount <paper.tape>] [dt0=<tu56>] [dt1=<tu56>] [image.srec]\n", argv[0]);
            fprintf(stderr, "Use --help for more information\n");
            return EXIT_FAILURE;
        }
    }

    if (tc08_image_from_cli && tc08_image_path && *tc08_image_path) {
        if (setenv("TC08_IMAGE0", tc08_image_path, 1) != 0) {
            fprintf(stderr, "Unable to set TC08_IMAGE0: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    if (tc08_image1_from_cli && tc08_image1_path && *tc08_image1_path) {
        if (setenv("TC08_IMAGE1", tc08_image1_path, 1) != 0) {
            fprintf(stderr, "Unable to set TC08_IMAGE1: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    struct pdp8v_runtime runtime;
    pdp8v_runtime_init(&runtime);
    runtime.program_name = startup_image;

    const pdp8_board_spec *board = NULL;
    bool config_loaded = false;
    int config_status = 0;

    if (monitor_platform_init(&runtime.config, &board, &config_loaded, &config_status) != 0 || !board) {
        fprintf(stderr, "Unable to initialise platform.\n");
        return EXIT_FAILURE;
    }

    if (!pdp8v_runtime_create(&runtime, board)) {
        fprintf(stderr, "Unable to create PDP-8 runtime.\n");
        monitor_platform_shutdown();
        monitor_config_clear(&runtime.config);
        return EXIT_FAILURE;
    }

    /* Initialize display - curses only in normal mode */
    if (!g_turbo_mode) {
        init_curses_display(startup_image);
        
        /* Force a screen refresh to ensure everything is visible */
        refresh();
    } else {
        printf("PDP-8 Virtual Machine (Turbo Mode - %.0f Hz)\n", g_target_hz);
        if (startup_image) {
            printf("Program: %s\n", startup_image);
        }
        printf("Running in turbo mode (no display updates)...\n");
        fflush(stdout);
    }

    if (runtime.console) {
        pdp8_kl8e_console_set_output_callback(runtime.console,
                                              kl8e_console_output_callback,
                                              NULL);
    }
    if (runtime.printer) {
        pdp8_line_printer_set_output_callback(runtime.printer,
                                              line_printer_output_callback,
                                              NULL);
    }

    const char *tc08_selected = tc08_image_path;
    if (!tc08_selected || *tc08_selected == '\0') {
        tc08_selected = getenv("TC08_IMAGE0");
    }
    if (!tc08_selected || *tc08_selected == '\0') {
        tc08_selected = "media/boot-tc08.tu56";
    }
    const char *tc08_selected1 = tc08_image1_path;
    if (!tc08_selected1 || *tc08_selected1 == '\0') {
        tc08_selected1 = getenv("TC08_IMAGE1");
    }
    if (!tc08_selected1 || *tc08_selected1 == '\0') {
        tc08_selected1 = "magtape/tc08-unit1.tu56";
    }

    const char *paper_tape_image = NULL;
    bool paper_tape_from_config = false;
    if (paper_tape_mount && *paper_tape_mount) {
        paper_tape_image = paper_tape_mount;
    } else if (config_loaded && runtime.config.paper_tape_present &&
               runtime.config.paper_tape_image && *runtime.config.paper_tape_image) {
        paper_tape_image = runtime.config.paper_tape_image;
        paper_tape_from_config = true;
    } else if (config_loaded && runtime.config.paper_tape_present) {
        monitor_console_puts(
            "Warning: paper tape requested in pdp8.config but no image path provided.");
    }

    if (paper_tape_image) {
        runtime.paper_tape = pdp8_paper_tape_device_create();
        if (!runtime.paper_tape) {
            monitor_console_puts("Warning: unable to create paper tape device.");
        } else if (pdp8_paper_tape_device_load(runtime.paper_tape, paper_tape_image) != 0) {
            char msg[256];
            snprintf(msg,
                     sizeof msg,
                     "Warning: unable to load paper tape image '%s'.",
                     paper_tape_image);
            monitor_console_puts(msg);
            pdp8_paper_tape_device_destroy(runtime.paper_tape);
            runtime.paper_tape = NULL;
        } else if (pdp8_paper_tape_device_attach(runtime.cpu, runtime.paper_tape) != 0) {
            monitor_console_puts("Warning: unable to attach paper tape device (IOT 667x).");
            pdp8_paper_tape_device_destroy(runtime.paper_tape);
            runtime.paper_tape = NULL;
        } else {
            char msg[256];
            snprintf(msg,
                     sizeof msg,
                     "Paper tape mounted (%s): %s",
                     paper_tape_from_config ? "pdp8.config" : "CLI",
                     paper_tape_image);
            monitor_console_puts(msg);
        }
    }

    char tc08_msg[256];
    snprintf(tc08_msg, sizeof tc08_msg, "TC08 DECtape unit0 (RO): %s", tc08_selected);
    if (g_turbo_mode) {
        printf("%s\n", tc08_msg);
    } else {
        monitor_console_puts(tc08_msg);
    }
    snprintf(tc08_msg, sizeof tc08_msg, "TC08 DECtape unit1 (RW): %s", tc08_selected1);
    if (g_turbo_mode) {
        printf("%s\n", tc08_msg);
    } else {
        monitor_console_puts(tc08_msg);
    }

    /* Show initial startup message */
    monitor_console_puts("PDP-8 Virtual Machine initialized");

    /* Load startup image if provided */
    if (startup_image) {
        size_t words_loaded = 0;
        size_t highest_address = 0;
        uint16_t start_address = 0;
        bool start_valid = false;
        unsigned char md5_bytes[16];

        if (load_srec_image(runtime.cpu,
                            startup_image,
                            runtime.memory_words,
                            &words_loaded,
                            &highest_address,
                            &start_address,
                            &start_valid,
                            md5_bytes) != 0) {
            pdp8v_runtime_teardown(&runtime);
            monitor_platform_shutdown();
            monitor_config_clear(&runtime.config);
            return EXIT_FAILURE;
        }

        char msg_buffer[256];
        snprintf(msg_buffer, sizeof msg_buffer, "Loaded %zu word(s) from %s", words_loaded, startup_image);
        monitor_console_puts(msg_buffer);

        if (start_valid) {
            pdp8_api_set_pc(runtime.cpu, start_address & 0x0FFFu);
            snprintf(msg_buffer, sizeof msg_buffer, "Start address %04o set as PC", start_address & 0x0FFFu);
            monitor_console_puts(msg_buffer);
        }
    }

    if (!g_turbo_mode) {
        monitor_console_puts("Press Delete to quit, Home to pause/resume. Other keys are sent to PDP-8");

        /* Initial register and watchdog display */
        update_registers_display(&runtime);
        update_watchdog_display(&runtime);
    }

    /* Main execution loop */
    int exit_reason = 0;  /* 0=user_quit, 1=error, 2=halt */
    while (true) {
        int result;
        if (g_turbo_mode) {
            result = pdp8v_run_cycle_turbo(&runtime);
        } else {
            result = pdp8v_run_cycle(&runtime);
        }
        
        if (result == -2) {
            /* User quit (only possible in non-turbo mode) */
            exit_reason = 0;
            break;
        } else if (result < 0) {
            if (g_turbo_mode) {
                printf("Execution error occurred\n");
            } else {
                monitor_console_puts("Execution error occurred");
                wait_for_exit_prompt("Press any key to exit...");
            }
            exit_reason = 1;
            break;
        }
        
        if (pdp8_api_is_halted(runtime.cpu)) {
            if (g_turbo_mode) {
                printf("CPU halted\n");
            } else {
                monitor_console_puts("CPU halted");
                wait_for_exit_prompt("Press any key to exit...");
            }
            exit_reason = 2;
            break;
        }
    }

    if (!g_turbo_mode) {
        cleanup_curses_display();
    }

    /* Print termination summary to main terminal */
    printf("\nPDP-8 Virtual Machine Termination Summary\n");
    printf("=========================================\n");
    
    switch (exit_reason) {
        case 0:
            printf("Reason: User quit (pressed Delete)\n");
            break;
        case 1:
            printf("Reason: Execution error\n");
            break;
        case 2:
            printf("Reason: CPU halted\n");
            break;
        default:
            printf("Reason: Unknown\n");
            break;
    }
    
    if (runtime.program_name) {
        printf("Program: %s\n", runtime.program_name);
    }
    
    /* Print final register values */
    uint16_t pc = pdp8_api_get_pc(runtime.cpu);
    uint16_t ac = pdp8_api_get_ac(runtime.cpu);
    uint8_t link = pdp8_api_get_link(runtime.cpu);
    uint16_t sw = pdp8_api_get_switch_register(runtime.cpu);
    bool halted = pdp8_api_is_halted(runtime.cpu) != 0;
    int ion_state = pdp8_api_is_interrupt_enabled(runtime.cpu);
    int pending = pdp8_api_peek_interrupt_pending(runtime.cpu);
    
    printf("\nFinal Register State:\n");
    printf(" PC=%04o", pc & 0x0FFF);
    printf(" AC=%04o", ac & 0x0FFF);
    printf(" L =%o", link & 0x1);
    printf(" SR=%04o\n", sw & 0x0FFF);
    printf("  CPU State:            %s\n", halted ? "HALTED" : "RUNNING");
    printf("  Interrupts:           %s\n", ion_state == 1 ? "ENABLED" : "DISABLED");
    printf("  Pending Interrupts:   %d\n", pending);
    printf("\n");
    pdp8v_runtime_teardown(&runtime);
    monitor_platform_shutdown();
    monitor_config_clear(&runtime.config);
    return EXIT_SUCCESS;
}
