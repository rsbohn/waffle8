#define _POSIX_C_SOURCE 200809L

#include "magtape_device.h"

#include "pdp8.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAGTAPE_SENTINEL_WORD 0xFFFFu
#define MAGTAPE_STATUS_READY 0x0001u
#define MAGTAPE_STATUS_ERROR 0x0002u
#define MAGTAPE_STATUS_EOR 0x0004u
#define MAGTAPE_STATUS_EOT 0x0008u
#define MAGTAPE_STATUS_WRITE_PROTECT 0x0010u

struct magtape_record {
    char *name;
    uint16_t *words;
    size_t word_count;
    bool partial;
    time_t timestamp;
};

struct magtape_unit {
    bool configured;
    unsigned unit_number;
    char *path;
    bool write_protected;
    struct magtape_record *records;
    size_t record_count;
    size_t record_capacity;
    size_t current_record;
    size_t position;
    bool ready;
    bool error;
    bool end_of_record;
    bool end_of_tape;
    FILE *write_stream;
    char *write_path;
    size_t write_words;
};

struct pdp8_magtape_device {
    struct magtape_unit *units;
    size_t unit_count;
    size_t unit_capacity;
    unsigned selected_unit;
};

static char *duplicate_string(const char *text) {
    if (!text) {
        return NULL;
    }
    size_t len = strlen(text) + 1u;
    char *copy = (char *)malloc(len);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len);
    return copy;
}

static char *join_path(const char *dir, const char *name) {
    if (!dir || !name) {
        return NULL;
    }
    size_t dir_len = strlen(dir);
    size_t needs_sep = (dir_len > 0 && dir[dir_len - 1] == '/') ? 0u : 1u;
    size_t name_len = strlen(name);
    size_t total = dir_len + needs_sep + name_len + 1u;
    char *result = (char *)malloc(total);
    if (!result) {
        return NULL;
    }
    strcpy(result, dir);
    if (needs_sep) {
        result[dir_len] = '/';
        result[dir_len + 1u] = '\0';
    }
    strcat(result, name);
    return result;
}

static bool read_word_le(FILE *fp, uint16_t *out_value) {
    uint8_t bytes[2];
    if (!fp || !out_value) {
        return false;
    }
    size_t read = fread(bytes, 1u, sizeof bytes, fp);
    if (read != sizeof bytes) {
        return false;
    }
    *out_value = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
    return true;
}

static bool write_word_le(FILE *fp, uint16_t value) {
    if (!fp) {
        return false;
    }
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    return fwrite(bytes, 1u, sizeof bytes, fp) == sizeof bytes;
}

static void free_record(struct magtape_record *record) {
    if (!record) {
        return;
    }
    free(record->name);
    free(record->words);
    record->name = NULL;
    record->words = NULL;
    record->word_count = 0u;
    record->partial = false;
    record->timestamp = 0;
}

static void reset_unit_runtime(struct magtape_unit *unit) {
    if (!unit) {
        return;
    }
    unit->current_record = 0u;
    unit->position = 0u;
    unit->ready = false;
    unit->error = false;
    unit->end_of_record = false;
    unit->end_of_tape = false;
}

static void clear_unit_records(struct magtape_unit *unit) {
    if (!unit) {
        return;
    }
    for (size_t i = 0; i < unit->record_count; ++i) {
        free_record(&unit->records[i]);
    }
    free(unit->records);
    unit->records = NULL;
    unit->record_count = 0u;
    unit->record_capacity = 0u;
    reset_unit_runtime(unit);
}

static struct magtape_unit *find_unit(struct pdp8_magtape_device *device, unsigned unit_number) {
    if (!device) {
        return NULL;
    }
    for (size_t i = 0; i < device->unit_count; ++i) {
        if (device->units[i].unit_number == unit_number) {
            return &device->units[i];
        }
    }
    return NULL;
}

static struct magtape_unit *ensure_unit(struct pdp8_magtape_device *device, unsigned unit_number) {
    if (!device) {
        return NULL;
    }
    struct magtape_unit *existing = find_unit(device, unit_number);
    if (existing) {
        return existing;
    }
    if (device->unit_count == device->unit_capacity) {
        size_t new_capacity = device->unit_capacity ? device->unit_capacity * 2u : 4u;
        struct magtape_unit *resized =
            (struct magtape_unit *)realloc(device->units, new_capacity * sizeof(struct magtape_unit));
        if (!resized) {
            return NULL;
        }
        device->units = resized;
        device->unit_capacity = new_capacity;
    }
    struct magtape_unit *slot = &device->units[device->unit_count++];
    memset(slot, 0, sizeof(*slot));
    slot->unit_number = unit_number;
    return slot;
}

static bool ends_with_case_insensitive(const char *name, const char *suffix) {
    if (!name || !suffix) {
        return false;
    }
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > name_len) {
        return false;
    }
    const char *start = name + (name_len - suffix_len);
    for (size_t i = 0; i < suffix_len; ++i) {
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)suffix[i])) {
            return false;
        }
    }
    return true;
}

static int compare_records(const void *lhs, const void *rhs) {
    const struct magtape_record *a = (const struct magtape_record *)lhs;
    const struct magtape_record *b = (const struct magtape_record *)rhs;
    if (a->timestamp < b->timestamp) {
        return -1;
    }
    if (a->timestamp > b->timestamp) {
        return 1;
    }
    if (!a->name && !b->name) {
        return 0;
    }
    if (!a->name) {
        return -1;
    }
    if (!b->name) {
        return 1;
    }
    return strcmp(a->name, b->name);
}

static bool parse_octal_record_index(const char *name, unsigned *out_index) {
    if (!name) {
        return false;
    }
    if (!ends_with_case_insensitive(name, ".tap")) {
        return false;
    }
    size_t len = strlen(name);
    size_t suffix_len = 4u; /* ".tap" */
    if (len <= suffix_len) {
        return false;
    }
    size_t digits = len - suffix_len;
    if (digits < 4u) {
        return false;
    }
    unsigned long value = 0ul;
    for (size_t i = 0; i < digits; ++i) {
        char c = name[i];
        if (c < '0' || c > '7') {
            return false;
        }
        value = (value << 3) | (unsigned long)(c - '0');
        if (value > UINT_MAX) {
            errno = ERANGE;
            return false;
        }
    }
    if (out_index) {
        *out_index = (unsigned)value;
    }
    return true;
}

static size_t octal_digit_count(unsigned value) {
    size_t digits = 1u;
    while (value >= 8u) {
        value >>= 3;
        digits++;
    }
    return digits;
}

static int format_record_filename(unsigned index, char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0u) {
        return -1;
    }
    size_t digits = octal_digit_count(index);
    if (digits < 4u) {
        digits = 4u;
    }
    int written = snprintf(buffer, buffer_len, "%0*o.tap", (int)digits, index);
    if (written < 0 || (size_t)written >= buffer_len) {
        return -1;
    }
    return 0;
}

static int append_record(struct magtape_unit *unit, const struct magtape_record *record) {
    if (!unit || !record) {
        return -1;
    }
    if (unit->record_count == unit->record_capacity) {
        size_t new_capacity = unit->record_capacity ? unit->record_capacity * 2u : 8u;
        struct magtape_record *resized =
            (struct magtape_record *)realloc(unit->records, new_capacity * sizeof(struct magtape_record));
        if (!resized) {
            return -1;
        }
        unit->records = resized;
        unit->record_capacity = new_capacity;
    }
    unit->records[unit->record_count++] = *record;
    return 0;
}

static int read_tap_record(const char *full_path,
                           struct magtape_record *out_record,
                           bool *out_partial) {
    if (out_partial) {
        *out_partial = false;
    }
    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        return -1;
    }

    uint16_t declared_words = 0u;
    if (!read_word_le(fp, &declared_words)) {
        fclose(fp);
        if (out_partial) {
            *out_partial = true;
        }
        return -1;
    }

    uint16_t *words = NULL;
    size_t capacity = declared_words ? declared_words : 16u;
    if (capacity > 0u) {
        words = (uint16_t *)calloc(capacity, sizeof(uint16_t));
        if (!words) {
            fclose(fp);
            return -1;
        }
    }

    size_t count = 0u;
    bool partial = false;
    while (count < declared_words) {
        uint16_t value = 0u;
        if (!read_word_le(fp, &value)) {
            partial = true;
            break;
        }
        words[count++] = value & 0x0FFFu;
    }

    if (!partial) {
        uint16_t sentinel = 0u;
        if (!read_word_le(fp, &sentinel) || sentinel != MAGTAPE_SENTINEL_WORD) {
            partial = true;
        }
    }

    fclose(fp);

    if (partial && out_partial) {
        *out_partial = true;
    }

    if (count < declared_words) {
        uint16_t *resized = (uint16_t *)realloc(words, count * sizeof(uint16_t));
        if (resized) {
            words = resized;
        }
    }

    out_record->words = words;
    out_record->word_count = count;
    out_record->partial = partial;
    return 0;
}

static int load_srec_words(const char *full_path,
                           struct magtape_record *out_record,
                           bool *out_partial) {
    if (out_partial) {
        *out_partial = false;
    }

    FILE *fp = fopen(full_path, "r");
    if (!fp) {
        return -1;
    }

    const size_t memory_words = 4096u;
    const size_t memory_bytes = memory_words * 2u;
    uint8_t *byte_data = (uint8_t *)calloc(memory_bytes, sizeof(uint8_t));
    bool *byte_present = (bool *)calloc(memory_bytes, sizeof(bool));
    if (!byte_data || !byte_present) {
        free(byte_data);
        free(byte_present);
        fclose(fp);
        errno = ENOMEM;
        return -1;
    }

    bool have_data = false;
    char line[1024];
    while (fgets(line, sizeof line, fp) != NULL) {
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        size_t len = strlen(cursor);
        while (len > 0u && isspace((unsigned char)cursor[len - 1u])) {
            cursor[--len] = '\0';
        }
        if (len == 0u || cursor[0] == '\0') {
            continue;
        }
        if (cursor[0] != 'S' && cursor[0] != 's') {
            continue;
        }
        char type = (char)toupper((unsigned char)cursor[1]);
        if (type == '1' || type == '2' || type == '3') {
            size_t addr_digits = (type == '1') ? 4u : (type == '2') ? 6u : 8u;
            if (len < 4u + addr_digits + 2u) {
                continue;
            }
            char count_buf[3] = {cursor[2], cursor[3], '\0'};
            char *endptr = NULL;
            unsigned long count = strtoul(count_buf, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                continue;
            }
            char addr_buf[9];
            if (addr_digits >= sizeof addr_buf) {
                continue;
            }
            memcpy(addr_buf, cursor + 4, addr_digits);
            addr_buf[addr_digits] = '\0';
            unsigned long base_address = strtoul(addr_buf, &endptr, 16);
            if (!endptr || *endptr != '\0') {
                continue;
            }
            size_t checksum_offset = len - 2u;
            if (checksum_offset <= 4u + addr_digits) {
                continue;
            }
            const char *data_ptr = cursor + 4 + addr_digits;
            size_t data_len = checksum_offset - (size_t)(data_ptr - cursor);
            if (data_len % 2u != 0u) {
                continue;
            }
            size_t data_bytes = data_len / 2u;
            size_t addr_bytes = addr_digits / 2u;
            if (count != data_bytes + addr_bytes + 1u) {
                continue;
            }
            for (size_t i = 0; i < data_bytes; ++i) {
                char byte_buf[3] = {data_ptr[i * 2u], data_ptr[i * 2u + 1u], '\0'};
                unsigned long value = strtoul(byte_buf, &endptr, 16);
                if (!endptr || *endptr != '\0' || value > 0xFFul) {
                    continue;
                }
                size_t absolute = base_address + i;
                if (absolute >= memory_bytes) {
                    continue;
                }
                byte_data[absolute] = (uint8_t)value;
                byte_present[absolute] = true;
                have_data = true;
            }
        }
    }

    fclose(fp);

    if (!have_data) {
        free(byte_data);
        free(byte_present);
        errno = EINVAL;
        return -1;
    }

    uint16_t *words = (uint16_t *)calloc(memory_words, sizeof(uint16_t));
    if (!words) {
        free(byte_data);
        free(byte_present);
        errno = ENOMEM;
        return -1;
    }

    size_t count = 0u;
    bool partial = false;
    for (size_t word = 0; word < memory_words; ++word) {
        size_t lo_index = word * 2u;
        size_t hi_index = lo_index + 1u;
        bool have_lo = (lo_index < memory_bytes) && byte_present[lo_index];
        bool have_hi = (hi_index < memory_bytes) && byte_present[hi_index];
        if (have_lo && have_hi) {
            uint16_t value = ((uint16_t)(byte_data[hi_index] & 0x0Fu) << 8) |
                             (uint16_t)byte_data[lo_index];
            words[count++] = value & 0x0FFFu;
        } else if (have_lo || have_hi) {
            partial = true;
        }
    }

    free(byte_data);
    free(byte_present);

    if (count == 0u) {
        free(words);
        errno = EINVAL;
        return -1;
    }

    if (count < memory_words) {
        uint16_t *resized = (uint16_t *)realloc(words, count * sizeof(uint16_t));
        if (resized) {
            words = resized;
        }
    }

    if (out_partial) {
        *out_partial = partial;
    }

    out_record->words = words;
    out_record->word_count = count;
    out_record->partial = partial;
    return 0;
}

static int load_record_from_file(struct magtape_unit *unit,
                                 const char *name,
                                 const char *full_path) {
    if (!unit || !name || !full_path) {
        return -1;
    }

    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }

    struct magtape_record record;
    memset(&record, 0, sizeof(record));

    bool partial = false;
    int rc = -1;
    if (ends_with_case_insensitive(name, ".tap")) {
        rc = read_tap_record(full_path, &record, &partial);
    } else if (ends_with_case_insensitive(name, ".srec")) {
        rc = load_srec_words(full_path, &record, &partial);
    } else {
        return 0;
    }

    if (rc != 0) {
        if (partial) {
            /* Treat truncated files as empty partial records instead of failing the unit. */
            record.words = NULL;
            record.word_count = 0u;
            record.partial = true;
        } else {
            return -1;
        }
    }

    record.name = duplicate_string(name);
    if (!record.name) {
        free_record(&record);
        return -1;
    }
    record.partial = partial;
    record.timestamp = st.st_mtime;

    if (append_record(unit, &record) != 0) {
        free_record(&record);
        return -1;
    }

    return 0;
}

static int reload_manifest(struct magtape_unit *unit) {
    if (!unit || !unit->path) {
        return -1;
    }

    DIR *dir = opendir(unit->path);
    if (!dir) {
        unit->error = true;
        clear_unit_records(unit);
        return -1;
    }

    clear_unit_records(unit);

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char *full_path = join_path(unit->path, entry->d_name);
        if (!full_path) {
            closedir(dir);
            clear_unit_records(unit);
            unit->error = true;
            return -1;
        }
        if (load_record_from_file(unit, entry->d_name, full_path) != 0) {
            free(full_path);
            closedir(dir);
            clear_unit_records(unit);
            unit->error = true;
            return -1;
        }
        free(full_path);
    }

    closedir(dir);

    if (unit->record_count > 1u) {
        qsort(unit->records, unit->record_count, sizeof(struct magtape_record), compare_records);
    }

    reset_unit_runtime(unit);
    if (unit->record_count > 0u) {
        struct magtape_record *record = &unit->records[0];
        unit->ready = record->word_count > 0u;
        unit->end_of_tape = false;
        unit->end_of_record = false;
        unit->error = false;
    } else {
        unit->ready = false;
        unit->end_of_tape = true;
        unit->end_of_record = false;
    }

    return 0;
}

static void close_write_stream(struct magtape_unit *unit, bool refresh_manifest) {
    if (!unit || !unit->write_stream) {
        return;
    }

    FILE *fp = unit->write_stream;
    unit->write_stream = NULL;

    if (fseek(fp, 0L, SEEK_END) == 0) {
        (void)write_word_le(fp, MAGTAPE_SENTINEL_WORD);
    }

    fflush(fp);

    if (fseek(fp, 0L, SEEK_SET) == 0) {
        uint16_t length = (uint16_t)(unit->write_words & 0x0FFFu);
        (void)write_word_le(fp, length);
    }

    fclose(fp);

    if (refresh_manifest) {
        (void)reload_manifest(unit);
    }

    free(unit->write_path);
    unit->write_path = NULL;
    unit->write_words = 0u;
}

static int ensure_write_stream(struct magtape_unit *unit) {
    if (!unit || !unit->path) {
        return -1;
    }
    if (unit->write_stream) {
        return 0;
    }

    DIR *dir = opendir(unit->path);
    if (!dir) {
        return -1;
    }

    unsigned next_index = 0u;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        unsigned parsed = 0u;
        if (parse_octal_record_index(entry->d_name, &parsed)) {
            if (parsed == UINT_MAX) {
                closedir(dir);
                errno = ERANGE;
                return -1;
            }
            if (parsed >= next_index) {
                next_index = parsed + 1u;
            }
        }
    }
    closedir(dir);

    FILE *fp = NULL;
    char *full_path = NULL;
    char candidate[32];
    unsigned candidate_index = next_index;
    unsigned attempts = 0u;
    while (attempts < 1024u) {
        if (format_record_filename(candidate_index, candidate, sizeof candidate) != 0) {
            free(full_path);
            return -1;
        }
        free(full_path);
        full_path = join_path(unit->path, candidate);
        if (!full_path) {
            return -1;
        }
        int fd = open(full_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd < 0) {
            if (errno == EEXIST) {
                if (candidate_index == UINT_MAX) {
                    free(full_path);
                    return -1;
                }
                candidate_index++;
                attempts++;
                continue;
            }
            free(full_path);
            return -1;
        }
        fp = fdopen(fd, "wb");
        if (!fp) {
            close(fd);
            remove(full_path);
            free(full_path);
            return -1;
        }
        break;
    }

    if (!fp) {
        free(full_path);
        return -1;
    }

    uint16_t placeholder = 0u;
    if (!write_word_le(fp, placeholder)) {
        fclose(fp);
        remove(full_path);
        free(full_path);
        return -1;
    }

    unit->write_stream = fp;
    unit->write_words = 0u;
    unit->write_path = full_path;
    return 0;
}

static struct magtape_unit *current_unit(struct pdp8_magtape_device *device) {
    return find_unit(device, device ? device->selected_unit : 0u);
}

static void select_unit(struct pdp8_magtape_device *device, unsigned unit_number) {
    if (!device) {
        return;
    }
    device->selected_unit = unit_number;
    struct magtape_unit *unit = find_unit(device, unit_number);
    if (!unit) {
        return;
    }
    if (unit->end_of_record && unit->current_record + 1u < unit->record_count) {
        unit->current_record++;
        unit->position = 0u;
        unit->end_of_record = false;
        unit->end_of_tape = false;
    }
    unit->error = false;
    if (unit->record_count > 0u) {
        struct magtape_record *record = &unit->records[unit->current_record];
        unit->ready = record->word_count > 0u;
    } else {
        unit->ready = false;
        unit->end_of_tape = true;
    }
}

static void perform_read(pdp8_t *cpu, struct magtape_unit *unit) {
    if (!cpu || !unit) {
        return;
    }
    if (unit->record_count == 0u) {
        unit->ready = false;
        unit->end_of_tape = true;
        unit->end_of_record = false;
        return;
    }
    if (unit->current_record >= unit->record_count) {
        unit->ready = false;
        unit->end_of_tape = true;
        return;
    }
    struct magtape_record *record = &unit->records[unit->current_record];
    if (unit->position >= record->word_count) {
        unit->ready = false;
        unit->end_of_record = true;
        if (unit->current_record + 1u >= unit->record_count) {
            unit->end_of_tape = true;
        }
        return;
    }
    uint16_t word = record->words[unit->position++];
    pdp8_api_set_ac(cpu, word & 0x0FFFu);
    unit->ready = unit->position < record->word_count;
    if (!unit->ready) {
        unit->end_of_record = true;
        if (unit->current_record + 1u >= unit->record_count) {
            unit->end_of_tape = true;
        }
    }
}

static void perform_write(pdp8_t *cpu, struct magtape_unit *unit) {
    if (!cpu || !unit) {
        return;
    }
    if (unit->write_protected) {
        unit->error = true;
        return;
    }
    if (ensure_write_stream(unit) != 0) {
        unit->error = true;
        return;
    }
    uint16_t word = pdp8_api_get_ac(cpu) & 0x0FFFu;
    if (!write_word_le(unit->write_stream, word)) {
        unit->error = true;
        return;
    }
    unit->write_words++;
    unit->end_of_record = false;
    unit->end_of_tape = false;
}

static void perform_rewind(struct magtape_unit *unit) {
    if (!unit) {
        return;
    }
    close_write_stream(unit, true);
    reload_manifest(unit);
}

static void perform_sense(pdp8_t *cpu, struct magtape_unit *unit, unsigned unit_number) {
    if (!cpu || !unit) {
        return;
    }
    uint16_t status = 0u;
    if (unit->ready) {
        status |= MAGTAPE_STATUS_READY;
    }
    if (unit->error) {
        status |= MAGTAPE_STATUS_ERROR;
    }
    if (unit->end_of_record) {
        status |= MAGTAPE_STATUS_EOR;
    }
    if (unit->end_of_tape) {
        status |= MAGTAPE_STATUS_EOT;
    }
    if (unit->write_protected) {
        status |= MAGTAPE_STATUS_WRITE_PROTECT;
    }
    status |= (uint16_t)((unit_number & 0x7u) << 7);
    pdp8_api_set_ac(cpu, status & 0x0FFFu);
}

static void magtape_device_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    struct pdp8_magtape_device *device = (struct pdp8_magtape_device *)context;
    if (!device) {
        return;
    }

    uint16_t microcode = instruction & 0x3Fu;

    if (microcode & PDP8_MAGTAPE_BIT_GO) {
        unsigned unit_request = pdp8_api_get_ac(cpu) & 0x7u;
        struct magtape_unit *unit = find_unit(device, unit_request);
        if (!unit || !unit->configured) {
            struct magtape_unit *current = current_unit(device);
            if (current) {
                current->error = true;
            }
        } else {
            select_unit(device, unit_request);
        }
    }

    struct magtape_unit *unit = current_unit(device);
    if (!unit) {
        return;
    }

    if (microcode & PDP8_MAGTAPE_BIT_SKIP) {
        if (unit->ready) {
            pdp8_api_request_skip(cpu);
        }
    }
    if (microcode & PDP8_MAGTAPE_BIT_REWIND) {
        perform_rewind(unit);
    }
    if (microcode & PDP8_MAGTAPE_BIT_READ) {
        perform_read(cpu, unit);
    }
    if (microcode & PDP8_MAGTAPE_BIT_WRITE) {
        perform_write(cpu, unit);
    }
    if (microcode & PDP8_MAGTAPE_BIT_SENSE) {
        perform_sense(cpu, unit, unit->unit_number);
    }
}

pdp8_magtape_device_t *pdp8_magtape_device_create(void) {
    struct pdp8_magtape_device *device =
        (struct pdp8_magtape_device *)calloc(1u, sizeof(struct pdp8_magtape_device));
    if (!device) {
        return NULL;
    }
    device->selected_unit = 0u;
    return device;
}

void pdp8_magtape_device_destroy(pdp8_magtape_device_t *device) {
    if (!device) {
        return;
    }
    for (size_t i = 0; i < device->unit_count; ++i) {
        struct magtape_unit *unit = &device->units[i];
        close_write_stream(unit, false);
        clear_unit_records(unit);
        free(unit->path);
        unit->path = NULL;
    }
    free(device->units);
    free(device);
}

int pdp8_magtape_device_attach(pdp8_t *cpu, pdp8_magtape_device_t *device) {
    if (!cpu || !device) {
        return -1;
    }
    static const uint8_t device_codes[] = {070u, 071u, 072u, 073u, 074u, 075u, 076u, 077u};
    for (size_t i = 0; i < sizeof(device_codes) / sizeof(device_codes[0]); ++i) {
        if (pdp8_api_register_iot(cpu, device_codes[i], magtape_device_iot, device) != 0) {
            return -1;
        }
    }
    return 0;
}

int pdp8_magtape_device_configure_unit(pdp8_magtape_device_t *device,
                                       const struct pdp8_magtape_unit_params *params) {
    if (!device || !params) {
        return -1;
    }
    struct magtape_unit *unit = ensure_unit(device, params->unit_number);
    if (!unit) {
        return -1;
    }
    if (params->path) {
        char *copy = duplicate_string(params->path);
        if (!copy) {
            return -1;
        }
        free(unit->path);
        unit->path = copy;
    }
    unit->unit_number = params->unit_number;
    unit->write_protected = params->write_protected;
    unit->configured = true;
    if (unit->path) {
        if (reload_manifest(unit) != 0) {
            return -1;
        }
    }
    if (device->unit_count == 1u) {
        device->selected_unit = unit->unit_number;
    }
    return 0;
}

int pdp8_magtape_device_rewind(pdp8_magtape_device_t *device, unsigned unit_number) {
    struct magtape_unit *unit = find_unit(device, unit_number);
    if (!unit || !unit->configured) {
        return -1;
    }
    perform_rewind(unit);
    return 0;
}

int pdp8_magtape_device_next_record(pdp8_magtape_device_t *device, unsigned unit_number) {
    struct magtape_unit *unit = find_unit(device, unit_number);
    if (!unit || !unit->configured) {
        return -1;
    }

    if (unit->current_record + 1u >= unit->record_count) {
        unit->end_of_tape = true;
        return -1; /* End of tape */
    }

    unit->current_record++;
    unit->position = 0u;
    unit->end_of_record = false;
    unit->end_of_tape = (unit->current_record + 1u >= unit->record_count);

    if (unit->record_count > 0u) {
        struct magtape_record *record = &unit->records[unit->current_record];
        unit->ready = record->word_count > 0u;
    } else {
        unit->ready = false;
    }

    return 0;
}

int pdp8_magtape_device_force_new_record(pdp8_magtape_device_t *device, unsigned unit_number) {
    struct magtape_unit *unit = find_unit(device, unit_number);
    if (!unit || !unit->configured) {
        return -1;
    }
    close_write_stream(unit, true);
    return 0;
}

int pdp8_magtape_device_get_status(const pdp8_magtape_device_t *device,
                                   unsigned unit_number,
                                   struct pdp8_magtape_unit_status *out_status) {
    if (!device || !out_status) {
        return -1;
    }
    const struct magtape_unit *unit = find_unit((struct pdp8_magtape_device *)device, unit_number);
    memset(out_status, 0, sizeof(*out_status));
    if (!unit || !unit->configured) {
        out_status->configured = false;
        out_status->unit_number = unit_number;
        return 0;
    }
    out_status->configured = true;
    out_status->unit_number = unit->unit_number;
    out_status->path = unit->path;
    out_status->record_count = unit->record_count;
    out_status->record_index = unit->current_record;
    out_status->word_position = unit->position;
    out_status->ready = unit->ready;
    out_status->write_protected = unit->write_protected;
    out_status->end_of_record = unit->end_of_record;
    out_status->end_of_tape = unit->end_of_tape;
    out_status->error = unit->error;
    if (unit->record_count > 0u && unit->current_record < unit->record_count) {
        const struct magtape_record *record = &unit->records[unit->current_record];
        out_status->current_record = record->name;
        out_status->word_count = record->word_count;
        out_status->partial_record = record->partial;
    }
    return 0;
}
