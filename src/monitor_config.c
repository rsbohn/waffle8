#include "monitor_config.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void monitor_config_init(struct monitor_config *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->line_printer_column_limit = 132;
}

void monitor_config_clear(struct monitor_config *config) {
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

int monitor_config_set_string(char **slot, const char *value) {
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
