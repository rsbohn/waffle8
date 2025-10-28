#define _POSIX_C_SOURCE 200809L

#include "monitor_platform.h"

#include "emulator/kl8e_console.h"
#include "emulator/line_printer.h"
#include "emulator/pdp8_board.h"
#include "monitor_config.h"

#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static bool s_config_loaded = false;
static FILE *s_console_input = NULL;
static FILE *s_console_output = NULL;
static FILE *s_printer_output = NULL;
static bool s_close_console_input = false;
static bool s_close_console_output = false;
static bool s_close_printer_output = false;

static bool parse_boolean(const char *text, bool *out_value) {
    if (!text || !out_value) {
        return false;
    }
    if (strcasecmp(text, "true") == 0 || strcasecmp(text, "yes") == 0 || strcmp(text, "1") == 0) {
        *out_value = true;
        return true;
    }
    if (strcasecmp(text, "false") == 0 || strcasecmp(text, "no") == 0 || strcmp(text, "0") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static struct monitor_magtape_unit_config *
find_or_create_magtape_slot(struct monitor_config *config, int unit_number) {
    if (!config) {
        return NULL;
    }
    for (size_t i = 0; i < config->magtape_unit_count; ++i) {
        if (config->magtape_units[i].unit_number == unit_number) {
            return &config->magtape_units[i];
        }
    }
    if (config->magtape_unit_count >= MONITOR_MAX_MAGTAPE_UNITS) {
        return NULL;
    }
    size_t index = config->magtape_unit_count++;
    struct monitor_magtape_unit_config *slot = &config->magtape_units[index];
    memset(slot, 0, sizeof(*slot));
    slot->unit_number = unit_number;
    return slot;
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

static int monitor_config_load_file(const char *path, struct monitor_config *config) {
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
    struct monitor_magtape_unit_config *current_magtape = NULL;
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
                current_magtape = NULL;
                continue;
            }
            if (strncmp(current_device, "magtape", 7) == 0) {
                const char *unit_str = current_device + 7;
                char *endptr = NULL;
                long parsed_unit = strtol(unit_str, &endptr, 10);
                if (endptr && *endptr == '\0' && parsed_unit >= 0 && parsed_unit <= INT_MAX) {
                    current_magtape = find_or_create_magtape_slot(config, (int)parsed_unit);
                    if (current_magtape) {
                        current_magtape->unit_number = (int)parsed_unit;
                    }
                } else {
                    current_magtape = NULL;
                }
            } else {
                current_magtape = NULL;
            }
            continue;
        }

        if (strcmp(trimmed, "}") == 0) {
            current_device[0] = '\0';
            current_magtape = NULL;
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
        } else if (strncmp(current_device, "magtape", 7) == 0) {
            struct monitor_magtape_unit_config *slot = current_magtape;
            if (!slot) {
                continue;
            }
            slot->present = true;
            if (strcmp(key, "unit") == 0) {
                long parsed = strtol(value, NULL, 10);
                if (parsed >= 0 && parsed <= INT_MAX) {
                    slot->unit_number = (int)parsed;
                }
            } else if (strcmp(key, "path") == 0) {
                if (monitor_config_set_string(&slot->path, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "write_protected") == 0) {
                bool flag = false;
                if (parse_boolean(value, &flag)) {
                    slot->write_protected = flag;
                }
            }
        }
        else if (strcmp(current_device, "watchdog") == 0) {
            config->watchdog_present = true;
            if (strcmp(key, "iot") == 0) {
                if (monitor_config_set_string(&config->watchdog_iot, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "enabled") == 0) {
                bool flag = false;
                if (parse_boolean(value, &flag)) {
                    config->watchdog_enabled = flag;
                }
            } else if (strcmp(key, "mode") == 0) {
                if (monitor_config_set_string(&config->watchdog_mode, value) != 0) {
                    fclose(fp);
                    return -1;
                }
            } else if (strcmp(key, "periodic") == 0) {
                bool flag = false;
                if (parse_boolean(value, &flag)) {
                    config->watchdog_periodic = flag;
                }
            } else if (strcmp(key, "default_count") == 0) {
                long parsed = strtol(value, NULL, 10);
                if (parsed >= 0 && parsed <= INT32_MAX) {
                    config->watchdog_default_count = (int)parsed;
                }
            } else if (strcmp(key, "pause_on_halt") == 0) {
                bool flag = false;
                if (parse_boolean(value, &flag)) {
                    config->watchdog_pause_on_halt = flag;
                }
            }
        }
    }

    fclose(fp);
    current_device[0] = '\0';
    return 0;
}

static void close_stream(FILE **stream, bool should_close) {
    if (!stream || !*stream) {
        return;
    }
    if (should_close) {
        fclose(*stream);
    }
    *stream = NULL;
}

int monitor_platform_init(struct monitor_config *config,
                          const pdp8_board_spec **out_board,
                          bool *config_loaded,
                          int *config_result) {
    if (!config || !out_board) {
        return -1;
    }

    int load_result = monitor_config_load_file("pdp8.config", config);
    int load_errno = errno;
    if (load_result == 0) {
        s_config_loaded = true;
    } else {
        s_config_loaded = false;
        if (load_result == 1) {
            monitor_config_init(config);
        }
    }

    if (config_loaded) {
        *config_loaded = s_config_loaded;
    }
    if (config_result) {
        *config_result = (load_result < 0) ? -load_errno : load_result;
    }

    const pdp8_board_spec *board = pdp8_board_host_simulator();
    if (!board) {
        return -1;
    }
    *out_board = board;

    s_console_input = stdin;
    s_close_console_input = false;
    s_console_output = stdout;
    s_close_console_output = false;
    s_printer_output = stdout;
    s_close_printer_output = false;

    if (s_config_loaded && config->kl8e_present) {
        if (config->kl8e_keyboard_input && strcmp(config->kl8e_keyboard_input, "stdin") != 0) {
            FILE *custom = fopen(config->kl8e_keyboard_input, "r");
            if (custom) {
                s_console_input = custom;
                s_close_console_input = true;
            } else {
                fprintf(stderr,
                        "Warning: unable to open KL8E keyboard input '%s': %s. Falling back to stdin.\n",
                        config->kl8e_keyboard_input, strerror(errno));
            }
        }

        if (config->kl8e_teleprinter_output &&
            strcmp(config->kl8e_teleprinter_output, "stdout") != 0) {
            if (strcmp(config->kl8e_teleprinter_output, "stderr") == 0) {
                s_console_output = stderr;
                s_close_console_output = false;
            } else {
                FILE *custom = fopen(config->kl8e_teleprinter_output, "a");
                if (custom) {
                    s_console_output = custom;
                    s_close_console_output = true;
                } else {
                    fprintf(stderr,
                            "Warning: unable to open KL8E teleprinter output '%s': %s. Falling back to stdout.\n",
                            config->kl8e_teleprinter_output, strerror(errno));
                }
            }
        }
    }

    if (s_config_loaded && config->line_printer_present) {
        if (config->line_printer_output && strcmp(config->line_printer_output, "stdout") != 0) {
            if (strcmp(config->line_printer_output, "stderr") == 0) {
                s_printer_output = stderr;
                s_close_printer_output = false;
            } else {
                FILE *custom = fopen(config->line_printer_output, "a");
                if (custom) {
                    s_printer_output = custom;
                    s_close_printer_output = true;
                } else {
                    fprintf(stderr,
                            "Warning: unable to open line printer output '%s': %s. Falling back to stdout.\n",
                            config->line_printer_output, strerror(errno));
                }
            }
        }
    }

    return 0;
}

void monitor_platform_shutdown(void) {
    close_stream(&s_console_input, s_close_console_input);
    close_stream(&s_console_output, s_close_console_output);
    close_stream(&s_printer_output, s_close_printer_output);
    s_close_console_input = false;
    s_close_console_output = false;
    s_close_printer_output = false;
    s_config_loaded = false;
}

pdp8_kl8e_console_t *monitor_platform_create_console(void) {
    FILE *input = s_console_input ? s_console_input : stdin;
    FILE *output = s_console_output ? s_console_output : stdout;
    return pdp8_kl8e_console_create(input, output);
}

pdp8_line_printer_t *monitor_platform_create_printer(void) {
    FILE *output = s_printer_output ? s_printer_output : stdout;
    return pdp8_line_printer_create(output);
}

void monitor_platform_console_putc(uint8_t ch) {
    FILE *output = s_console_output ? s_console_output : stdout;
    fputc((int)ch, output);
}

void monitor_platform_console_flush(void) {
    FILE *output = s_console_output ? s_console_output : stdout;
    fflush(output);
}

void monitor_platform_printer_putc(uint8_t ch) {
    FILE *output = s_printer_output ? s_printer_output : stdout;
    fputc((int)ch, output);
}

void monitor_platform_printer_flush(void) {
    FILE *output = s_printer_output ? s_printer_output : stdout;
    fflush(output);
}

bool monitor_platform_poll_keyboard(uint8_t *out_ch) {
    FILE *input = s_console_input ? s_console_input : stdin;
    int fd = fileno(input);
    if (fd < 0) {
        return false;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval timeout = {0, 0};
    int ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready <= 0 || !FD_ISSET(fd, &read_fds)) {
        return false;
    }

    errno = 0;
    int ch = fgetc(input);
    if (ch == EOF) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            clearerr(input);
        } else if (feof(input)) {
            clearerr(input);
        }
        return false;
    }

    if (ch == '\n') {
        ch = '\r';
    }

    if (out_ch) {
        *out_ch = (uint8_t)(ch & 0x7F);
    }
    return true;
}

void monitor_platform_idle(void) {
    struct timespec req = {0, 1000000};
    nanosleep(&req, NULL);
}

bool monitor_platform_readline(char *buffer, size_t buffer_length) {
    if (!buffer || buffer_length == 0) {
        return false;
    }

    FILE *input = s_console_input ? s_console_input : stdin;
    if (!input) {
        return false;
    }

    if (!fgets(buffer, (int)buffer_length, input)) {
        return false;
    }

    return true;
}

uint64_t monitor_platform_time_us(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000ull + (uint64_t)now.tv_nsec / 1000ull;
}

void monitor_platform_sleep_until(uint64_t target_time_us) {
    uint64_t now = monitor_platform_time_us();
    if (target_time_us <= now) {
        return;
    }
    uint64_t delta = target_time_us - now;
    struct timespec req;
    req.tv_sec = (time_t)(delta / 1000000ull);
    req.tv_nsec = (long)((delta % 1000000ull) * 1000ull);
    nanosleep(&req, NULL);
}
