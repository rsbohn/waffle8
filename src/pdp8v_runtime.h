#ifndef PDP8V_RUNTIME_H
#define PDP8V_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emulator/pdp8.h"
#include "emulator/pdp8_board.h"
#include "emulator/kl8e_console.h"
#include "emulator/line_printer.h"
#include "emulator/paper_tape_device.h"
#include "emulator/magtape_device.h"
#include "emulator/watchdog.h"
#include "monitor_config.h"

struct pdp8v_runtime {
    pdp8_t *cpu;
    pdp8_kl8e_console_t *console;
    pdp8_line_printer_t *printer;
    pdp8_paper_tape_device_t *paper_tape;
    pdp8_magtape_device_t *magtape;
    pdp8_watchdog_t *watchdog;
    struct monitor_config config;
    size_t memory_words;
    const char *program_name;
};

void pdp8v_runtime_init(struct pdp8v_runtime *runtime);
bool pdp8v_runtime_create(struct pdp8v_runtime *runtime, const pdp8_board_spec *board);
void pdp8v_runtime_teardown(struct pdp8v_runtime *runtime);

pdp8_kl8e_console_t *pdp8v_get_console(void);

#if defined(PDP8V_ENABLE_TEST_HOOKS)
struct pdp8v_test_injections {
    bool fail_console_factory;
    bool fail_console_attach;
    bool fail_printer_factory;
    bool fail_printer_attach;
};

struct pdp8v_test_snapshot {
    bool cpu_present;
    bool console_present;
    bool printer_present;
    bool watchdog_present;
    bool global_console_present;
};

void pdp8v_test_set_injections(const struct pdp8v_test_injections *injections);
void pdp8v_test_reset_injections(void);
bool pdp8v_test_attempt_runtime_create(const pdp8_board_spec *board,
                                       struct pdp8v_test_snapshot *snapshot);
#endif /* PDP8V_ENABLE_TEST_HOOKS */

#endif /* PDP8V_RUNTIME_H */
