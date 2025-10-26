#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/emulator/line_printer.h"
#include "../src/emulator/kl8e_console.h"
#include "../src/emulator/paper_tape_device.h"
#include "../src/emulator/magtape_device.h"
#include "../src/monitor_config.h"
#include "../src/monitor_platform.h"

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    unsigned char buffer[64];
} md5_ctx;

static void md5_transform(uint32_t state[4], const unsigned char block[64]);

static void md5_init(md5_ctx *ctx) {
    ctx->count[0] = ctx->count[1] = 0u;
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
}

static void md5_update(md5_ctx *ctx, const unsigned char *input, size_t len) {
    size_t index = (ctx->count[0] >> 3) & 0x3Fu;
    uint32_t bit_len = (uint32_t)(len << 3);

    ctx->count[0] += bit_len;
    if (ctx->count[0] < bit_len) {
        ctx->count[1]++;
    }
    ctx->count[1] += (uint32_t)(len >> 29);

    size_t part_len = 64u - index;
    size_t i = 0;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], input, part_len);
        md5_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63u < len; i += 64u) {
            md5_transform(ctx->state, &input[i]);
        }
        index = 0;
    }

    memcpy(&ctx->buffer[index], &input[i], len - i);
}

static void md5_encode(unsigned char *output, const uint32_t *input, size_t len) {
    for (size_t i = 0, j = 0; j < len; ++i, j += 4u) {
        output[j] = (unsigned char)(input[i] & 0xFFu);
        output[j + 1u] = (unsigned char)((input[i] >> 8) & 0xFFu);
        output[j + 2u] = (unsigned char)((input[i] >> 16) & 0xFFu);
        output[j + 3u] = (unsigned char)((input[i] >> 24) & 0xFFu);
    }
}

static void md5_decode(uint32_t *output, const unsigned char *input, size_t len) {
    for (size_t i = 0, j = 0; j < len; ++i, j += 4u) {
        output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j + 1u]) << 8) |
                    (((uint32_t)input[j + 2u]) << 16) | (((uint32_t)input[j + 3u]) << 24);
    }
}

static void md5_final(unsigned char digest[16], md5_ctx *ctx) {
    static const unsigned char padding[64] = {0x80};
    unsigned char bits[8];

    md5_encode(bits, ctx->count, 8u);

    size_t index = (ctx->count[0] >> 3) & 0x3Fu;
    size_t pad_len = (index < 56u) ? (56u - index) : (120u - index);
    md5_update(ctx, padding, pad_len);
    md5_update(ctx, bits, 8u);

    md5_encode(digest, ctx->state, 16u);
    memset(ctx, 0, sizeof(*ctx));
}

#define F_MD5(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G_MD5(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H_MD5(x, y, z) ((x) ^ (y) ^ (z))
#define I_MD5(x, y, z) ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define STEP_MD5(func, a, b, c, d, x, s, ac) \
    do {                                     \
        (a) += func((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s));         \
        (a) += (b);                          \
    } while (0)

static void md5_transform(uint32_t state[4], const unsigned char block[64]) {
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t x[16];

    md5_decode(x, block, 64u);

    STEP_MD5(F_MD5, a, b, c, d, x[0], 7, 0xd76aa478);
    STEP_MD5(F_MD5, d, a, b, c, x[1], 12, 0xe8c7b756);
    STEP_MD5(F_MD5, c, d, a, b, x[2], 17, 0x242070db);
    STEP_MD5(F_MD5, b, c, d, a, x[3], 22, 0xc1bdceee);
    STEP_MD5(F_MD5, a, b, c, d, x[4], 7, 0xf57c0faf);
    STEP_MD5(F_MD5, d, a, b, c, x[5], 12, 0x4787c62a);
    STEP_MD5(F_MD5, c, d, a, b, x[6], 17, 0xa8304613);
    STEP_MD5(F_MD5, b, c, d, a, x[7], 22, 0xfd469501);
    STEP_MD5(F_MD5, a, b, c, d, x[8], 7, 0x698098d8);
    STEP_MD5(F_MD5, d, a, b, c, x[9], 12, 0x8b44f7af);
    STEP_MD5(F_MD5, c, d, a, b, x[10], 17, 0xffff5bb1);
    STEP_MD5(F_MD5, b, c, d, a, x[11], 22, 0x895cd7be);
    STEP_MD5(F_MD5, a, b, c, d, x[12], 7, 0x6b901122);
    STEP_MD5(F_MD5, d, a, b, c, x[13], 12, 0xfd987193);
    STEP_MD5(F_MD5, c, d, a, b, x[14], 17, 0xa679438e);
    STEP_MD5(F_MD5, b, c, d, a, x[15], 22, 0x49b40821);

    STEP_MD5(G_MD5, a, b, c, d, x[1], 5, 0xf61e2562);
    STEP_MD5(G_MD5, d, a, b, c, x[6], 9, 0xc040b340);
    STEP_MD5(G_MD5, c, d, a, b, x[11], 14, 0x265e5a51);
    STEP_MD5(G_MD5, b, c, d, a, x[0], 20, 0xe9b6c7aa);
    STEP_MD5(G_MD5, a, b, c, d, x[5], 5, 0xd62f105d);
    STEP_MD5(G_MD5, d, a, b, c, x[10], 9, 0x02441453);
    STEP_MD5(G_MD5, c, d, a, b, x[15], 14, 0xd8a1e681);
    STEP_MD5(G_MD5, b, c, d, a, x[4], 20, 0xe7d3fbc8);
    STEP_MD5(G_MD5, a, b, c, d, x[9], 5, 0x21e1cde6);
    STEP_MD5(G_MD5, d, a, b, c, x[14], 9, 0xc33707d6);
    STEP_MD5(G_MD5, c, d, a, b, x[3], 14, 0xf4d50d87);
    STEP_MD5(G_MD5, b, c, d, a, x[8], 20, 0x455a14ed);
    STEP_MD5(G_MD5, a, b, c, d, x[13], 5, 0xa9e3e905);
    STEP_MD5(G_MD5, d, a, b, c, x[2], 9, 0xfcefa3f8);
    STEP_MD5(G_MD5, c, d, a, b, x[7], 14, 0x676f02d9);
    STEP_MD5(G_MD5, b, c, d, a, x[12], 20, 0x8d2a4c8a);

    STEP_MD5(H_MD5, a, b, c, d, x[5], 4, 0xfffa3942);
    STEP_MD5(H_MD5, d, a, b, c, x[8], 11, 0x8771f681);
    STEP_MD5(H_MD5, c, d, a, b, x[11], 16, 0x6d9d6122);
    STEP_MD5(H_MD5, b, c, d, a, x[14], 23, 0xfde5380c);
    STEP_MD5(H_MD5, a, b, c, d, x[1], 4, 0xa4beea44);
    STEP_MD5(H_MD5, d, a, b, c, x[4], 11, 0x4bdecfa9);
    STEP_MD5(H_MD5, c, d, a, b, x[7], 16, 0xf6bb4b60);
    STEP_MD5(H_MD5, b, c, d, a, x[10], 23, 0xbebfbc70);
    STEP_MD5(H_MD5, a, b, c, d, x[13], 4, 0x289b7ec6);
    STEP_MD5(H_MD5, d, a, b, c, x[0], 11, 0xeaa127fa);
    STEP_MD5(H_MD5, c, d, a, b, x[3], 16, 0xd4ef3085);
    STEP_MD5(H_MD5, b, c, d, a, x[6], 23, 0x04881d05);
    STEP_MD5(H_MD5, a, b, c, d, x[9], 4, 0xd9d4d039);
    STEP_MD5(H_MD5, d, a, b, c, x[12], 11, 0xe6db99e5);
    STEP_MD5(H_MD5, c, d, a, b, x[15], 16, 0x1fa27cf8);
    STEP_MD5(H_MD5, b, c, d, a, x[2], 23, 0xc4ac5665);

    STEP_MD5(I_MD5, a, b, c, d, x[0], 6, 0xf4292244);
    STEP_MD5(I_MD5, d, a, b, c, x[7], 10, 0x432aff97);
    STEP_MD5(I_MD5, c, d, a, b, x[14], 15, 0xab9423a7);
    STEP_MD5(I_MD5, b, c, d, a, x[5], 21, 0xfc93a039);
    STEP_MD5(I_MD5, a, b, c, d, x[12], 6, 0x655b59c3);
    STEP_MD5(I_MD5, d, a, b, c, x[3], 10, 0x8f0ccc92);
    STEP_MD5(I_MD5, c, d, a, b, x[10], 15, 0xffeff47d);
    STEP_MD5(I_MD5, b, c, d, a, x[1], 21, 0x85845dd1);
    STEP_MD5(I_MD5, a, b, c, d, x[8], 6, 0x6fa87e4f);
    STEP_MD5(I_MD5, d, a, b, c, x[15], 10, 0xfe2ce6e0);
    STEP_MD5(I_MD5, c, d, a, b, x[6], 15, 0xa3014314);
    STEP_MD5(I_MD5, b, c, d, a, x[13], 21, 0x4e0811a1);
    STEP_MD5(I_MD5, a, b, c, d, x[4], 6, 0xf7537e82);
    STEP_MD5(I_MD5, d, a, b, c, x[11], 10, 0xbd3af235);
    STEP_MD5(I_MD5, c, d, a, b, x[2], 15, 0x2ad7d2bb);
    STEP_MD5(I_MD5, b, c, d, a, x[9], 21, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    memset(x, 0, sizeof x);
}

#undef F_MD5
#undef G_MD5
#undef H_MD5
#undef I_MD5
#undef STEP_MD5

struct monitor_runtime {
    pdp8_t *cpu;
    pdp8_kl8e_console_t *console;
    pdp8_line_printer_t *printer;
    pdp8_paper_tape_device_t *paper_tape;
    pdp8_magtape_device_t *magtape;
    struct monitor_config config;
    bool config_loaded;
    size_t memory_words;
};

static pdp8_kl8e_console_t *g_console = NULL;

static void monitor_console_write(const char *text, size_t length) {
    if (!text) {
        return;
    }
    for (size_t i = 0; i < length; ++i) {
        monitor_platform_console_putc((uint8_t)text[i]);
    }
}

static void monitor_console_vprintf(const char *fmt, va_list args) {
    if (!fmt) {
        return;
    }

    char stack_buffer[256];
    va_list copy;
    va_copy(copy, args);
    int required = vsnprintf(stack_buffer, sizeof stack_buffer, fmt, copy);
    va_end(copy);
    if (required < 0) {
        return;
    }

    if ((size_t)required < sizeof stack_buffer) {
        monitor_console_write(stack_buffer, (size_t)required);
        return;
    }

    size_t heap_size = (size_t)required + 1u;
    char *heap_buffer = (char *)malloc(heap_size);
    if (!heap_buffer) {
        return;
    }

    va_list second_copy;
    va_copy(second_copy, args);
    vsnprintf(heap_buffer, heap_size, fmt, second_copy);
    va_end(second_copy);
    monitor_console_write(heap_buffer, (size_t)required);
    free(heap_buffer);
}

static void monitor_console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    monitor_console_vprintf(fmt, args);
    va_end(args);
    monitor_platform_console_flush();
}

static void monitor_console_puts(const char *text) {
    if (text) {
        monitor_console_write(text, strlen(text));
    }
    monitor_platform_console_putc('\n');
    monitor_platform_console_flush();
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

    monitor_console_printf("Configuration source: %s\n",
                           config_loaded ? "pdp8.config" : "built-in defaults");

    monitor_console_puts("Devices:");
    monitor_console_puts("  KL8E console");
    monitor_console_printf("    keyboard IOT     : %s\n", keyboard_iot);
    monitor_console_printf("    teleprinter IOT  : %s\n", teleprinter_iot);
    monitor_console_printf("    keyboard input   : %s\n", keyboard_input);
    monitor_console_printf("    teleprinter output: %s\n", teleprinter_output);

    monitor_console_puts("  Line printer");
    monitor_console_printf("    IOT              : %s\n", line_printer_iot);
    monitor_console_printf("    output           : %s\n", line_printer_output);
    monitor_console_printf("    column limit     : %d\n", line_printer_columns);

    monitor_console_puts("  Paper tape");
    monitor_console_printf("    IOT              : %s\n", paper_tape_iot);
    if (paper_tape_device) {
        monitor_console_printf("    image            : %s\n", paper_tape_image);
        monitor_console_printf("    label            : %s\n",
                               paper_tape_label ? paper_tape_label : "(none)");
    } else {
        monitor_console_printf("    image            : %s\n", paper_tape_image);
        monitor_console_puts("    status           : (not attached)");
    }
}

static void show_magtape(const struct monitor_runtime *runtime) {
    if (!runtime) {
        return;
    }
    if (!runtime->magtape) {
        monitor_console_puts("Magtape: (device not attached).");
        return;
    }

    bool any = false;
    for (unsigned unit = 0; unit < MONITOR_MAX_MAGTAPE_UNITS; ++unit) {
        struct pdp8_magtape_unit_status status;
        if (pdp8_magtape_device_get_status(runtime->magtape, unit, &status) != 0) {
            continue;
        }
        if (!status.configured) {
            continue;
        }
        any = true;
        monitor_console_printf("Unit %u (%s)\n",
                               status.unit_number,
                               status.write_protected ? "read-only" : "read/write");
        monitor_console_printf("  path            : %s\n",
                               status.path ? status.path : "(unconfigured)");
        if (status.current_record) {
            monitor_console_printf("  current record  : %s\n", status.current_record);
            if (status.word_count > 0u) {
                monitor_console_printf("  position        : %zu / %zu word(s)%s\n",
                                       status.word_position,
                                       status.word_count,
                                       status.partial_record ? " (partial)" : "");
            } else {
                monitor_console_printf("  position        : %zu word(s)%s\n",
                                       status.word_position,
                                       status.partial_record ? " (partial)" : "");
            }
        } else {
            monitor_console_puts("  current record  : (none)");
        }
        monitor_console_printf("  ready=%s eor=%s eot=%s error=%s\n",
                               status.ready ? "yes" : "no",
                               status.end_of_record ? "yes" : "no",
                               status.end_of_tape ? "yes" : "no",
                               status.error ? "yes" : "no");
    }

    if (!any) {
        monitor_console_puts("Magtape: no configured units.");
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
        monitor_console_printf("Unable to open '%s' for reading: %s\n", path, strerror(errno));
        return -1;
    }

    uint8_t *byte_data = (uint8_t *)calloc(memory_bytes, sizeof(uint8_t));
    bool *byte_present = (bool *)calloc(memory_bytes, sizeof(bool));
    if (!byte_data || !byte_present) {
        monitor_console_printf("Unable to allocate buffer for '%s'.\n", path);
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

    while (fgets(line, sizeof line, fp) != NULL) {
        char *cursor = line;
        size_t raw_len = strlen(cursor);
        md5_update(&md5, (const unsigned char *)cursor, raw_len);

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
                monitor_console_printf("Malformed S-record line (too short): %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            char count_str[3] = {cursor[2], cursor[3], '\0'};
            char *endptr = NULL;
            unsigned long count = strtoul(count_str, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                monitor_console_printf("Malformed byte count in line: %s\n", cursor);
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
                monitor_console_printf("Malformed address in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            const size_t checksum_offset = len - 2u;
            if (checksum_offset < 4u + addr_digits) {
                monitor_console_printf("Malformed data section in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            const char *data_ptr = cursor + 4 + addr_digits;
            const size_t data_hex_len = checksum_offset - (size_t)(data_ptr - cursor);
            if (data_hex_len % 2u != 0) {
                monitor_console_printf("Odd data length in line: %s\n", cursor);
                free(byte_data);
                free(byte_present);
                fclose(fp);
                return -1;
            }

            const size_t data_bytes = data_hex_len / 2u;
            if (count != data_bytes + addr_bytes + 1u) {
                monitor_console_printf("Count mismatch for line: %s\n", cursor);
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
                    monitor_console_printf("Malformed data byte in line: %s\n", cursor);
                    free(byte_data);
                    free(byte_present);
                    fclose(fp);
                    return -1;
                }

                const size_t absolute = base_address + i;
                if (absolute >= memory_bytes) {
                    monitor_console_printf(
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
        monitor_console_printf("No data records found in '%s'.\n", path);
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
                monitor_console_printf("Failed to write memory at %04zo.\n", word);
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
        monitor_console_printf("Parsed data from '%s' but no complete words were written.\n",
                               path);
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
        monitor_console_printf(
            "Warning: Incomplete word(s) encountered while reading '%s'; skipped those entries.\n",
            path);
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

enum monitor_command_status {
    MONITOR_COMMAND_OK = 0,
    MONITOR_COMMAND_ERROR = -1,
    MONITOR_COMMAND_EXIT = 1
};

struct monitor_command;

typedef enum monitor_command_status (*monitor_command_handler)(struct monitor_runtime *runtime,
                                                               char **state);

static int run_with_console(struct monitor_runtime *runtime, size_t cycles);

struct monitor_command {
    const char *name;
    monitor_command_handler handler;
    const char *usage;
    const char *description;
    bool show_in_help;
};

static char *command_next_token(char **state) {
    if (!state) {
        return NULL;
    }
    return strtok_r(NULL, " \t", state);
}

static enum monitor_command_status command_help(struct monitor_runtime *runtime,
                                                char **state);
static enum monitor_command_status command_quit(struct monitor_runtime *runtime,
                                                char **state);
static enum monitor_command_status command_regs(struct monitor_runtime *runtime,
                                                char **state);
static enum monitor_command_status command_switch(struct monitor_runtime *runtime,
                                                 char **state);
static enum monitor_command_status command_mem(struct monitor_runtime *runtime,
                                               char **state);
static enum monitor_command_status command_dep(struct monitor_runtime *runtime,
                                               char **state);
static enum monitor_command_status command_continue(struct monitor_runtime *runtime,
                                                    char **state);
static enum monitor_command_status command_run(struct monitor_runtime *runtime,
                                               char **state);
static enum monitor_command_status command_save(struct monitor_runtime *runtime,
                                                char **state);
static enum monitor_command_status command_restore(struct monitor_runtime *runtime,
                                                   char **state);
static enum monitor_command_status command_read(struct monitor_runtime *runtime,
                                                char **state);
static enum monitor_command_status command_show(struct monitor_runtime *runtime,
                                                char **state);
static enum monitor_command_status command_magtape(struct monitor_runtime *runtime,
                                                  char **state);
static enum monitor_command_status command_reset(struct monitor_runtime *runtime,
                                                 char **state);
static enum monitor_command_status command_trace(struct monitor_runtime *runtime,
                                                 char **state);

static const struct monitor_command monitor_commands[] = {
    {"help", command_help, "help [command]", "Show command list or detailed help.", true},
    {"quit", command_quit, "quit", "Exit the monitor.", true},
    {"exit", command_quit, "exit", "Exit the monitor (alias of quit).", false},
    {"regs", command_regs, "regs", "Show registers and halt state.", true},
    {"switch",
     command_switch,
     "switch [value|load [value]]",
     "Show/set the front-panel switch register; 'load' copies it to PC.",
     true},
    {"mem", command_mem, "mem <addr> [count]", "Dump memory words (octal).", true},
    {"dep", command_dep, "dep <addr> <w0> [w1 ...]", "Deposit consecutive memory words.", true},
    {"c", command_continue, "c [cycles]", "Continue execution (default 1 cycle).", true},
    {"t", command_trace, "t [cycles]", "Execute N cycles (default 1), showing registers after each.", true},
    {"run", command_run, "run <addr> <cycles>", "Set PC and execute for a number of cycles.", true},
    {"save", command_save, "save <file>", "Write RAM image to a file.", true},
    {"restore", command_restore, "restore <file>", "Load RAM image from a file.", true},
    {"read", command_read, "read <file>", "Load Motorola S-record image.", true},
    {"show", command_show, "show devices", "Display configured peripherals.", true},
    {"magtape",
     command_magtape,
     "magtape <rewind|new> <unit>",
     "Control magnetic tape units (see 'show magtape').",
     true},
    {"reset", command_reset, "reset", "Reset CPU and reload board ROM.", true},
};

static const struct monitor_command *find_command(const char *name) {
    if (!name) {
        return NULL;
    }
    size_t count = sizeof(monitor_commands) / sizeof(monitor_commands[0]);
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(monitor_commands[i].name, name) == 0) {
            return &monitor_commands[i];
        }
    }
    return NULL;
}

static void print_help_listing(void) {
    monitor_console_puts("Commands:");
    size_t count = sizeof(monitor_commands) / sizeof(monitor_commands[0]);
    for (size_t i = 0; i < count; ++i) {
        const struct monitor_command *cmd = &monitor_commands[i];
        if (!cmd->show_in_help) {
            continue;
        }
        monitor_console_printf("  %-27s %s\n",
                               cmd->usage ? cmd->usage : cmd->name,
                               cmd->description ? cmd->description : "");
    }
    monitor_console_puts("Notes: numbers default to octal; prefix with '#' for decimal or 0x for hex.");
}

#define MONITOR_RUN_SLICE 2000u

static void monitor_runtime_init(struct monitor_runtime *runtime) {
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    monitor_config_init(&runtime->config);
    runtime->config_loaded = false;
}

static enum monitor_command_status command_help(struct monitor_runtime *runtime,
                                                char **state) {
    (void)runtime;
    char *topic = command_next_token(state);
    if (!topic) {
        print_help_listing();
        return MONITOR_COMMAND_OK;
    }

    const struct monitor_command *cmd = find_command(topic);
    if (!cmd) {
        monitor_console_printf("Unknown command '%s'. Type 'help' for a list.\n", topic);
        return MONITOR_COMMAND_ERROR;
    }

    const char *usage = cmd->usage ? cmd->usage : cmd->name;
    const char *desc = cmd->description ? cmd->description : "";
    monitor_console_printf("%s\n", usage);
    if (desc[0] != '\0') {
        monitor_console_printf("  %s\n", desc);
    }
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_quit(struct monitor_runtime *runtime,
                                                char **state) {
    (void)runtime;
    (void)state;
    return MONITOR_COMMAND_EXIT;
}

static enum monitor_command_status command_regs(struct monitor_runtime *runtime,
                                                char **state) {
    (void)state;
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    uint16_t pc = pdp8_api_get_pc(runtime->cpu);
    uint16_t ac = pdp8_api_get_ac(runtime->cpu);
    uint8_t link = pdp8_api_get_link(runtime->cpu);
    uint16_t sw = pdp8_api_get_switch_register(runtime->cpu);
    bool halted = pdp8_api_is_halted(runtime->cpu) != 0;
    monitor_console_printf("PC=%04o AC=%04o LINK=%o SW=%04o HALT=%s\n",
                           pc & 0x0FFFu,
                           ac & 0x0FFFu,
                           link & 0x1u,
                           sw & 0x0FFFu,
                           halted ? "yes" : "no");
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_switch(struct monitor_runtime *runtime,
                                                 char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *first = command_next_token(state);
    if (!first) {
        uint16_t current = pdp8_api_get_switch_register(runtime->cpu) & 0x0FFFu;
        monitor_console_printf("Switch register: %04o\n", current);
        return MONITOR_COMMAND_OK;
    }

    if (strcmp(first, "load") == 0) {
        uint16_t value = pdp8_api_get_switch_register(runtime->cpu) & 0x0FFFu;

        char *value_tok = command_next_token(state);
        if (value_tok) {
            long parsed = 0;
            if (parse_number(value_tok, &parsed) != 0 || parsed < 0 || parsed > 0x0FFF) {
                monitor_console_printf("Invalid switch value '%s'.\n", value_tok);
                return MONITOR_COMMAND_ERROR;
            }
            value = (uint16_t)parsed;
            pdp8_api_set_switch_register(runtime->cpu, value);
        }

        char *extra = command_next_token(state);
        if (extra) {
            monitor_console_puts("switch load takes at most one value.");
            return MONITOR_COMMAND_ERROR;
        }

        pdp8_api_set_pc(runtime->cpu, value & 0x0FFFu);
        monitor_console_printf("PC loaded from switch register: %04o\n", value & 0x0FFFu);
        return MONITOR_COMMAND_OK;
    }

    long parsed_value = 0;
    if (parse_number(first, &parsed_value) != 0 || parsed_value < 0 || parsed_value > 0x0FFF) {
        monitor_console_printf("Invalid switch value '%s'.\n", first);
        return MONITOR_COMMAND_ERROR;
    }

    char *extra = command_next_token(state);
    if (extra) {
        monitor_console_puts("switch takes at most one value.");
        return MONITOR_COMMAND_ERROR;
    }

    pdp8_api_set_switch_register(runtime->cpu, (uint16_t)parsed_value);
    monitor_console_printf("Switch register set to %04o.\n", (unsigned)parsed_value & 0x0FFFu);
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_mem(struct monitor_runtime *runtime,
                                               char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *addr_tok = command_next_token(state);
    if (!addr_tok) {
        monitor_console_puts("mem requires address.");
        return MONITOR_COMMAND_ERROR;
    }

    long addr_val = 0;
    if (parse_number(addr_tok, &addr_val) != 0 || addr_val < 0) {
        monitor_console_printf("Invalid address '%s'.\n", addr_tok);
        return MONITOR_COMMAND_ERROR;
    }

    char *count_tok = command_next_token(state);
    long count_val = 8;
    if (count_tok) {
        if (parse_number(count_tok, &count_val) != 0 || count_val <= 0) {
            monitor_console_printf("Invalid count '%s'.\n", count_tok);
            return MONITOR_COMMAND_ERROR;
        }
    }

    size_t address = (size_t)addr_val;
    for (long i = 0; i < count_val; ++i) {
        size_t current = (address + (size_t)i) % runtime->memory_words;
        uint16_t word = pdp8_api_read_mem(runtime->cpu, (uint16_t)current);

        if (i % 8 == 0) {
            if (i > 0) {
                monitor_platform_console_putc('\n');
            }
            monitor_console_printf("%04zo:", current);
        }

        monitor_console_printf(" %04o", word & 0x0FFFu);
    }
    if (count_val > 0) {
        monitor_platform_console_putc('\n');
        monitor_platform_console_flush();
    }
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_dep(struct monitor_runtime *runtime,
                                               char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *addr_tok = command_next_token(state);
    if (!addr_tok) {
        monitor_console_puts("dep requires address.");
        return MONITOR_COMMAND_ERROR;
    }

    long addr_val = 0;
    if (parse_number(addr_tok, &addr_val) != 0 || addr_val < 0) {
        monitor_console_printf("Invalid address '%s'.\n", addr_tok);
        return MONITOR_COMMAND_ERROR;
    }

    size_t loaded = 0;
    for (char *word_tok = command_next_token(state); word_tok != NULL;
         word_tok = command_next_token(state)) {
        long word_val = 0;
        if (parse_number(word_tok, &word_val) != 0 || word_val < 0 || word_val > 0x0FFF) {
            monitor_console_printf("Invalid word '%s'.\n", word_tok);
            loaded = 0;
            break;
        }

        size_t current = ((size_t)addr_val + loaded) % runtime->memory_words;
        if (pdp8_api_write_mem(runtime->cpu, (uint16_t)current, (uint16_t)word_val) != 0) {
            monitor_console_printf("Failed to write memory at %04zo.\n", current);
            loaded = 0;
            break;
        }
        ++loaded;
    }

    if (loaded > 0) {
        monitor_console_printf("Deposited %zu word(s) starting at %04zo.\n",
                               loaded,
                               (size_t)addr_val % runtime->memory_words);
    }
    return loaded > 0 ? MONITOR_COMMAND_OK : MONITOR_COMMAND_ERROR;
}

static enum monitor_command_status command_continue(struct monitor_runtime *runtime,
                                                    char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *cycles_tok = command_next_token(state);
    long cycles_val = 1;
    if (cycles_tok) {
        if (parse_number(cycles_tok, &cycles_val) != 0 || cycles_val <= 0) {
            monitor_console_printf("Invalid cycle count '%s'.\n", cycles_tok);
            return MONITOR_COMMAND_ERROR;
        }
    }

    pdp8_api_clear_halt(runtime->cpu);
    size_t cycles = (size_t)cycles_val;
    int executed = run_with_console(runtime, cycles);
    if (executed < 0) {
        monitor_console_puts("Continue failed.");
        return MONITOR_COMMAND_ERROR;
    }

    monitor_console_printf("\nExecuted %d cycle(s). PC=%04o HALT=%s\n",
                           executed,
                           pdp8_api_get_pc(runtime->cpu) & 0x0FFFu,
                           pdp8_api_is_halted(runtime->cpu) ? "yes" : "no");
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_run(struct monitor_runtime *runtime,
                                               char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *start_tok = command_next_token(state);
    char *cycles_tok = command_next_token(state);
    if (!start_tok || !cycles_tok) {
        monitor_console_puts("run requires start address and cycle count.");
        return MONITOR_COMMAND_ERROR;
    }

    long start_val = 0;
    if (parse_number(start_tok, &start_val) != 0 || start_val < 0) {
        monitor_console_printf("Invalid start address '%s'.\n", start_tok);
        return MONITOR_COMMAND_ERROR;
    }
    if ((size_t)start_val >= runtime->memory_words) {
        monitor_console_printf("Start address %04lo exceeds memory size.\n", start_val);
        return MONITOR_COMMAND_ERROR;
    }

    long cycles_val = 0;
    if (parse_number(cycles_tok, &cycles_val) != 0 || cycles_val <= 0) {
        monitor_console_printf("Invalid cycle count '%s'.\n", cycles_tok);
        return MONITOR_COMMAND_ERROR;
    }

    pdp8_api_clear_halt(runtime->cpu);
    pdp8_api_set_pc(runtime->cpu, (uint16_t)(start_val & 0x0FFFu));
    size_t cycles = (size_t)cycles_val;
    int executed = run_with_console(runtime, cycles);
    if (executed < 0) {
        monitor_console_puts("Run failed.");
        return MONITOR_COMMAND_ERROR;
    }

    monitor_console_printf("\nExecuted %d cycle(s). PC=%04o HALT=%s\n",
                           executed,
                           pdp8_api_get_pc(runtime->cpu) & 0x0FFFu,
                           pdp8_api_is_halted(runtime->cpu) ? "yes" : "no");
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_save(struct monitor_runtime *runtime,
                                                char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *path = command_next_token(state);
    if (!path) {
        monitor_console_puts("save requires file path.");
        return MONITOR_COMMAND_ERROR;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        monitor_console_printf("Unable to open '%s' for writing: %s\n", path, strerror(errno));
        return MONITOR_COMMAND_ERROR;
    }

    size_t written = 0;
    for (size_t addr = 0; addr < runtime->memory_words; ++addr) {
        uint16_t word = pdp8_api_read_mem(runtime->cpu, (uint16_t)addr);
        if (fwrite(&word, sizeof word, 1, fp) != 1) {
            monitor_console_printf("Write failed at address %04zo: %s\n", addr, strerror(errno));
            fclose(fp);
            return MONITOR_COMMAND_ERROR;
        }
        ++written;
    }

    fclose(fp);
    monitor_console_printf("Saved %zu word(s) to %s.\n", written, path);
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_restore(struct monitor_runtime *runtime,
                                                   char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *path = command_next_token(state);
    if (!path) {
        monitor_console_puts("restore requires file path.");
        return MONITOR_COMMAND_ERROR;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        monitor_console_printf("Unable to open '%s' for reading: %s\n", path, strerror(errno));
        return MONITOR_COMMAND_ERROR;
    }

    size_t restored = 0;
    uint16_t word = 0;
    while (restored < runtime->memory_words && fread(&word, sizeof word, 1, fp) == 1) {
        pdp8_api_write_mem(runtime->cpu, (uint16_t)restored, word);
        ++restored;
    }

    if (!feof(fp)) {
        monitor_console_printf("Read error while restoring: %s\n", strerror(errno));
        fclose(fp);
        return MONITOR_COMMAND_ERROR;
    }

    fclose(fp);
    monitor_console_printf("Restored %zu word(s) from %s.\n", restored, path);
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_read(struct monitor_runtime *runtime,
                                                char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *path = command_next_token(state);
    if (!path) {
        monitor_console_puts("read requires file path.");
        return MONITOR_COMMAND_ERROR;
    }

    size_t words_loaded = 0;
    size_t highest_address = 0;
    uint16_t start_address = 0;
    bool start_valid = false;
    unsigned char md5_bytes[16];

    if (load_srec_image(runtime->cpu,
                        path,
                        runtime->memory_words,
                        &words_loaded,
                        &highest_address,
                        &start_address,
                        &start_valid,
                        md5_bytes) != 0) {
        return MONITOR_COMMAND_ERROR;
    }

    monitor_console_printf("Loaded %zu word(s) from %s", words_loaded, path);
    if (words_loaded > 0) {
        monitor_console_printf(" (last %04zo)", highest_address);
    }
    monitor_console_puts(".");

    char md5_hex[33];
    for (size_t i = 0; i < sizeof md5_bytes; ++i) {
        snprintf(&md5_hex[i * 2u], 3u, "%02x", md5_bytes[i] & 0xFFu);
    }
    md5_hex[32] = '\0';
    monitor_console_printf("SREC md5 (%s): %s\n", path, md5_hex);

    if (start_valid) {
        pdp8_api_set_pc(runtime->cpu, start_address & 0x0FFFu);
        monitor_console_printf("Start address %04o set as PC.\n", start_address & 0x0FFFu);
    }

    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_show(struct monitor_runtime *runtime,
                                                char **state) {
    if (!runtime) {
        return MONITOR_COMMAND_ERROR;
    }

    char *topic = command_next_token(state);
    if (!topic) {
        monitor_console_puts("show requires a subject (e.g. 'show devices').");
        return MONITOR_COMMAND_ERROR;
    }

    if (strcmp(topic, "devices") == 0) {
        show_devices(&runtime->config, runtime->config_loaded, runtime->paper_tape);
        return MONITOR_COMMAND_OK;
    }

    if (strcmp(topic, "magtape") == 0) {
        show_magtape(runtime);
        return MONITOR_COMMAND_OK;
    }

    monitor_console_printf("Unknown subject for show: '%s'.\n", topic);
    return MONITOR_COMMAND_ERROR;
}

static enum monitor_command_status command_magtape(struct monitor_runtime *runtime,
                                                  char **state) {
    if (!runtime) {
        return MONITOR_COMMAND_ERROR;
    }

    if (!runtime->magtape) {
        monitor_console_puts("No magnetic tape device is attached.");
        return MONITOR_COMMAND_ERROR;
    }

    char *action = command_next_token(state);
    if (!action) {
        monitor_console_puts("magtape requires an action (rewind|new).");
        return MONITOR_COMMAND_ERROR;
    }

    char *unit_tok = command_next_token(state);
    if (!unit_tok) {
        monitor_console_puts("magtape requires a unit number.");
        return MONITOR_COMMAND_ERROR;
    }

    long unit_value = 0;
    if (parse_number(unit_tok, &unit_value) != 0 || unit_value < 0 || unit_value > INT_MAX) {
        monitor_console_printf("Invalid unit '%s'.\n", unit_tok);
        return MONITOR_COMMAND_ERROR;
    }

    unsigned unit = (unsigned)unit_value;

    if (strcmp(action, "rewind") == 0) {
        if (pdp8_magtape_device_rewind(runtime->magtape, unit) != 0) {
            monitor_console_printf("Unable to rewind magtape unit %u.\n", unit);
            return MONITOR_COMMAND_ERROR;
        }
        monitor_console_printf("Magtape unit %u rewound.\n", unit);
        return MONITOR_COMMAND_OK;
    }

    if (strcmp(action, "new") == 0) {
        if (pdp8_magtape_device_force_new_record(runtime->magtape, unit) != 0) {
            monitor_console_printf("Unable to seal current record for unit %u.\n", unit);
            return MONITOR_COMMAND_ERROR;
        }
        monitor_console_printf("Magtape unit %u will create a new record on next write.\n", unit);
        return MONITOR_COMMAND_OK;
    }

    monitor_console_printf("Unknown magtape action '%s'.\n", action);
    return MONITOR_COMMAND_ERROR;
}

static enum monitor_command_status command_reset(struct monitor_runtime *runtime,
                                                 char **state) {
    (void)state;
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    pdp8_api_reset(runtime->cpu);
    monitor_console_puts("CPU reset.");
    return MONITOR_COMMAND_OK;
}

static enum monitor_command_status command_trace(struct monitor_runtime *runtime,
                                                 char **state) {
    if (!runtime || !runtime->cpu) {
        return MONITOR_COMMAND_ERROR;
    }

    char *cycles_tok = command_next_token(state);
    long cycles_val = 1;  // Default to 1 cycle
    
    if (cycles_tok) {
        if (parse_number(cycles_tok, &cycles_val) != 0 || cycles_val <= 0) {
            monitor_console_printf("Invalid cycle count '%s'.\n", cycles_tok);
            return MONITOR_COMMAND_ERROR;
        }
    }

    pdp8_api_clear_halt(runtime->cpu);
    
    for (long i = 0; i < cycles_val; ++i) {
        // Show state before execution
        uint16_t pc = pdp8_api_get_pc(runtime->cpu);
        uint16_t ac = pdp8_api_get_ac(runtime->cpu);
        uint8_t link = pdp8_api_get_link(runtime->cpu);
        uint16_t instruction = pdp8_api_read_mem(runtime->cpu, pc);
        
        monitor_console_printf("[%04lo] PC=%04o AC=%04o LINK=%o INSTR=%04o -> ",
                               i + 1, pc & 0x0FFFu, ac & 0x0FFFu, link & 0x1u, instruction & 0x0FFFu);
        
        // Execute one step
        int executed = pdp8_api_step(runtime->cpu);
        if (executed <= 0) {
            monitor_console_puts("FAILED");
            return MONITOR_COMMAND_ERROR;
        }
        
        // Show state after execution
        pc = pdp8_api_get_pc(runtime->cpu);
        ac = pdp8_api_get_ac(runtime->cpu);
        link = pdp8_api_get_link(runtime->cpu);
        bool halted = pdp8_api_is_halted(runtime->cpu) != 0;
        
        monitor_console_printf("PC=%04o AC=%04o LINK=%o%s\n",
                               pc & 0x0FFFu, ac & 0x0FFFu, link & 0x1u, halted ? " HALT" : "");
        
        if (halted) {
            monitor_console_printf("CPU halted after 0%04lo cycle(s).\n", i + 1);
            break;
        }
    }
    
    return MONITOR_COMMAND_OK;
}

static void monitor_runtime_teardown(struct monitor_runtime *runtime) {
    if (!runtime) {
        return;
    }
    if (runtime->magtape) {
        pdp8_magtape_device_destroy(runtime->magtape);
        runtime->magtape = NULL;
    }
    if (runtime->paper_tape) {
        pdp8_paper_tape_device_destroy(runtime->paper_tape);
        runtime->paper_tape = NULL;
    }
    if (runtime->printer) {
        pdp8_line_printer_destroy(runtime->printer);
        runtime->printer = NULL;
    }
    if (runtime->console) {
        if (g_console == runtime->console) {
            g_console = NULL;
        }
        pdp8_kl8e_console_destroy(runtime->console);
        runtime->console = NULL;
    }
    if (runtime->cpu) {
        pdp8_api_destroy(runtime->cpu);
        runtime->cpu = NULL;
    }
    runtime->memory_words = 0u;
}

static void service_platform_keyboard(pdp8_kl8e_console_t *console) {
    if (!console) {
        return;
    }

    uint8_t ch = 0;
    while (monitor_platform_poll_keyboard(&ch)) {
        (void)pdp8_kl8e_console_queue_input(console, ch);
    }
}

static int run_with_console(struct monitor_runtime *runtime, size_t cycles) {
    if (!runtime || !runtime->cpu) {
        return -1;
    }

    size_t remaining = cycles;
    int total_executed = 0;

    while (remaining > 0u) {
        service_platform_keyboard(runtime->console);

        size_t request = remaining > MONITOR_RUN_SLICE ? MONITOR_RUN_SLICE : remaining;
        int executed = pdp8_api_run(runtime->cpu, request);
        if (executed <= 0) {
            if (executed < 0) {
                total_executed = executed;
            }
            break;
        }

        total_executed += executed;
        remaining -= (size_t)executed;

        if ((size_t)executed < request || pdp8_api_is_halted(runtime->cpu)) {
            break;
        }

        monitor_platform_idle();
    }

    service_platform_keyboard(runtime->console);
    return total_executed;
}

static bool monitor_runtime_create(struct monitor_runtime *runtime,
                                   const pdp8_board_spec *board) {
    if (!runtime || !board) {
        return false;
    }

    runtime->memory_words = board->memory_words ? board->memory_words : 4096u;

    runtime->cpu = pdp8_api_create_for_board(board);
    if (!runtime->cpu) {
        return false;
    }

    runtime->console = monitor_platform_create_console();
    if (!runtime->console) {
        monitor_runtime_teardown(runtime);
        return false;
    }

    if (pdp8_kl8e_console_attach(runtime->cpu, runtime->console) != 0) {
        monitor_runtime_teardown(runtime);
        return false;
    }

    g_console = runtime->console;

    runtime->printer = monitor_platform_create_printer();
    if (!runtime->printer) {
        monitor_runtime_teardown(runtime);
        return false;
    }

    if (pdp8_line_printer_attach(runtime->cpu, runtime->printer) != 0) {
        monitor_runtime_teardown(runtime);
        return false;
    }

    int printer_columns = 132;
    if (runtime->config_loaded && runtime->config.line_printer_present &&
        runtime->config.line_printer_column_limit > 0) {
        printer_columns = runtime->config.line_printer_column_limit;
    }
    pdp8_line_printer_set_column_limit(runtime->printer, (uint16_t)printer_columns);

    if (runtime->config_loaded && runtime->config.paper_tape_present) {
        if (runtime->config.paper_tape_image && *runtime->config.paper_tape_image) {
            runtime->paper_tape = pdp8_paper_tape_device_create();
            if (!runtime->paper_tape) {
                monitor_console_puts("Warning: unable to create paper tape device.");
            } else if (pdp8_paper_tape_device_load(runtime->paper_tape,
                                                   runtime->config.paper_tape_image) != 0) {
                monitor_console_printf("Warning: unable to load paper tape image '%s'.\n",
                                       runtime->config.paper_tape_image);
                pdp8_paper_tape_device_destroy(runtime->paper_tape);
                runtime->paper_tape = NULL;
            } else if (pdp8_paper_tape_device_attach(runtime->cpu, runtime->paper_tape) != 0) {
                monitor_console_puts("Warning: unable to attach paper tape device (IOT 667x).");
                pdp8_paper_tape_device_destroy(runtime->paper_tape);
                runtime->paper_tape = NULL;
            }
        } else {
            monitor_console_puts(
                "Warning: paper_tape device requested but no image path provided in pdp8.config.");
        }
    }

    bool need_magtape = runtime->config.magtape_unit_count > 0u || !runtime->config_loaded;
    if (need_magtape) {
        runtime->magtape = pdp8_magtape_device_create();
        if (!runtime->magtape) {
            monitor_console_puts("Warning: unable to create magtape controller.");
        } else if (pdp8_magtape_device_attach(runtime->cpu, runtime->magtape) != 0) {
            monitor_console_puts("Warning: unable to attach magtape controller IOT.");
            pdp8_magtape_device_destroy(runtime->magtape);
            runtime->magtape = NULL;
        } else {
            bool configured = false;
            if (runtime->config.magtape_unit_count > 0u) {
                for (size_t i = 0; i < runtime->config.magtape_unit_count; ++i) {
                    const struct monitor_magtape_unit_config *slot = &runtime->config.magtape_units[i];
                    if (!slot->present) {
                        continue;
                    }
                    if (!slot->path || *slot->path == '\0') {
                        monitor_console_printf(
                            "Warning: magtape unit %d missing path in configuration.\n", slot->unit_number);
                        continue;
                    }
                    if (!slot->write_protected) {
                        struct stat st;
                        if (stat(slot->path, &st) != 0) {
                            if (mkdir(slot->path, 0755) != 0 && errno != EEXIST) {
                                monitor_console_printf(
                                    "Warning: unable to create magtape directory '%s': %s\n",
                                    slot->path,
                                    strerror(errno));
                                continue;
                            }
                        } else if (!S_ISDIR(st.st_mode)) {
                            monitor_console_printf(
                                "Warning: magtape path '%s' is not a directory.\n", slot->path);
                            continue;
                        }
                    }
                    struct pdp8_magtape_unit_params params;
                    params.unit_number = (unsigned)slot->unit_number;
                    params.path = slot->path;
                    params.write_protected = slot->write_protected;
                    if (pdp8_magtape_device_configure_unit(runtime->magtape, &params) != 0) {
                        monitor_console_printf("Warning: failed to configure magtape unit %u.\n",
                                               params.unit_number);
                    } else {
                        configured = true;
                    }
                }
            } else {
                static const struct pdp8_magtape_unit_params defaults[] = {
                    {0u, "demo", true},
                    {1u, "magtape", false},
                };
                for (size_t i = 0; i < sizeof defaults / sizeof defaults[0]; ++i) {
                    const struct pdp8_magtape_unit_params *params = &defaults[i];
                    if (!params->write_protected) {
                        struct stat st;
                        if (stat(params->path, &st) != 0) {
                            if (mkdir(params->path, 0755) != 0 && errno != EEXIST) {
                                monitor_console_printf(
                                    "Warning: unable to create magtape directory '%s': %s\n",
                                    params->path,
                                    strerror(errno));
                                continue;
                            }
                        } else if (!S_ISDIR(st.st_mode)) {
                            monitor_console_printf(
                                "Warning: magtape path '%s' is not a directory.\n", params->path);
                            continue;
                        }
                    }
                    if (pdp8_magtape_device_configure_unit(runtime->magtape, params) != 0) {
                        monitor_console_printf("Warning: failed to configure default magtape unit %u.\n",
                                               params->unit_number);
                    } else {
                        configured = true;
                    }
                }
            }

            if (!configured) {
                monitor_console_puts("Warning: no magtape units available; disabling controller.");
                pdp8_magtape_device_destroy(runtime->magtape);
                runtime->magtape = NULL;
            }
        }
    }

    return true;
}

static int monitor_runtime_loop(struct monitor_runtime *runtime) {
    if (!runtime || !runtime->cpu) {
        return EXIT_FAILURE;
    }

    char line[512];
    monitor_console_puts("PDP-8 Monitor. Type 'help' for commands.");

    while (true) {
        monitor_console_printf("pdp8> ");
        monitor_platform_console_flush();

        if (!monitor_platform_readline(line, sizeof line)) {
            monitor_console_puts("");
            return EXIT_SUCCESS;
        }

        line[strcspn(line, "\r\n")] = '\0';

        char *saveptr = NULL;
        char *cmd_name = strtok_r(line, " \t", &saveptr);
        if (!cmd_name) {
            continue;
        }

        const struct monitor_command *command = find_command(cmd_name);
        if (!command) {
            monitor_console_printf("Unknown command '%s'. Type 'help' for a list.\n", cmd_name);
            continue;
        }

        enum monitor_command_status status = command->handler(runtime, &saveptr);
        if (status == MONITOR_COMMAND_EXIT) {
            return EXIT_SUCCESS;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int exit_code = EXIT_SUCCESS;
    struct monitor_runtime runtime;
    monitor_runtime_init(&runtime);

    const char *startup_image = NULL;
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [image.srec]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        startup_image = argv[1];
    }

    const pdp8_board_spec *board = NULL;
    bool config_loaded = false;
    int config_status = 0;

    if (monitor_platform_init(&runtime.config, &board, &config_loaded, &config_status) != 0 || !board) {
        fprintf(stderr, "Unable to initialise monitor platform.\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }

    runtime.config_loaded = config_loaded;

    if (config_status < 0) {
        fprintf(stderr,
                "Warning: failed to read pdp8.config (%s). Using built-in defaults.\n",
                strerror(-config_status));
    }

    if (!monitor_runtime_create(&runtime, board)) {
        fprintf(stderr, "Unable to prepare monitor runtime.\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }

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
            exit_code = EXIT_FAILURE;
            goto shutdown;
        }

        monitor_console_printf("Loaded %zu word(s) from %s", words_loaded, startup_image);
        if (words_loaded > 0) {
            monitor_console_printf(" (last %04zo)", highest_address);
        }
        monitor_console_puts(".");

        char md5_hex[33];
        for (size_t i = 0; i < sizeof md5_bytes; ++i) {
            snprintf(&md5_hex[i * 2u], 3u, "%02x", md5_bytes[i] & 0xFFu);
        }
        md5_hex[32] = '\0';
        monitor_console_printf("SREC md5 (%s): %s\n", startup_image, md5_hex);

        if (start_valid) {
            pdp8_api_set_pc(runtime.cpu, start_address & 0x0FFFu);
            monitor_console_printf("Start address %04o set as PC.\n", start_address & 0x0FFFu);
        }
    }

    exit_code = monitor_runtime_loop(&runtime);

shutdown:
    monitor_runtime_teardown(&runtime);
    monitor_platform_shutdown();
    monitor_config_clear(&runtime.config);
    return exit_code;
}

void monitor_platform_enqueue_key(uint8_t ch) {
    if (!g_console) {
        return;
    }
    (void)pdp8_kl8e_console_queue_input(g_console, ch);
}
