#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/emulator/kl8e_console.h"
#include "../src/emulator/line_printer.h"
#include "../src/emulator/paper_tape.h"
#include "../src/emulator/paper_tape_device.h"
#include "../src/emulator/magtape_device.h"
#include <unistd.h>

#define ASSERT_EQ(label, expected, actual)                                                             \
    do {                                                                                               \
        if ((expected) != (actual)) {                                                                  \
            fprintf(stderr, "Assertion failed: %s (expected %06o, got %06o)\n", label, expected, actual); \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

#define ASSERT_TRUE(label, condition)                                                                  \
    do {                                                                                               \
        if (!(condition)) {                                                                            \
            fprintf(stderr, "Assertion failed: %s\n", label);                                          \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

#define ASSERT_INT_EQ(label, expected, actual)                                                         \
    do {                                                                                               \
        long long _exp = (long long)(expected);                                                        \
        long long _act = (long long)(actual);                                                          \
        if (_exp != _act) {                                                                            \
            fprintf(stderr, "Assertion failed: %s (expected %lld, got %lld)\n", label, _exp, _act);    \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

#define ASSERT_STR_EQ(label, expected, actual)                                                         \
    do {                                                                                               \
        const char *_exp = (expected);                                                                 \
        const char *_act = (actual);                                                                   \
        if ((_exp == NULL && _act != NULL) || (_exp != NULL && _act == NULL) ||                        \
            (_exp && _act && strcmp(_exp, _act) != 0)) {                                               \
            fprintf(stderr, "Assertion failed: %s\n", label);                                          \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

static int load_srec_into_cpu(pdp8_t *cpu,
                              const char *path,
                              uint16_t *start_address,
                              int *start_valid) {
    if (start_address) {
        *start_address = 0;
    }
    if (start_valid) {
        *start_valid = 0;
    }
    if (!cpu || !path) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Unable to open S-record '%s'\n", path);
        return -1;
    }

    const size_t memory_words = 4096u;
    const size_t memory_bytes = memory_words * 2u;
    uint8_t *byte_data = (uint8_t *)calloc(memory_bytes, sizeof(uint8_t));
    uint8_t *byte_mask = (uint8_t *)calloc(memory_bytes, sizeof(uint8_t));
    if (!byte_data || !byte_mask) {
        fprintf(stderr, "Unable to allocate buffer for '%s'\n", path);
        free(byte_data);
        free(byte_mask);
        fclose(fp);
        return -1;
    }

    char line[1024];
    int have_data = 0;
    unsigned long start_byte_address = 0ul;
    int have_start = 0;

    int status = -1;

    while (fgets(line, sizeof line, fp) != NULL) {
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }

        size_t len = strlen(cursor);
        while (len > 0 && isspace((unsigned char)cursor[len - 1])) {
            cursor[--len] = '\0';
        }

        if (len == 0u) {
            continue;
        }

        if (cursor[0] != 'S' && cursor[0] != 's') {
            continue;
        }

        char type = (char)toupper((unsigned char)cursor[1]);
        if (type == '1' || type == '2' || type == '3') {
            size_t addr_digits = (type == '1') ? 4u : (type == '2') ? 6u : 8u;
            size_t addr_bytes = addr_digits / 2u;

            if (len < 4u + addr_digits + 2u) {
                fprintf(stderr, "Malformed S-record line: %s\n", cursor);
                goto cleanup;
            }

            char count_str[3] = {cursor[2], cursor[3], '\0'};
            char *endptr = NULL;
            unsigned long count = strtoul(count_str, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                fprintf(stderr, "Invalid byte count in S-record: %s\n", cursor);
                goto cleanup;
            }

            char addr_buf[9];
            if (addr_digits >= sizeof addr_buf) {
                fprintf(stderr, "Address field too large in S-record: %s\n", cursor);
                goto cleanup;
            }
            memcpy(addr_buf, cursor + 4, addr_digits);
            addr_buf[addr_digits] = '\0';
            endptr = NULL;
            unsigned long base_address = strtoul(addr_buf, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                fprintf(stderr, "Invalid address in S-record: %s\n", cursor);
                goto cleanup;
            }

            const char *data_ptr = cursor + 4 + addr_digits;
            const char *checksum_ptr = cursor + len - 2;
            if (checksum_ptr < data_ptr) {
                fprintf(stderr, "Malformed data payload in S-record: %s\n", cursor);
                goto cleanup;
            }
            size_t data_hex_len = (size_t)(checksum_ptr - data_ptr);
            if (data_hex_len % 2u != 0u) {
                fprintf(stderr, "Odd number of data nybbles in S-record: %s\n", cursor);
                goto cleanup;
            }

            size_t data_bytes = data_hex_len / 2u;
            if (count != data_bytes + addr_bytes + 1u) {
                fprintf(stderr, "Count mismatch in S-record: %s\n", cursor);
                goto cleanup;
            }

            for (size_t i = 0; i < data_bytes; ++i) {
                char byte_buf[3] = {data_ptr[i * 2u], data_ptr[i * 2u + 1u], '\0'};
                endptr = NULL;
                unsigned long value = strtoul(byte_buf, &endptr, 16);
                if (!endptr || *endptr != '\0' || value > 0xFFul) {
                    fprintf(stderr, "Invalid data byte in S-record: %s\n", cursor);
                    goto cleanup;
                }

                unsigned long absolute = base_address + i;
                if (absolute >= memory_bytes) {
                    continue;
                }
                byte_data[absolute] = (uint8_t)value;
                byte_mask[absolute] = 1u;
                have_data = 1;
            }
        } else if (type == '7' || type == '8' || type == '9') {
            size_t addr_digits = (type == '9') ? 4u : (type == '8') ? 6u : 8u;
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
                have_start = 1;
            }
        }
    }

    if (!have_data) {
        fprintf(stderr, "No data records found in '%s'\n", path);
        goto cleanup;
    }

    size_t words_written = 0;
    int encountered_partial = 0;
    for (size_t word = 0; word < memory_words; ++word) {
        size_t lo_index = word * 2u;
        size_t hi_index = lo_index + 1u;
        bool have_lo = byte_mask[lo_index] != 0u;
        bool have_hi = byte_mask[hi_index] != 0u;

        if (have_lo && have_hi) {
            uint16_t value =
                (uint16_t)(((uint16_t)(byte_data[hi_index] & 0x0Fu) << 8u) | byte_data[lo_index]);
            if (pdp8_api_write_mem(cpu, (uint16_t)word, (uint16_t)(value & 0x0FFFu)) != 0) {
                fprintf(stderr, "Failed to write memory at %04zo while loading '%s'\n", word, path);
                goto cleanup;
            }
            ++words_written;
        } else if (have_lo || have_hi) {
            encountered_partial = 1;
        }
    }

    if (words_written == 0 || encountered_partial) {
        fprintf(stderr, "Incomplete word data encountered in '%s'\n", path);
        goto cleanup;
    }

    if (start_address && have_start) {
        *start_address = (uint16_t)((start_byte_address / 2u) & 0x0FFFu);
    }
    if (start_valid) {
        *start_valid = have_start ? 1 : 0;
    }

    status = 0;

cleanup:
    free(byte_data);
    free(byte_mask);
    fclose(fp);
    return status;
}

static int test_memory_reference(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        fprintf(stderr, "Unable to allocate CPU\n");
        return 0;
    }

    /* TAD direct */
    pdp8_api_set_ac(cpu, 0400);
    pdp8_api_write_mem(cpu, 0000, 01010); /* TAD 0010 */
    pdp8_api_write_mem(cpu, 0001, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0010, 0200);
    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("TAD result", 0600, pdp8_api_get_ac(cpu));
    ASSERT_EQ("Link after TAD", 0, pdp8_api_get_link(cpu));

    /* ISZ with skip */
    pdp8_api_write_mem(cpu, 0002, 02020); /* ISZ 0020 */
    pdp8_api_write_mem(cpu, 0003, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0020, 07777);
    pdp8_api_set_pc(cpu, 0002);
    pdp8_api_step(cpu);
    ASSERT_EQ("ISZ increments", 00000, pdp8_api_read_mem(cpu, 0020));
    ASSERT_EQ("ISZ skip", 0004, pdp8_api_get_pc(cpu));

    /* JMS stores PC and jumps */
    pdp8_api_write_mem(cpu, 0004, 04030); /* JMS 0030 */
    pdp8_api_write_mem(cpu, 0030, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0031, 00000);
    pdp8_api_set_pc(cpu, 0004);
    pdp8_api_step(cpu);
    ASSERT_EQ("Return address stored", 0005, pdp8_api_read_mem(cpu, 0030));
    ASSERT_EQ("PC after JMS", 0031, pdp8_api_get_pc(cpu));

    /* JMP */
    pdp8_api_write_mem(cpu, 0031, 05040); /* JMP 0040 */
    pdp8_api_write_mem(cpu, 0040, 07402);
    pdp8_api_step(cpu);
    ASSERT_EQ("JMP target", 0040, pdp8_api_get_pc(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_indirect_and_auto_increment(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_write_mem(cpu, 0000, 01010 | 00400); /* TAD I 0010 */
    pdp8_api_write_mem(cpu, 0001, 07402);
    pdp8_api_write_mem(cpu, 0010, 00020); /* pointer */
    pdp8_api_write_mem(cpu, 00021, 00005);

    pdp8_api_set_pc(cpu, 0);
    pdp8_api_step(cpu);
    ASSERT_EQ("Indirect fetch", 00005, pdp8_api_get_ac(cpu));
    ASSERT_EQ("Auto-increment", 00021, pdp8_api_read_mem(cpu, 0010));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_operate_group1(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_set_ac(cpu, 01234);
    pdp8_api_set_link(cpu, 1);
    pdp8_api_write_mem(cpu, 0000, 07200); /* CLA */
    pdp8_api_write_mem(cpu, 0001, 07040); /* CMA */
    pdp8_api_write_mem(cpu, 0002, 07001); /* IAC */
    pdp8_api_write_mem(cpu, 0003, 07010); /* RAR */
    pdp8_api_write_mem(cpu, 0004, 07402); /* HLT */

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("CLA clears AC", 00000, pdp8_api_get_ac(cpu));
    pdp8_api_step(cpu);
    ASSERT_EQ("CMA", 07777, pdp8_api_get_ac(cpu));
    pdp8_api_step(cpu);
    ASSERT_EQ("IAC", 00000, pdp8_api_get_ac(cpu));
    ASSERT_EQ("IAC link", 0, pdp8_api_get_link(cpu));
    pdp8_api_set_ac(cpu, 00000);
    pdp8_api_set_link(cpu, 1);
    pdp8_api_step(cpu);
    ASSERT_EQ("RAR rotates AC", 04000, pdp8_api_get_ac(cpu));
    ASSERT_EQ("RAR link", 0, pdp8_api_get_link(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_operate_group2(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_set_ac(cpu, 04000);
    pdp8_api_set_link(cpu, 1);
    pdp8_api_set_switch_register(cpu, 00012);

    pdp8_api_write_mem(cpu, 0000, 07600); /* CLA */
    pdp8_api_write_mem(cpu, 0001, 07500); /* SMA (negative -> skip) */
    pdp8_api_write_mem(cpu, 0002, 07404); /* OSR */
    pdp8_api_write_mem(cpu, 0003, 07402); /* HLT */

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("Group2 CLA", 00000, pdp8_api_get_ac(cpu));

    pdp8_api_set_ac(cpu, 04000); /* negative because bit 11 set */
    pdp8_api_step(cpu); /* SMA -> skip */
    ASSERT_EQ("SMA skip", 0003, pdp8_api_get_pc(cpu));

    pdp8_api_set_pc(cpu, 0002);
    pdp8_api_set_ac(cpu, 00000);
    pdp8_api_step(cpu);
    ASSERT_EQ("OSR merges switch register", 00012, pdp8_api_get_ac(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

struct iot_state {
    int invoked;
};

static void sample_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    (void)instruction;
    struct iot_state *state = (struct iot_state *)context;
    state->invoked += 1;
    pdp8_api_request_skip(cpu);
}

static int test_iot(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    struct iot_state state = {0};
    pdp8_api_register_iot(cpu, 03, sample_iot, &state);

    pdp8_api_write_mem(cpu, 0000, 06030); /* device 03 */
    pdp8_api_write_mem(cpu, 0001, 07402);
    pdp8_api_set_pc(cpu, 0);
    pdp8_api_step(cpu);
    ASSERT_EQ("IOT invoked", 1, state.invoked);
    ASSERT_EQ("IOT skip", 0002, pdp8_api_get_pc(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int write_le_word(FILE *fp, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    return fwrite(bytes, 1u, sizeof bytes, fp) == sizeof bytes;
}

static int test_magtape_sense_reports_status(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_magtape_device_t *device = pdp8_magtape_device_create();
    if (!device) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    if (pdp8_magtape_device_attach(cpu, device) != 0) {
        pdp8_magtape_device_destroy(device);
        pdp8_api_destroy(cpu);
        return 0;
    }

    char temp_dir[64];
    snprintf(temp_dir, sizeof temp_dir, "magtape-test-%ld", (long)getpid());
    if (mkdir(temp_dir, 0700) != 0) {
        if (errno == EEXIST) {
            char stale_path[128];
            snprintf(stale_path, sizeof stale_path, "%s/sample.tap", temp_dir);
            remove(stale_path);
            if (rmdir(temp_dir) != 0 || mkdir(temp_dir, 0700) != 0) {
                pdp8_magtape_device_destroy(device);
                pdp8_api_destroy(cpu);
                return 0;
            }
        } else {
            pdp8_magtape_device_destroy(device);
            pdp8_api_destroy(cpu);
            return 0;
        }
    }

    char file_path[128];
    snprintf(file_path, sizeof file_path, "%s/sample.tap", temp_dir);
    FILE *fp = fopen(file_path, "wb");
    if (!fp) {
        rmdir(temp_dir);
        pdp8_magtape_device_destroy(device);
        pdp8_api_destroy(cpu);
        return 0;
    }

    if (!write_le_word(fp, 00001u) || !write_le_word(fp, 01234u) ||
        !write_le_word(fp, 0xFFFFu)) {
        fclose(fp);
        remove(file_path);
        rmdir(temp_dir);
        pdp8_magtape_device_destroy(device);
        pdp8_api_destroy(cpu);
        return 0;
    }
    fclose(fp);

    struct pdp8_magtape_unit_params params;
    params.unit_number = 0u;
    params.path = temp_dir;
    params.write_protected = true;
    if (pdp8_magtape_device_configure_unit(device, &params) != 0) {
        remove(file_path);
        rmdir(temp_dir);
        pdp8_magtape_device_destroy(device);
        pdp8_api_destroy(cpu);
        return 0;
    }

    struct pdp8_magtape_unit_status host_status;
    ASSERT_INT_EQ("query magtape status", 0,
                  pdp8_magtape_device_get_status(device, 0u, &host_status));
    ASSERT_TRUE("host API reports ready", host_status.ready);
    ASSERT_TRUE("host API reports write protect", host_status.write_protected);

    pdp8_api_write_mem(cpu, 0000, PDP8_MAGTAPE_INSTR(PDP8_MAGTAPE_BIT_GO));
    pdp8_api_write_mem(cpu, 0001, PDP8_MAGTAPE_INSTR(PDP8_MAGTAPE_BIT_SENSE));
    pdp8_api_write_mem(cpu, 0002, PDP8_MAGTAPE_INSTR(PDP8_MAGTAPE_BIT_READ));
    pdp8_api_write_mem(cpu, 0003, PDP8_MAGTAPE_INSTR(PDP8_MAGTAPE_BIT_SENSE));
    ASSERT_EQ("GO instruction", PDP8_MAGTAPE_INSTR(PDP8_MAGTAPE_BIT_GO),
              pdp8_api_read_mem(cpu, 0000));
    ASSERT_EQ("SENSE instruction", PDP8_MAGTAPE_INSTR(PDP8_MAGTAPE_BIT_SENSE),
              pdp8_api_read_mem(cpu, 0001));
    pdp8_api_set_ac(cpu, 0000);
    pdp8_api_set_pc(cpu, 0000);

    ASSERT_INT_EQ("execute GO", 1, pdp8_api_step(cpu));
    ASSERT_EQ("PC after GO", 0001u, pdp8_api_get_pc(cpu));
    ASSERT_INT_EQ("execute SENSE", 1, pdp8_api_step(cpu));
    ASSERT_EQ("PC after SENSE", 0002u, pdp8_api_get_pc(cpu));
    uint16_t sense_ready = pdp8_api_get_ac(cpu);
    ASSERT_TRUE("READY flag set", (sense_ready & 00001u) != 0u);
    ASSERT_TRUE("WRITE PROTECT flag set", (sense_ready & 00020u) != 0u);

    ASSERT_INT_EQ("execute READ", 1, pdp8_api_step(cpu));
    ASSERT_EQ("read data word", 01234u, pdp8_api_get_ac(cpu));

    ASSERT_INT_EQ("execute SENSE after read", 1, pdp8_api_step(cpu));
    uint16_t sense_eor = pdp8_api_get_ac(cpu);
    ASSERT_TRUE("EOR flag set", (sense_eor & 00004u) != 0u);
    ASSERT_TRUE("EOT flag set", (sense_eor & 00010u) != 0u);

    remove(file_path);
    rmdir(temp_dir);
    pdp8_magtape_device_destroy(device);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_clear_halt(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_write_mem(cpu, 0000, 07402); /* HLT */
    pdp8_api_set_pc(cpu, 0000);
    ASSERT_INT_EQ("execute HLT", 1, pdp8_api_step(cpu));
    ASSERT_TRUE("halt flag set", pdp8_api_is_halted(cpu) != 0);

    pdp8_api_clear_halt(cpu);
    ASSERT_TRUE("halt flag cleared", pdp8_api_is_halted(cpu) == 0);

    pdp8_api_write_mem(cpu, 0001, 07001); /* IAC */
    pdp8_api_set_ac(cpu, 07777);
    pdp8_api_set_pc(cpu, 0001);
    ASSERT_INT_EQ("run executes after clear", 1, pdp8_api_run(cpu, 1));
    ASSERT_EQ("IAC result", 00000, pdp8_api_get_ac(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_kl8e_console(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    FILE *sink = tmpfile();
    if (!sink) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    pdp8_kl8e_console_t *console = pdp8_kl8e_console_create(NULL, sink);
    if (!console) {
        fclose(sink);
        pdp8_api_destroy(cpu);
        return 0;
    }

    if (pdp8_kl8e_console_attach(cpu, console) != 0) {
        pdp8_kl8e_console_destroy(console);
        fclose(sink);
        pdp8_api_destroy(cpu);
        return 0;
    }

    ASSERT_INT_EQ("queue input", 0, pdp8_kl8e_console_queue_input(console, 'A'));

    pdp8_api_write_mem(cpu, 0000, PDP8_KL8E_KEYBOARD_INSTR(PDP8_KL8E_KEYBOARD_BIT_SKIP));
    pdp8_api_write_mem(cpu, 0001, 00000);
    pdp8_api_write_mem(cpu, 0002,
                       PDP8_KL8E_KEYBOARD_INSTR(PDP8_KL8E_KEYBOARD_BIT_CLEAR | PDP8_KL8E_KEYBOARD_BIT_READ));
    pdp8_api_write_mem(cpu, 0003, PDP8_KL8E_TELEPRINTER_INSTR(PDP8_KL8E_TELEPRINTER_BIT_SKIP));
    pdp8_api_write_mem(cpu, 0004, 00000);
    pdp8_api_write_mem(cpu, 0005,
                       PDP8_KL8E_TELEPRINTER_INSTR(PDP8_KL8E_TELEPRINTER_BIT_CLEAR | PDP8_KL8E_TELEPRINTER_BIT_LOAD));
    pdp8_api_write_mem(cpu, 0006, 07402);

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("KSF skip", 0002, pdp8_api_get_pc(cpu));

    pdp8_api_step(cpu);
    ASSERT_EQ("KRB loads AC", 'A' & 0x7F, pdp8_api_get_ac(cpu));
    ASSERT_INT_EQ("input consumed", 0, (int)pdp8_kl8e_console_input_pending(console));

    pdp8_api_step(cpu);
    ASSERT_EQ("TSF skip", 0005, pdp8_api_get_pc(cpu));

    pdp8_api_step(cpu);
    ASSERT_INT_EQ("output buffered", 1, (int)pdp8_kl8e_console_output_pending(console));
    uint8_t out_ch = 0;
    ASSERT_INT_EQ("pop output", 0, pdp8_kl8e_console_pop_output(console, &out_ch));
    ASSERT_EQ("output char", 'A' & 0x7F, out_ch);

    pdp8_kl8e_console_destroy(console);
    fclose(sink);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_line_printer(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    FILE *sink = tmpfile();
    if (!sink) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    pdp8_line_printer_t *printer = pdp8_line_printer_create(sink);
    ASSERT_TRUE("line printer created", printer != NULL);
    ASSERT_INT_EQ("line printer attach", 0, pdp8_line_printer_attach(cpu, printer));

    pdp8_api_write_mem(cpu, 0000, 07200); /* CLA */
    pdp8_api_write_mem(cpu, 0001, 01010); /* TAD 0010 */
    pdp8_api_write_mem(cpu, 0002,
                       PDP8_LINE_PRINTER_INSTR(PDP8_LINE_PRINTER_BIT_CLEAR | PDP8_LINE_PRINTER_BIT_PRINT));
    pdp8_api_write_mem(cpu, 0003, PDP8_LINE_PRINTER_INSTR(PDP8_LINE_PRINTER_BIT_SKIP));
    pdp8_api_write_mem(cpu, 0004, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0005, 07402); /* HLT (skip target) */
    pdp8_api_write_mem(cpu, 0010, 00101); /* ASCII 'A' */
    pdp8_api_set_pc(cpu, 0000);

    pdp8_api_run(cpu, 16);
    ASSERT_INT_EQ("halted after print", 1, pdp8_api_is_halted(cpu));
    ASSERT_EQ("PC after skip-ready", 0006, pdp8_api_get_pc(cpu));

    fflush(sink);
    long position = ftell(sink);
    ASSERT_TRUE("line printer wrote data", position >= 1);
    fseek(sink, 0, SEEK_SET);
    char buffer[16] = {0};
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, sink);
    ASSERT_TRUE("printed output length", len > 0 && len < sizeof(buffer));

    const char *payload = memchr(buffer, 'A', len);
    ASSERT_TRUE("printed character present", payload != NULL);

    static const char reset_sequence[] = "\x1b[0m";
    ASSERT_TRUE("line printer colour reset",
                len >= sizeof(reset_sequence) - 1 &&
                    memcmp(buffer + len - (sizeof(reset_sequence) - 1),
                           reset_sequence,
                           sizeof(reset_sequence) - 1) == 0);

    pdp8_line_printer_destroy(printer);
    fclose(sink);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_demo_core_fixture(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    FILE *sink = tmpfile();
    if (!sink) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    pdp8_line_printer_t *printer = pdp8_line_printer_create(sink);
    if (!printer) {
        fclose(sink);
        pdp8_api_destroy(cpu);
        return 0;
    }

    ASSERT_INT_EQ("attach line printer", 0, pdp8_line_printer_attach(cpu, printer));
    ASSERT_INT_EQ("load core image", 0, load_srec_into_cpu(cpu, "demo/core.srec", NULL, NULL));

    uint16_t fixture_start = 0;
    int fixture_start_valid = 0;
    ASSERT_INT_EQ("load fixture image",
                  0,
                  load_srec_into_cpu(cpu, "demo/fixture.srec", &fixture_start, &fixture_start_valid));

    pdp8_api_set_pc(cpu, 07000);
    ASSERT_TRUE("core init executed", pdp8_api_run(cpu, 64) > 0);
    ASSERT_TRUE("core init halted", pdp8_api_is_halted(cpu) != 0);
    ASSERT_EQ("core call vector opcode", 05403, pdp8_api_read_mem(cpu, 0002));
    ASSERT_EQ("core entry address", 07000, pdp8_api_read_mem(cpu, 0003));

    pdp8_api_clear_halt(cpu);
    uint16_t start_pc = fixture_start_valid ? fixture_start : 0200;
    pdp8_api_set_pc(cpu, start_pc);
    ASSERT_TRUE("fixture executed", pdp8_api_run(cpu, 2000) > 0);
    ASSERT_TRUE("fixture halted", pdp8_api_is_halted(cpu) != 0);

    ASSERT_EQ("dispatch call counter", 0003, pdp8_api_read_mem(cpu, 0007));
    ASSERT_EQ("final AC", 0053, pdp8_api_get_ac(cpu));

    ASSERT_INT_EQ("seek output end", 0, fseek(sink, 0, SEEK_END));
    long output_len = ftell(sink);
    ASSERT_TRUE("fixture produced output", output_len >= 3);
    ASSERT_INT_EQ("rewind output", 0, fseek(sink, 0, SEEK_SET));

    char buffer[64] = {0};
    size_t read_len = fread(buffer, 1, sizeof(buffer) - 1u, sink);
    ASSERT_TRUE("fixture output length", read_len > 0 && read_len < sizeof(buffer));

    char printable[8] = {0};
    size_t printable_len = 0;
    for (size_t idx = 0; idx < read_len; ++idx) {
        unsigned char ch = (unsigned char)buffer[idx];
        if (ch == 0x1Bu && idx + 1u < read_len && buffer[idx + 1u] == '[') {
            idx += 2u;
            while (idx < read_len && (unsigned char)buffer[idx] < 0x40u) {
                ++idx;
            }
            continue;
        }
        if (printable_len + 1u < sizeof(printable)) {
            printable[printable_len++] = (char)ch;
        }
    }
    printable[printable_len] = '\0';

    ASSERT_INT_EQ("fixture printable length", 3, (int)printable_len);
    ASSERT_STR_EQ("fixture output sequence", "..+", printable);

    pdp8_line_printer_destroy(printer);
    fclose(sink);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_paper_tape_parser(void) {
    const char *path = "paper_tape_parser.tmp";
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }

    fprintf(fp, "AA001: 000000 000001 111111 000000\n");
    fprintf(fp, "AA002: 101010 101010 010101 010101\n");
    fclose(fp);

    pdp8_paper_tape *image = NULL;
    if (pdp8_paper_tape_load(path, &image) != 0) {
        remove(path);
        return 0;
    }

    ASSERT_TRUE("tape image present", image != NULL);
    ASSERT_STR_EQ("tape label", "AA", image->label);
    ASSERT_INT_EQ("two blocks parsed", 2, (int)image->block_count);

    const pdp8_paper_tape_block *block1 = pdp8_paper_tape_find(image, 0001);
    ASSERT_TRUE("block 001 present", block1 != NULL);
    ASSERT_INT_EQ("block 001 word count", 2, (int)block1->word_count);
    ASSERT_EQ("block 001 word 0", 000001, block1->words[0]);
    ASSERT_EQ("block 001 word 1", 07700, block1->words[1]);

    const pdp8_paper_tape_block *block2 = pdp8_paper_tape_find(image, 0002);
    ASSERT_TRUE("block 002 present", block2 != NULL);
    ASSERT_INT_EQ("block 002 word count", 2, (int)block2->word_count);
    ASSERT_EQ("block 002 word 0", 05252, block2->words[0]);
    ASSERT_EQ("block 002 word 1", 02525, block2->words[1]);

    pdp8_paper_tape_destroy(image);
    remove(path);

    fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }

    fputs("AA003:", fp);
    for (size_t i = 0; i < PDP8_PAPER_TAPE_MAX_WORDS + 1; ++i) {
        fputs(" 000000000000", fp);
    }
    fputc('\n', fp);
    fclose(fp);

    pdp8_paper_tape *too_large = NULL;
    ASSERT_INT_EQ("block size limit enforced", -1,
                  pdp8_paper_tape_load(path, &too_large));
    ASSERT_TRUE("no tape returned on failure", too_large == NULL);
    remove(path);

    return 1;
}

static int test_paper_tape_device(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_paper_tape_device_t *device = pdp8_paper_tape_device_create();
    if (!device) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    ASSERT_INT_EQ("load tape image", 0, pdp8_paper_tape_device_load(device, "tapes/tp_demo.tape"));
    ASSERT_STR_EQ("tape device label", "TP", pdp8_paper_tape_device_label(device));
    ASSERT_INT_EQ("attach tape device", 0, pdp8_paper_tape_device_attach(cpu, device));

    pdp8_api_write_mem(cpu, 0000, PDP8_PAPER_TAPE_INSTR(PDP8_PAPER_TAPE_BIT_SELECT));
    pdp8_api_write_mem(cpu, 0001, PDP8_PAPER_TAPE_INSTR(PDP8_PAPER_TAPE_BIT_READ));
    pdp8_api_write_mem(cpu, 0002, 05001); /* JMP 0001 */

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_set_ac(cpu, 0001);
    ASSERT_INT_EQ("select block 1", 1, pdp8_api_step(cpu));

    for (size_t i = 0; i < PDP8_PAPER_TAPE_MAX_WORDS; ++i) {
        ASSERT_INT_EQ("read block 1 word", 1, pdp8_api_step(cpu));
        ASSERT_EQ("block 1 data", 05252, pdp8_api_get_ac(cpu));
        ASSERT_INT_EQ("loop block 1", 1, pdp8_api_step(cpu));
    }

    pdp8_api_set_ac(cpu, 0);
    ASSERT_INT_EQ("read past block 1", 1, pdp8_api_step(cpu));
    ASSERT_EQ("block 1 exhausted", 0, pdp8_api_get_ac(cpu));
    ASSERT_INT_EQ("loop after block 1 exhaustion", 1, pdp8_api_step(cpu));

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_set_ac(cpu, 0002);
    ASSERT_INT_EQ("select block 2", 1, pdp8_api_step(cpu));

    for (size_t i = 0; i < PDP8_PAPER_TAPE_MAX_WORDS; ++i) {
        ASSERT_INT_EQ("read block 2 word", 1, pdp8_api_step(cpu));
        ASSERT_EQ("block 2 data", 02445, pdp8_api_get_ac(cpu));
        ASSERT_INT_EQ("loop block 2", 1, pdp8_api_step(cpu));
    }

    pdp8_paper_tape_device_destroy(device);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_board_spec(void) {
    const pdp8_board_spec *spec = pdp8_board_adafruit_fruit_jam();
    ASSERT_TRUE("Fruit Jam spec available", spec != NULL);
    ASSERT_STR_EQ("Fruit Jam name", "Adafruit Fruit Jam", spec->name);
    ASSERT_STR_EQ("Fruit Jam MCU", "RP2350", spec->mcu);
    ASSERT_INT_EQ("Fruit Jam memory words", 4096, spec->memory_words);
    ASSERT_INT_EQ("Fruit Jam tick", 60, spec->tick_hz);

    pdp8_t *cpu = pdp8_api_create_for_board(spec);
    ASSERT_TRUE("CPU created for Fruit Jam", cpu != NULL);
    ASSERT_TRUE("Board attached", pdp8_api_get_board(cpu) == spec);
    ASSERT_EQ("Write last word", 0, pdp8_api_write_mem(cpu, (uint16_t)(spec->memory_words - 1u), 01234));
    ASSERT_EQ("Read last word", 01234, pdp8_api_read_mem(cpu, (uint16_t)(spec->memory_words - 1u)));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_ion_ioff(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    ASSERT_TRUE("CPU created", cpu != NULL);
    
    /* ION instruction: 0x0F01 (operate group 2, bit 8=1, bit 0=1 for ION) */
    pdp8_api_write_mem(cpu, 0, 0x0F01);  /* ION */
    pdp8_api_step(cpu);
    
    /* After ION, interrupts should be enabled (but we can't directly query this in public API) */
    /* We can test via IOFF (0x0F00 - operate group 2, bit 8=1, bit 0=0) */
    pdp8_api_write_mem(cpu, 1, 0x0F00);  /* IOFF */
    pdp8_api_step(cpu);
    
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_interrupt_pending_count(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    ASSERT_TRUE("CPU created", cpu != NULL);
    
    /* Initially no interrupts pending */
    ASSERT_INT_EQ("initial pending count", 0, pdp8_api_peek_interrupt_pending(cpu));
    
    /* Request interrupt */
    ASSERT_INT_EQ("request returns 0", 0, pdp8_api_request_interrupt(cpu, 055));
    ASSERT_INT_EQ("pending count after request", 1, pdp8_api_peek_interrupt_pending(cpu));
    
    /* Request multiple interrupts */
    pdp8_api_request_interrupt(cpu, 055);
    pdp8_api_request_interrupt(cpu, 031);
    ASSERT_INT_EQ("pending count after 3 requests", 3, pdp8_api_peek_interrupt_pending(cpu));
    
    /* Clear one interrupt */
    ASSERT_INT_EQ("clear returns 0", 0, pdp8_api_clear_interrupt_pending(cpu));
    ASSERT_INT_EQ("pending count after clear", 2, pdp8_api_peek_interrupt_pending(cpu));
    
    /* Clear remaining interrupts */
    pdp8_api_clear_interrupt_pending(cpu);
    pdp8_api_clear_interrupt_pending(cpu);
    ASSERT_INT_EQ("pending count after clearing all", 0, pdp8_api_peek_interrupt_pending(cpu));
    
    /* Clearing when no interrupts returns error */
    ASSERT_INT_EQ("clear on empty returns -1", -1, pdp8_api_clear_interrupt_pending(cpu));
    
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_interrupt_dispatch(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    ASSERT_TRUE("CPU created", cpu != NULL);
    
    /* Setup program: ION at 0, AND at 1, AND at 2 */
    pdp8_api_write_mem(cpu, 0, 0x0F01);  /* ION - enables interrupts (group 2) */
    pdp8_api_write_mem(cpu, 1, 0x0000);  /* AND - benign instruction */
    pdp8_api_write_mem(cpu, 2, 0x0000);  /* AND - benign instruction */
    
    /* Setup ISR entry at 0x0010 */
    pdp8_api_write_mem(cpu, 0x0010, 0x0F02);  /* CLA CLL (group 2) */
    
    /* First: test WITHOUT interrupt pending - should not dispatch */
    pdp8_api_step(cpu);  /* Execute ION at PC 0, PC -> 1 */
    ASSERT_EQ("PC after ION without interrupt", 1, pdp8_api_get_pc(cpu));
    
    /* Reset and try again, but this time request interrupt AFTER ION but BEFORE AND */
    pdp8_api_reset(cpu);
    pdp8_api_write_mem(cpu, 0, 0x0F01);  /* ION */
    pdp8_api_write_mem(cpu, 1, 0x0000);  /* AND */
    
    /* Execute ION to enable interrupts */
    pdp8_api_step(cpu);  /* PC: 0 -> 1 */
    ASSERT_EQ("PC after ION", 1, pdp8_api_get_pc(cpu));
    
    /* Now request interrupt */
    pdp8_api_request_interrupt(cpu, 055);
    ASSERT_INT_EQ("interrupt pending", 1, pdp8_api_peek_interrupt_pending(cpu));
    
    /* Execute AND - interrupt should dispatch at end of this instruction */
    pdp8_api_step(cpu);  /* PC: 1 -> 2, then dispatch triggers: saves context, PC -> 0x0010 */
    
    /* After dispatch, PC should be at ISR */
    ASSERT_INT_EQ("PC after dispatch", 0x0010, pdp8_api_get_pc(cpu));
    ASSERT_INT_EQ("AC saved at 0x0006", 0, pdp8_api_read_mem(cpu, 0x0006));
    ASSERT_INT_EQ("PC saved at 0x0007", 2, pdp8_api_read_mem(cpu, 0x0007));
    ASSERT_INT_EQ("pending count after dispatch", 0, pdp8_api_peek_interrupt_pending(cpu));
    
    pdp8_api_destroy(cpu);
    return 1;
}

int main(int argc, char **argv) {
    int verbose = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"memory reference", test_memory_reference},
        {"indirect", test_indirect_and_auto_increment},
        {"operate group 1", test_operate_group1},
        {"operate group 2", test_operate_group2},
        {"iot", test_iot},
        {"magtape sense", test_magtape_sense_reports_status},
        {"clear halt", test_clear_halt},
        {"kl8e console", test_kl8e_console},
        {"line printer", test_line_printer},
        //{"core fixture", test_demo_core_fixture},
        //{"paper tape parser", test_paper_tape_parser},
        //{"paper tape device", test_paper_tape_device},
        {"fruit jam board", test_board_spec},
        {"ion ioff", test_ion_ioff},
        {"interrupt pending count", test_interrupt_pending_count},
        {"interrupt dispatch", test_interrupt_dispatch},
    };

    size_t total = sizeof(tests) / sizeof(tests[0]);
    size_t passed = 0;
    for (size_t i = 0; i < total; ++i) {
        int result = tests[i].fn();
        if (result) {
            ++passed;
            if (verbose) {
                printf("%s: PASS\n", tests[i].name);
            }
            continue;
        }

        if (verbose) {
            printf("%s: FAIL\n", tests[i].name);
        }
        fprintf(stderr, "Test failed: %s\n", tests[i].name);
        break;
    }

    if (passed == total) {
        printf("All %zu tests passed.\n", total);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
