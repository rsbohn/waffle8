#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/emulator/interrupt_control.h"
#include "../src/emulator/kl8e_console.h"
#include "../src/emulator/paper_tape_punch.h"
#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [--max-cycles N] [--no-eot] [--trace] <rom.srec>\n", prog);
    fprintf(stderr, "  Feed stdin to KL8E keyboard, write teleprinter output to stdout.\n");
    fprintf(stderr,
            "  Optional flags: --append-eot (default), --no-eot, --stats, --trace, --max-cycles N\n");
}

struct srec_result {
    size_t words_loaded;
    size_t highest_address;
    uint16_t start_pc;
    bool start_valid;
};

static int hex_byte(const char *hex, uint8_t *out) {
    if (!hex || !out || !isxdigit((unsigned char)hex[0]) || !isxdigit((unsigned char)hex[1])) {
        return -1;
    }
    char buf[3] = {hex[0], hex[1], '\0'};
    char *endptr = NULL;
    unsigned long val = strtoul(buf, &endptr, 16);
    if (!endptr || *endptr != '\0' || val > 0xFFul) {
        return -1;
    }
    *out = (uint8_t)val;
    return 0;
}

static int load_srec_image(pdp8_t *cpu, const char *path, struct srec_result *out) {
    if (!cpu || !path) {
        return -1;
    }

    const size_t memory_words = pdp8_api_get_memory_words(cpu);
    const size_t memory_bytes = memory_words * 2u;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Unable to open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    uint8_t *byte_data = (uint8_t *)calloc(memory_bytes, sizeof(uint8_t));
    bool *byte_present = (bool *)calloc(memory_bytes, sizeof(bool));
    if (!byte_data || !byte_present) {
        fprintf(stderr, "Unable to allocate %zu-byte buffer for S-record load\n", memory_bytes);
        free(byte_data);
        free(byte_present);
        fclose(fp);
        return -1;
    }

    char line[1024];
    bool start_seen = false;
    unsigned long start_byte_address = 0ul;
    bool have_data = false;

    while (fgets(line, sizeof line, fp)) {
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        size_t len = strlen(cursor);
        while (len > 0 && isspace((unsigned char)cursor[len - 1])) {
            cursor[--len] = '\0';
        }
        if (len < 4 || (cursor[0] != 'S' && cursor[0] != 's')) {
            continue;
        }

        char type = (char)toupper((unsigned char)cursor[1]);
        if (type != '1' && type != '2' && type != '3' && type != '7' && type != '8' &&
            type != '9') {
            continue;
        }

        uint8_t count = 0;
        if (hex_byte(cursor + 2, &count) != 0) {
            fprintf(stderr, "Malformed byte count: %s\n", cursor);
            free(byte_data);
            free(byte_present);
            fclose(fp);
            return -1;
        }

        size_t addr_bytes = 0u;
        if (type == '1' || type == '9') {
            addr_bytes = 2u;
        } else if (type == '2' || type == '8') {
            addr_bytes = 3u;
        } else {
            addr_bytes = 4u;
        }
        size_t addr_digits = addr_bytes * 2u;
        if (len < 4u + addr_digits + 2u) {
            fprintf(stderr, "Malformed record: %s\n", cursor);
            free(byte_data);
            free(byte_present);
            fclose(fp);
            return -1;
        }

        unsigned long base_address = 0ul;
        char addr_buf[9];
        if (addr_digits >= sizeof addr_buf) {
            fprintf(stderr, "Address too long in record: %s\n", cursor);
            free(byte_data);
            free(byte_present);
            fclose(fp);
            return -1;
        }
        memcpy(addr_buf, cursor + 4, addr_digits);
        addr_buf[addr_digits] = '\0';
        char *endptr = NULL;
        base_address = strtoul(addr_buf, &endptr, 16);
        if (!endptr || *endptr != '\0') {
            fprintf(stderr, "Malformed address: %s\n", cursor);
            free(byte_data);
            free(byte_present);
            fclose(fp);
            return -1;
        }

        const char *data_ptr = cursor + 4 + addr_digits;
        size_t data_hex_len = len - (size_t)(data_ptr - cursor);
        if (data_hex_len < 2u || data_hex_len % 2u != 0) {
            fprintf(stderr, "Malformed data section: %s\n", cursor);
            free(byte_data);
            free(byte_present);
            fclose(fp);
            return -1;
        }

        size_t data_bytes = (data_hex_len / 2u) - 1u; /* subtract checksum */
        if (count != data_bytes + addr_bytes + 1u) {
            fprintf(stderr, "Byte count mismatch: %s\n", cursor);
            free(byte_data);
            free(byte_present);
            fclose(fp);
            return -1;
        }

        if (type == '7' || type == '8' || type == '9') {
            start_seen = true;
            start_byte_address = base_address;
            continue;
        }

        for (size_t i = 0; i < data_bytes; ++i) {
            uint8_t value = 0;
            if (hex_byte(data_ptr + i * 2u, &value) != 0) {
                fprintf(stderr, "Malformed data byte in record: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }
            size_t absolute = base_address + i;
            if (absolute >= memory_bytes) {
                fprintf(stderr,
                        "Warning: S-record byte address 0x%lX exceeds memory (max 0x%zX); skipping\n",
                        base_address + i,
                        memory_bytes ? memory_bytes - 1u : 0u);
                continue;
            }
            byte_data[absolute] = value;
            byte_present[absolute] = true;
            have_data = true;
        }
    }

    fclose(fp);

    if (!have_data) {
        fprintf(stderr, "No data found in %s\n", path);
        free(byte_data);
        free(byte_present);
        return -1;
    }

    size_t written_words = 0;
    size_t highest_word = 0;
    for (size_t word = 0; word < memory_words; ++word) {
        size_t lo_index = word * 2u;
        size_t hi_index = lo_index + 1u;
        if (!byte_present[lo_index] || !byte_present[hi_index]) {
            continue;
        }
        uint16_t value =
            ((uint16_t)(byte_data[hi_index] & 0x0Fu) << 8) | (uint16_t)byte_data[lo_index];
        if (pdp8_api_write_mem(cpu, (uint16_t)word, value & 0x0FFFu) != 0) {
            fprintf(stderr, "Failed to write memory at %04zo\n", word);
            free(byte_data);
            free(byte_present);
            return -1;
        }
        written_words++;
        highest_word = word;
    }

    free(byte_data);
    free(byte_present);

    if (written_words == 0) {
        fprintf(stderr, "Parsed %s but wrote no words\n", path);
        return -1;
    }

    if (out) {
        out->words_loaded = written_words;
        out->highest_address = highest_word;
        out->start_valid = start_seen;
        out->start_pc = start_seen ? (uint16_t)((start_byte_address / 2u) & 0x0FFFu) : 0u;
    }
    return 0;
}

static int read_stdin(uint8_t **out_data, size_t *out_len, bool append_eot) {
    if (!out_data || !out_len) {
        return -1;
    }
    const size_t chunk = 4096;
    uint8_t *buffer = NULL;
    size_t size = 0;
    size_t capacity = 0;

    for (;;) {
        if (size + chunk > capacity) {
            size_t new_capacity = capacity ? capacity * 2u : chunk;
            uint8_t *new_data = (uint8_t *)realloc(buffer, new_capacity);
            if (!new_data) {
                free(buffer);
                return -1;
            }
            buffer = new_data;
            capacity = new_capacity;
        }
        size_t read_now = fread(buffer + size, 1, chunk, stdin);
        size += read_now;
        if (read_now < chunk) {
            if (ferror(stdin)) {
                free(buffer);
                return -1;
            }
            break; /* EOF */
        }
    }

    if (append_eot) {
        if (size + 1 > capacity) {
            uint8_t *new_data = (uint8_t *)realloc(buffer, size + 1);
            if (!new_data) {
                free(buffer);
                return -1;
            }
            buffer = new_data;
            capacity = size + 1;
        }
        buffer[size++] = 0x04; /* EOT */
    }

    *out_data = buffer;
    *out_len = size;
    return 0;
}

int main(int argc, char **argv) {
    const char *rom_path = NULL;
    size_t max_cycles = 10000000u;
    bool append_eot = true;
    bool print_stats = false;
    bool trace = false;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(arg, "--no-eot") == 0) {
            append_eot = false;
            continue;
        }
        if (strcmp(arg, "--append-eot") == 0) {
            append_eot = true;
            continue;
        }
        if (strcmp(arg, "--max-cycles") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            char *endptr = NULL;
            unsigned long long parsed = strtoull(argv[++i], &endptr, 10);
            if (!argv[i][0] || (endptr && *endptr != '\0')) {
                fprintf(stderr, "Invalid cycle count: %s\n", argv[i]);
                return EXIT_FAILURE;
            }
            max_cycles = (size_t)parsed;
            continue;
        }
        if (strcmp(arg, "--stats") == 0) {
            print_stats = true;
            continue;
        }
        if (strcmp(arg, "--trace") == 0) {
            trace = true;
            continue;
        }
        if (!rom_path) {
            rom_path = arg;
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!rom_path) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const pdp8_board_spec *board = pdp8_board_host_simulator();
    pdp8_t *cpu = pdp8_api_create_for_board(board);
    if (!cpu) {
        fprintf(stderr, "Unable to allocate PDP-8 CPU\n");
        return EXIT_FAILURE;
    }

    if (pdp8_interrupt_control_attach(cpu) != 0) {
        fprintf(stderr, "Unable to attach interrupt controller\n");
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    pdp8_kl8e_console_t *console = pdp8_kl8e_console_create(stdin, stdout);
    if (!console) {
        fprintf(stderr, "Unable to create KL8E console\n");
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }
    if (pdp8_kl8e_console_attach(cpu, console) != 0) {
        fprintf(stderr, "Unable to attach KL8E console\n");
        pdp8_kl8e_console_destroy(console);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    struct srec_result srec_info = {0};
    if (load_srec_image(cpu, rom_path, &srec_info) != 0) {
        pdp8_kl8e_console_destroy(console);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }
    uint16_t initial_pc = 0;
    if (srec_info.start_valid) {
        pdp8_api_set_pc(cpu, srec_info.start_pc);
    }
    initial_pc = pdp8_api_get_pc(cpu) & 0x0FFFu;

    pdp8_paper_tape_punch_t *punch = pdp8_paper_tape_punch_create();
    if (!punch) {
        fprintf(stderr, "Unable to create paper tape punch\n");
        pdp8_kl8e_console_destroy(console);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }
    pdp8_paper_tape_punch_set_stream(punch, stdout);
    if (pdp8_paper_tape_punch_attach(cpu, punch) != 0) {
        fprintf(stderr, "Unable to attach paper tape punch\n");
        pdp8_paper_tape_punch_destroy(punch);
        pdp8_kl8e_console_destroy(console);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    uint8_t *input_data = NULL;
    size_t input_len = 0;
    if (read_stdin(&input_data, &input_len, append_eot) != 0) {
        fprintf(stderr, "Failed to read stdin\n");
        pdp8_paper_tape_punch_destroy(punch);
        pdp8_kl8e_console_destroy(console);
        pdp8_api_destroy(cpu);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < input_len; ++i) {
        if (pdp8_kl8e_console_queue_input(console, input_data[i]) != 0) {
            fprintf(stderr, "Failed to queue input byte at index %zu\n", i);
            free(input_data);
            pdp8_paper_tape_punch_destroy(punch);
            pdp8_kl8e_console_destroy(console);
            pdp8_api_destroy(cpu);
            return EXIT_FAILURE;
        }
    }
    free(input_data);

    pdp8_api_clear_halt(cpu);

    size_t executed_total = 0;
    if (trace) {
        while (executed_total < max_cycles && !pdp8_api_is_halted(cpu)) {
            uint16_t pc = pdp8_api_get_pc(cpu) & 0x0FFFu;
            uint16_t instr = pdp8_api_read_mem(cpu, pc) & 0x0FFFu;
            uint16_t ac = pdp8_api_get_ac(cpu) & 0x0FFFu;
            uint8_t link = pdp8_api_get_link(cpu) & 0x1u;
            fprintf(stderr, "[%05zu] PC=%04o AC=%04o LINK=%o INSTR=%04o\n",
                    executed_total + 1,
                    pc,
                    ac,
                    link,
                    instr);
            int ran = pdp8_api_step(cpu);
            if (ran <= 0) {
                break;
            }
            executed_total += (size_t)ran;
        }
    } else {
        const size_t slice = 2000u;
        while (executed_total < max_cycles) {
            size_t remaining = max_cycles - executed_total;
            size_t request = remaining < slice ? remaining : slice;
            int executed = pdp8_api_run(cpu, request);
            if (executed <= 0) {
                break;
            }
            executed_total += (size_t)executed;
            if (pdp8_api_is_halted(cpu)) {
                break;
            }
        }
    }

    pdp8_kl8e_console_flush(console);
    fflush(stdout);

    if (print_stats) {
        size_t pending = pdp8_kl8e_console_input_pending(console);
        size_t tele_out = pdp8_kl8e_console_output_pending(console);
        size_t punched = pdp8_paper_tape_punch_bytes_written(punch);
        uint16_t pc = pdp8_api_get_pc(cpu) & 0x0FFFu;
        fprintf(stderr,
                "PC=%04o HALT=%s (initial PC=%04o)\n",
                pc,
                pdp8_api_is_halted(cpu) ? "yes" : "no",
                initial_pc);
        fprintf(stderr, "Memory %zu words; sample at PC:\n", pdp8_api_get_memory_words(cpu));
        for (uint16_t offset = 0; offset < 6u; ++offset) {
            uint16_t addr = (uint16_t)((pc + offset) & 0x0FFFu);
            fprintf(stderr, "  %04o: %04o\n", addr, pdp8_api_read_mem(cpu, addr) & 0x0FFFu);
        }
        fprintf(stderr, "Memory sample at 0200:\n");
        for (uint16_t addr = 0200; addr < 0206; ++addr) {
            fprintf(stderr, "  %04o: %04o\n", addr, pdp8_api_read_mem(cpu, addr) & 0x0FFFu);
        }
        fprintf(stderr,
                "Stats: executed=%zu input_remaining=%zu teleprinter_bytes=%zu punched_bytes=%zu\n",
                executed_total,
                pending,
                tele_out,
                punched);
    }

    int exit_code = EXIT_SUCCESS;
    if (!pdp8_api_is_halted(cpu) && executed_total >= max_cycles) {
        fprintf(stderr, "Cycle budget (%zu) exhausted before HALT\n", max_cycles);
        exit_code = EXIT_FAILURE;
    }

    pdp8_paper_tape_punch_destroy(punch);
    pdp8_kl8e_console_destroy(console);
    pdp8_api_destroy(cpu);
    return exit_code;
}
