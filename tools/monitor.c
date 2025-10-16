#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/emulator/line_printer.h"
#include "../src/emulator/kl8e_console.h"
#include "../src/emulator/paper_tape_device.h"

struct monitor_config {
    bool kl8e_present;
    char *kl8e_keyboard_iot;
    char *kl8e_keyboard_input;
    char *kl8e_teleprinter_iot;
    char *kl8e_teleprinter_output;

    bool line_printer_present;
    char *line_printer_iot;
    char *line_printer_output;
    int line_printer_column_limit;

    bool paper_tape_present;
    char *paper_tape_iot;
    char *paper_tape_image;
};

static void monitor_config_init(struct monitor_config *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->line_printer_column_limit = 132;
}

static void monitor_config_clear(struct monitor_config *config) {
    if (!config) {
        return;
    }
    free(config->kl8e_keyboard_iot);
    free(config->kl8e_keyboard_input);
    free(config->kl8e_teleprinter_iot);
    free(config->kl8e_teleprinter_output);
    free(config->line_printer_iot);
    free(config->line_printer_output);
    free(config->paper_tape_iot);
    free(config->paper_tape_image);
    monitor_config_init(config);
}

static int monitor_config_set_string(char **slot, const char *value) {
    if (!slot) {
        return -1;
    }
    char *copy = NULL;
    if (value) {
        size_t len = strlen(value) + 1u;
        copy = (char *)malloc(len);
        if (!copy) {
            errno = ENOMEM;
            return -1;
        }
        memcpy(copy, value, len);
    }
    free(*slot);
    *slot = copy;
    return 0;
}

static char *trim_whitespace(char *text) {
    if (!text) {
        return NULL;
    }
    while (*text && isspace((unsigned char)*text)) {
        ++text;
    }
    if (*text == '\0') {
        return text;
    }
    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        --end;
    }
    end[1] = '\0';
    return text;
}

static int monitor_config_load(const char *path, struct monitor_config *config) {
    if (!path || !config) {
        return -1;
    }

    monitor_config_init(config);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return errno == ENOENT ? 1 : -1;
    }

    char line[512];
    char current_device[64] = {0};
    while (fgets(line, sizeof line, fp) != NULL) {
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }
        char *trimmed = trim_whitespace(line);
        if (!trimmed || *trimmed == '\0') {
            continue;
        }

        if (current_device[0] == '\0') {
            if (strncmp(trimmed, "device", 6) != 0 || !isspace((unsigned char)trimmed[6])) {
                continue;
            }
            char *cursor = trimmed + 6;
            while (*cursor && isspace((unsigned char)*cursor)) {
                ++cursor;
            }
            size_t name_len = 0;
            while (cursor[name_len] && !isspace((unsigned char)cursor[name_len]) && cursor[name_len] != '{') {
                if (name_len + 1 >= sizeof current_device) {
                    break;
                }
                current_device[name_len] = cursor[name_len];
                ++name_len;
            }
            current_device[name_len] = '\0';
            cursor += name_len;
            while (*cursor && isspace((unsigned char)*cursor)) {
                ++cursor;
            }
            if (*cursor != '{') {
                current_device[0] = '\0';
                continue;
            }
            continue;
        }

        if (strcmp(trimmed, "}") == 0) {
            current_device[0] = '\0';
            continue;
        }

        char *equals = strchr(trimmed, '=');
        if (!equals) {
            continue;
        }
        *equals = '\0';
        char *key = trim_whitespace(trimmed);
        char *value = trim_whitespace(equals + 1);
        if (!key || !value || *key == '\0' || *value == '\0') {
            continue;
        }

        if (strcmp(current_device, "kl8e_console") == 0) {
            config->kl8e_present = true;
            if (strcmp(key, "keyboard_iot") == 0) {
                if (monitor_config_set_string(&config->kl8e_keyboard_iot, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "teleprinter_iot") == 0) {
                if (monitor_config_set_string(&config->kl8e_teleprinter_iot, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "keyboard_input") == 0) {
                if (monitor_config_set_string(&config->kl8e_keyboard_input, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "teleprinter_output") == 0) {
                if (monitor_config_set_string(&config->kl8e_teleprinter_output, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            }
        } else if (strcmp(current_device, "line_printer") == 0) {
            config->line_printer_present = true;
            if (strcmp(key, "iot") == 0) {
                if (monitor_config_set_string(&config->line_printer_iot, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "output") == 0) {
                if (monitor_config_set_string(&config->line_printer_output, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "column_limit") == 0) {
                long parsed = strtol(value, NULL, 10);
                if (parsed > 0 && parsed <= INT32_MAX) {
                    config->line_printer_column_limit = (int)parsed;
                }
            }
        } else if (strcmp(current_device, "paper_tape") == 0) {
            config->paper_tape_present = true;
            if (strcmp(key, "iot") == 0) {
                if (monitor_config_set_string(&config->paper_tape_iot, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "image") == 0) {
                if (monitor_config_set_string(&config->paper_tape_image, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            }
        }
    }

    fclose(fp);
    current_device[0] = '\0';
    return 0;
}

static void show_devices(const struct monitor_config *config,
                         bool config_loaded,
                         const pdp8_paper_tape_device_t *paper_tape_device) {
    const char *keyboard_iot = (config && config->kl8e_keyboard_iot) ? config->kl8e_keyboard_iot : "603x";
    const char *keyboard_input = (config && config->kl8e_keyboard_input) ? config->kl8e_keyboard_input : "stdin";
    const char *teleprinter_iot =
        (config && config->kl8e_teleprinter_iot) ? config->kl8e_teleprinter_iot : "604x";
    const char *teleprinter_output =
        (config && config->kl8e_teleprinter_output) ? config->kl8e_teleprinter_output : "stdout";

    const char *line_printer_iot =
        (config && config->line_printer_iot) ? config->line_printer_iot : "660x";
    const char *line_printer_output =
        (config && config->line_printer_output) ? config->line_printer_output : "stdout";
    int line_printer_columns =
        (config && config->line_printer_column_limit > 0) ? config->line_printer_column_limit : 132;

    const char *paper_tape_iot =
        (config && config->paper_tape_iot) ? config->paper_tape_iot : "667x";
    const char *paper_tape_image =
        (config && config->paper_tape_image) ? config->paper_tape_image : "(unconfigured)";
    const char *paper_tape_label =
        paper_tape_device ? pdp8_paper_tape_device_label(paper_tape_device) : NULL;

    printf("Configuration source: %s\n", config_loaded ? "pdp8.config" : "built-in defaults");

    puts("Devices:");
    printf("  KL8E console\n");
    printf("    keyboard IOT     : %s\n", keyboard_iot);
    printf("    teleprinter IOT  : %s\n", teleprinter_iot);
    printf("    keyboard input   : %s\n", keyboard_input);
    printf("    teleprinter output: %s\n", teleprinter_output);

    printf("  Line printer\n");
    printf("    IOT              : %s\n", line_printer_iot);
    printf("    output           : %s\n", line_printer_output);
    printf("    column limit     : %d\n", line_printer_columns);

    printf("  Paper tape\n");
    printf("    IOT              : %s\n", paper_tape_iot);
    if (paper_tape_device) {
        printf("    image            : %s\n", paper_tape_image);
        printf("    label            : %s\n", paper_tape_label ? paper_tape_label : "(none)");
    } else {
        printf("    image            : %s\n", paper_tape_image);
        printf("    status           : (not attached)\n");
    }
}

static int parse_number(const char *token, long *value) {
    if (!token || !*token) {
        return -1;
    }

    int base = 8;
    const char *ptr = token;

    if (token[0] == '#') {
        base = 10;
        ptr = token + 1;
    } else if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        base = 16;
        ptr = token + 2;
    }

    char *end = NULL;
    long parsed = strtol(ptr, &end, base);
    if (!end || *end != '\0') {
        return -1;
    }

    *value = parsed;
    return 0;
}

static int load_srec_image(pdp8_t *cpu,
                           const char *path,
                           size_t memory_words,
                           size_t *words_loaded,
                           size_t *highest_address,
                           uint16_t *start_pc,
                           bool *start_pc_valid) {
    if (!cpu || !path) {
        return -1;
    }

    if (memory_words == 0) {
        memory_words = 4096u;
    }

    const size_t memory_bytes = memory_words * 2u;

    FILE *fp = fopen(path, "r");
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

    while (fgets(line, sizeof line, fp) != NULL) {
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }

        size_t len = strlen(cursor);
        while (len > 0 && isspace((unsigned char)cursor[len - 1])) {
            cursor[--len] = '\0';
        }

        if (len == 0 || cursor[0] == '\0') {
            continue;
        }

        if (cursor[0] != 'S' && cursor[0] != 's') {
            continue;
        }

        const char type = (char)toupper((unsigned char)cursor[1]);
        if (type == '\0') {
            continue;
        }

        if (type == '1' || type == '2' || type == '3') {
            const size_t addr_digits = (type == '1') ? 4u : (type == '2') ? 6u : 8u;
            const size_t addr_bytes = addr_digits / 2u;

            if (len < 4u + addr_digits + 2u) {
                fprintf(stderr, "Malformed S-record line (too short): %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            char count_str[3] = {cursor[2], cursor[3], '\0'};
            char *endptr = NULL;
            unsigned long count = strtoul(count_str, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                fprintf(stderr, "Malformed byte count in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            char addr_buf[9];
            memcpy(addr_buf, cursor + 4, addr_digits);
            addr_buf[addr_digits] = '\0';
            endptr = NULL;
            unsigned long base_address = strtoul(addr_buf, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                fprintf(stderr, "Malformed address in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            const size_t checksum_offset = len - 2u;
            if (checksum_offset < 4u + addr_digits) {
                fprintf(stderr, "Malformed data section in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            const char *data_ptr = cursor + 4 + addr_digits;
            const size_t data_hex_len = checksum_offset - (size_t)(data_ptr - cursor);
            if (data_hex_len % 2u != 0) {
                fprintf(stderr, "Odd data length in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            const size_t data_bytes = data_hex_len / 2u;
            if (count != data_bytes + addr_bytes + 1u) {
                fprintf(stderr, "Count mismatch for line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            for (size_t i = 0; i < data_bytes; ++i) {
                char byte_buf[3] = {data_ptr[i * 2u], data_ptr[i * 2u + 1u], '\0'};
                endptr = NULL;
                unsigned long value = strtoul(byte_buf, &endptr, 16);
                if (!endptr || *endptr != '\0' || value > 0xFFul) {
                    fprintf(stderr, "Malformed data byte in line: %s\n", cursor);
                    free(byte_data);
                    free(byte_present);
                    fclose(fp);
                    return -1;
                }

                const size_t absolute = base_address + i;
                if (absolute >= memory_bytes) {
                    fprintf(stderr,
                            "Warning: S-record byte address 0x%lX exceeds memory (max 0x%llX). Skipping.\n",
                            base_address + i,
                            memory_bytes ? (unsigned long long)(memory_bytes - 1u) : 0ull);
                    continue;
                }

                byte_data[absolute] = (uint8_t)value;
                byte_present[absolute] = true;
                have_data = true;
            }
        } else if (type == '7' || type == '8' || type == '9') {
            const size_t addr_digits = (type == '9') ? 4u : (type == '8') ? 6u : 8u;
            if (len < 4u + addr_digits) {
                continue;
            }
            char addr_buf[9];
            if (addr_digits >= sizeof addr_buf) {
                continue;
            }
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

    fclose(fp);
    fp = NULL;

    if (!have_data) {
        fprintf(stderr, "No data records found in '%s'.\n", path);
        free(byte_data);
        free(byte_present);
        return -1;
    }

    size_t written_words = 0;
    size_t highest_word = 0;
    bool encountered_partial = false;

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
        } else if (have_lo || have_hi) {
            encountered_partial = true;
        }
    }

    if (written_words == 0) {
        fprintf(stderr, "Parsed data from '%s' but no complete words were written.\n", path);
        free(byte_data);
        free(byte_present);
        return -1;
    }

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

    if (encountered_partial) {
        fprintf(stderr,
                "Warning: Incomplete word(s) encountered while reading '%s'; skipped those entries.\n",
                path);
    }

    free(byte_data);
    free(byte_present);
    return 0;
}

static void print_help(void) {
    puts("Commands:");
    puts("  mem <addr> [count]         Dump memory words (octal).");
    puts("  load <addr> <w0> [w1 ...]  Write consecutive words starting at addr.");
    puts("  run <addr> <cycles>        Set PC to addr then execute cycles.");
    puts("  regs                       Show PC, AC, link, switch register.");
    puts("  save <file>                Write RAM image to file.");
    puts("  restore <file>             Load RAM image from file.");
    puts("  read <file>                Load Motorola S-record image and apply start address.");
    puts("  reset                      Clear state and reload board ROM.");
    puts("  show devices               List attached peripherals and configuration.");
    puts("  help                       Show this help.");
    puts("  quit                       Exit monitor.");
    puts("Notes: numbers default to octal; prefix with '#' for decimal or 0x for hex.");
}

static void pump_console_input(pdp8_kl8e_console_t *console, FILE *stream) {
    if (!console || !stream) {
        return;
    }

    while (true) {
        errno = 0;
        int ch = fgetc(stream);
        if (ch == EOF) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                clearerr(stream);
            } else if (feof(stream)) {
                clearerr(stream);
            }
            break;
        }

        if (ch == '\n') {
            ch = '\r';
        }

        (void)pdp8_kl8e_console_queue_input(console, (uint8_t)(ch & 0x7F));
    }
}

#define MONITOR_RUN_SLICE 2000u

static int run_with_console(pdp8_t *cpu, pdp8_kl8e_console_t *console, size_t cycles, FILE *input_stream) {
    if (!cpu) {
        return -1;
    }

    int input_fd = -1;
    int original_flags = -1;
    if (console && input_stream) {
        input_fd = fileno(input_stream);
        if (input_fd >= 0) {
            original_flags = fcntl(input_fd, F_GETFL, 0);
            if (original_flags != -1) {
                if (fcntl(input_fd, F_SETFL, original_flags | O_NONBLOCK) == -1) {
                    original_flags = -1;
                }
            }
        }
    }

    size_t remaining = cycles;
    int total_executed = 0;

    while (remaining > 0u) {
        pump_console_input(console, input_stream);

        size_t request = remaining > MONITOR_RUN_SLICE ? MONITOR_RUN_SLICE : remaining;
        int executed = pdp8_api_run(cpu, request);
        if (executed <= 0) {
            if (executed < 0) {
                total_executed = executed;
            }
            break;
        }

        total_executed += executed;
        remaining -= (size_t)executed;

        if ((size_t)executed < request || pdp8_api_is_halted(cpu)) {
            break;
        }
    }

    pump_console_input(console, input_stream);

    if (input_fd >= 0 && original_flags != -1) {
        fcntl(input_fd, F_SETFL, original_flags);
    }

    return total_executed;
}

int main(void) {
    int exit_code = EXIT_SUCCESS;
    const pdp8_board_spec *board = pdp8_board_host_simulator();
    if (!board) {
        fprintf(stderr, "Unable to fetch host simulator board spec.\n");
        return EXIT_FAILURE;
    }

    pdp8_t *cpu = pdp8_api_create_for_board(board);
    if (!cpu) {
        fprintf(stderr, "Unable to create PDP-8 instance.\n");
        return EXIT_FAILURE;
    }

    struct monitor_config config;
    bool config_loaded = false;
    int config_status = monitor_config_load("pdp8.config", &config);
    if (config_status == 0) {
        config_loaded = true;
    } else {
        monitor_config_init(&config);
        if (config_status < 0) {
            fprintf(stderr, "Warning: failed to read pdp8.config (%s). Using built-in defaults.\n",
                    strerror(errno));
        }
    }

    FILE *console_input_stream = stdin;
    FILE *console_output_stream = stdout;
    bool close_console_input = false;
    bool close_console_output = false;

    if (config_loaded && config.kl8e_present) {
        if (config.kl8e_keyboard_input && strcmp(config.kl8e_keyboard_input, "stdin") != 0) {
            FILE *custom = fopen(config.kl8e_keyboard_input, "r");
            if (!custom) {
                fprintf(stderr,
                        "Warning: unable to open KL8E keyboard input '%s': %s. Falling back to stdin.\n",
                        config.kl8e_keyboard_input, strerror(errno));
            } else {
                console_input_stream = custom;
                close_console_input = true;
            }
        }

        if (config.kl8e_teleprinter_output &&
            strcmp(config.kl8e_teleprinter_output, "stdout") != 0) {
            FILE *custom = NULL;
            if (strcmp(config.kl8e_teleprinter_output, "stderr") == 0) {
                custom = stderr;
            } else {
                custom = fopen(config.kl8e_teleprinter_output, "a");
                if (!custom) {
                    fprintf(stderr,
                            "Warning: unable to open KL8E teleprinter output '%s': %s. "
                            "Falling back to stdout.\n",
                            config.kl8e_teleprinter_output, strerror(errno));
                } else {
                    close_console_output = true;
                }
            }
            if (custom) {
                console_output_stream = custom;
            }
        }
    }

    pdp8_kl8e_console_t *console = pdp8_kl8e_console_create(console_input_stream, console_output_stream);
    if (!console) {
        fprintf(stderr, "Unable to create KL8E console.\n");
        if (close_console_input && console_input_stream) {
            fclose(console_input_stream);
        }
        if (close_console_output && console_output_stream && console_output_stream != console_input_stream) {
            fclose(console_output_stream);
        }
        monitor_config_clear(&config);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    if (pdp8_kl8e_console_attach(cpu, console) != 0) {
        fprintf(stderr, "Unable to attach KL8E console.\n");
        pdp8_kl8e_console_destroy(console);
        if (close_console_input && console_input_stream) {
            fclose(console_input_stream);
        }
        if (close_console_output && console_output_stream && console_output_stream != console_input_stream) {
            fclose(console_output_stream);
        }
        monitor_config_clear(&config);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    FILE *printer_output_stream = stdout;
    bool close_printer_output = false;
    int printer_column_limit = 132;

    if (config_loaded && config.line_printer_present) {
        printer_column_limit = config.line_printer_column_limit > 0
                                   ? config.line_printer_column_limit
                                   : 132;
        if (config.line_printer_output && strcmp(config.line_printer_output, "stdout") != 0) {
            FILE *custom = NULL;
            if (strcmp(config.line_printer_output, "stderr") == 0) {
                custom = stderr;
            } else {
                custom = fopen(config.line_printer_output, "a");
                if (!custom) {
                    fprintf(stderr,
                            "Warning: unable to open line printer output '%s': %s. "
                            "Falling back to stdout.\n",
                            config.line_printer_output, strerror(errno));
                } else {
                    close_printer_output = true;
                }
            }
            if (custom) {
                printer_output_stream = custom;
            }
        }
    }

    pdp8_line_printer_t *printer = pdp8_line_printer_create(printer_output_stream);
    if (!printer) {
        fprintf(stderr, "Unable to create line printer peripheral.\n");
        pdp8_kl8e_console_destroy(console);
        if (close_console_input && console_input_stream) {
            fclose(console_input_stream);
        }
        if (close_console_output && console_output_stream && console_output_stream != console_input_stream) {
            fclose(console_output_stream);
        }
        monitor_config_clear(&config);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    if (pdp8_line_printer_attach(cpu, printer) != 0) {
        fprintf(stderr, "Unable to attach line printer peripheral.\n");
        pdp8_line_printer_destroy(printer);
        pdp8_kl8e_console_destroy(console);
        if (close_console_input && console_input_stream) {
            fclose(console_input_stream);
        }
        if (close_console_output && console_output_stream && console_output_stream != console_input_stream) {
            fclose(console_output_stream);
        }
        if (close_printer_output && printer_output_stream && printer_output_stream != console_output_stream &&
            printer_output_stream != console_input_stream) {
            fclose(printer_output_stream);
        }
        monitor_config_clear(&config);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    pdp8_line_printer_set_column_limit(printer, (uint16_t)printer_column_limit);

    pdp8_paper_tape_device_t *paper_tape_device = NULL;
    if (config_loaded && config.paper_tape_present) {
        if (config.paper_tape_image && *config.paper_tape_image) {
            paper_tape_device = pdp8_paper_tape_device_create();
            if (!paper_tape_device) {
                fprintf(stderr, "Warning: unable to create paper tape device.\n");
            } else if (pdp8_paper_tape_device_load(paper_tape_device, config.paper_tape_image) != 0) {
                fprintf(stderr, "Warning: unable to load paper tape image '%s'.\n",
                        config.paper_tape_image);
                pdp8_paper_tape_device_destroy(paper_tape_device);
                paper_tape_device = NULL;
            } else if (pdp8_paper_tape_device_attach(cpu, paper_tape_device) != 0) {
                fprintf(stderr, "Warning: unable to attach paper tape device (IOT 667x).\n");
                pdp8_paper_tape_device_destroy(paper_tape_device);
                paper_tape_device = NULL;
            }
        } else {
            fprintf(stderr,
                    "Warning: paper_tape device requested but no image path provided in pdp8.config.\n");
        }
    }

    const size_t memory_words = board->memory_words ? board->memory_words : 4096u;
    char line[512];

    puts("PDP-8 Monitor. Type 'help' for commands.");
    while (true) {
        fputs("pdp8> ", stdout);
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin)) {
            puts("");
            break;
        }

        line[strcspn(line, "\r\n")] = '\0';

        char *cmd = strtok(line, " \t");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "regs") == 0) {
            uint16_t pc = pdp8_api_get_pc(cpu);
            uint16_t ac = pdp8_api_get_ac(cpu);
            uint8_t link = pdp8_api_get_link(cpu);
            uint16_t sw = pdp8_api_get_switch_register(cpu);
            bool halted = pdp8_api_is_halted(cpu) != 0;
            printf("PC=%04o AC=%04o LINK=%o SW=%04o HALT=%s\n",
                   pc & 0x0FFFu, ac & 0x0FFFu, link & 0x1u, sw & 0x0FFFu, halted ? "yes" : "no");
        } else if (strcmp(cmd, "mem") == 0) {
            char *addr_tok = strtok(NULL, " \t");
            if (!addr_tok) {
                fprintf(stderr, "mem requires address.\n");
                continue;
            }
            long addr_val = 0;
            if (parse_number(addr_tok, &addr_val) != 0 || addr_val < 0) {
                fprintf(stderr, "Invalid address '%s'.\n", addr_tok);
                continue;
            }
            char *count_tok = strtok(NULL, " \t");
            long count_val = 8;
            if (count_tok) {
                if (parse_number(count_tok, &count_val) != 0 || count_val <= 0) {
                    fprintf(stderr, "Invalid count '%s'.\n", count_tok);
                    continue;
                }
            }

            size_t address = (size_t)addr_val;
            for (long i = 0; i < count_val; ++i) {
                size_t current = (address + (size_t)i) % memory_words;
                uint16_t word = pdp8_api_read_mem(cpu, (uint16_t)current);
                printf("%04zo: %04o\n", current, word & 0x0FFFu);
            }
        } else if (strcmp(cmd, "load") == 0) {
            char *addr_tok = strtok(NULL, " \t");
            if (!addr_tok) {
                fprintf(stderr, "load requires address.\n");
                continue;
            }
            long addr_val = 0;
            if (parse_number(addr_tok, &addr_val) != 0 || addr_val < 0) {
                fprintf(stderr, "Invalid address '%s'.\n", addr_tok);
                continue;
            }

            size_t loaded = 0;
            char *word_tok = NULL;
            while ((word_tok = strtok(NULL, " \t")) != NULL) {
                long word_val = 0;
                if (parse_number(word_tok, &word_val) != 0 || word_val < 0 || word_val > 0x0FFF) {
                    fprintf(stderr, "Invalid word '%s'.\n", word_tok);
                    loaded = 0;
                    break;
                }
                size_t current = ((size_t)addr_val + loaded) % memory_words;
                if (pdp8_api_write_mem(cpu, (uint16_t)current, (uint16_t)word_val) != 0) {
                    fprintf(stderr, "Failed to write memory at %04zo.\n", current);
                    loaded = 0;
                    break;
                }
                ++loaded;
            }

            if (loaded > 0) {
                printf("Loaded %zu word(s) starting at %04zo.\n", loaded, (size_t)addr_val % memory_words);
            }
        } else if (strcmp(cmd, "run") == 0) {
            char *start_tok = strtok(NULL, " \t");
            char *cycles_tok = strtok(NULL, " \t");
            if (!start_tok || !cycles_tok) {
                fprintf(stderr, "run requires start address and cycle count.\n");
                continue;
            }

            long start_val = 0;
            if (parse_number(start_tok, &start_val) != 0 || start_val < 0) {
                fprintf(stderr, "Invalid start address '%s'.\n", start_tok);
                continue;
            }
            if ((size_t)start_val >= memory_words) {
                fprintf(stderr, "Start address %04lo exceeds memory size.\n", start_val);
                continue;
            }

            long cycles_val = 0;
            if (parse_number(cycles_tok, &cycles_val) != 0 || cycles_val <= 0) {
                fprintf(stderr, "Invalid cycle count '%s'.\n", cycles_tok);
                continue;
            }

            pdp8_api_set_pc(cpu, (uint16_t)(start_val & 0x0FFFu));
            size_t cycles = (size_t)cycles_val;
            int executed = run_with_console(cpu, console, cycles, stdin);
            if (executed < 0) {
                fprintf(stderr, "Run failed.\n");
                continue;
            }
            printf("Executed %d cycle(s). PC=%04o HALT=%s\n",
                   executed, pdp8_api_get_pc(cpu) & 0x0FFFu,
                   pdp8_api_is_halted(cpu) ? "yes" : "no");
        } else if (strcmp(cmd, "save") == 0) {
            char *path = strtok(NULL, " \t");
            if (!path) {
                fprintf(stderr, "save requires file path.\n");
                continue;
            }
            FILE *fp = fopen(path, "wb");
            if (!fp) {
                fprintf(stderr, "Unable to open '%s' for writing: %s\n", path, strerror(errno));
                continue;
            }
            size_t written = 0;
            for (size_t addr = 0; addr < memory_words; ++addr) {
                uint16_t word = pdp8_api_read_mem(cpu, (uint16_t)addr);
                if (fwrite(&word, sizeof word, 1, fp) != 1) {
                    fprintf(stderr, "Write failed at address %04zo: %s\n", addr, strerror(errno));
                    break;
                }
                ++written;
            }
            fclose(fp);
            printf("Saved %zu word(s) to %s.\n", written, path);
        } else if (strcmp(cmd, "restore") == 0) {
            char *path = strtok(NULL, " \t");
            if (!path) {
                fprintf(stderr, "restore requires file path.\n");
                continue;
            }
            FILE *fp = fopen(path, "rb");
            if (!fp) {
                fprintf(stderr, "Unable to open '%s' for reading: %s\n", path, strerror(errno));
                continue;
            }
            size_t restored = 0;
            uint16_t word = 0;
            while (restored < memory_words && fread(&word, sizeof word, 1, fp) == 1) {
                pdp8_api_write_mem(cpu, (uint16_t)restored, word);
                ++restored;
            }
            if (!feof(fp)) {
                fprintf(stderr, "Read error while restoring: %s\n", strerror(errno));
            }
            fclose(fp);
            printf("Restored %zu word(s) from %s.\n", restored, path);
        } else if (strcmp(cmd, "read") == 0) {
            char *path = strtok(NULL, " \t");
            if (!path) {
                fprintf(stderr, "read requires file path.\n");
                continue;
            }

            size_t words_loaded = 0;
            size_t highest_address = 0;
            uint16_t start_address = 0;
            bool start_valid = false;

            if (load_srec_image(cpu, path, memory_words, &words_loaded, &highest_address,
                                &start_address, &start_valid) != 0) {
                continue;
            }

            printf("Loaded %zu word(s) from %s", words_loaded, path);
            if (words_loaded > 0) {
                printf(" (last %04zo)", highest_address);
            }
            puts(".");

            if (start_valid) {
                pdp8_api_set_pc(cpu, start_address & 0x0FFFu);
                printf("Start address %04o set as PC.\n", start_address & 0x0FFFu);
            }
        } else if (strcmp(cmd, "show") == 0) {
            char *topic = strtok(NULL, " \t");
            if (!topic) {
                fprintf(stderr, "show requires a subject (e.g. 'show devices').\n");
                continue;
            }
            if (strcmp(topic, "devices") == 0) {
                show_devices(&config, config_loaded, paper_tape_device);
            } else {
                fprintf(stderr, "Unknown subject for show: '%s'.\n", topic);
            }
        } else if (strcmp(cmd, "reset") == 0) {
            pdp8_api_reset(cpu);
            puts("CPU reset.");
        } else {
            fprintf(stderr, "Unknown command '%s'. Type 'help' for a list.\n", cmd);
        }
    }

    if (paper_tape_device) {
        pdp8_paper_tape_device_destroy(paper_tape_device);
    }
    pdp8_line_printer_destroy(printer);
    pdp8_kl8e_console_destroy(console);
    if (close_printer_output && printer_output_stream &&
        printer_output_stream != stdout && printer_output_stream != stderr) {
        fclose(printer_output_stream);
    }
    if (close_console_output && console_output_stream &&
        console_output_stream != stdout && console_output_stream != stderr &&
        console_output_stream != printer_output_stream) {
        fclose(console_output_stream);
    }
    if (close_console_input && console_input_stream && console_input_stream != stdin) {
        fclose(console_input_stream);
    }
    monitor_config_clear(&config);
    pdp8_api_destroy(cpu);
    return exit_code;
}
