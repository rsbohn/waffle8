#ifndef MONITOR_CONFIG_H
#define MONITOR_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define MONITOR_MAX_MAGTAPE_UNITS 8u

struct monitor_magtape_unit_config {
    bool present;
    int unit_number;
    char *path;
    bool write_protected;
};

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

    size_t magtape_unit_count;
    struct monitor_magtape_unit_config magtape_units[MONITOR_MAX_MAGTAPE_UNITS];
};

void monitor_config_init(struct monitor_config *config);

void monitor_config_clear(struct monitor_config *config);

int monitor_config_set_string(char **slot, const char *value);

#endif /* MONITOR_CONFIG_H */
