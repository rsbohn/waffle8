#define _POSIX_C_SOURCE 200809L

#include "pdp8v_runtime.h"

#include <string.h>

#include "monitor_platform.h"

static pdp8_kl8e_console_t *g_console = NULL;

pdp8_kl8e_console_t *pdp8v_get_console(void) {
    return g_console;
}

static void pdp8v_set_console(pdp8_kl8e_console_t *console) {
    g_console = console;
}

#if defined(PDP8V_ENABLE_TEST_HOOKS)
static struct pdp8v_test_injections g_test_injections;

void pdp8v_test_reset_injections(void) {
    memset(&g_test_injections, 0, sizeof(g_test_injections));
}

void pdp8v_test_set_injections(const struct pdp8v_test_injections *injections) {
    if (injections) {
        g_test_injections = *injections;
    } else {
        memset(&g_test_injections, 0, sizeof(g_test_injections));
    }
}

static bool pdp8v_should_fail_console_factory(void) {
    return g_test_injections.fail_console_factory;
}

static bool pdp8v_should_fail_printer_factory(void) {
    return g_test_injections.fail_printer_factory;
}

static bool pdp8v_should_fail_console_attach(void) {
    return g_test_injections.fail_console_attach;
}

static bool pdp8v_should_fail_printer_attach(void) {
    return g_test_injections.fail_printer_attach;
}
#endif

void pdp8v_runtime_init(struct pdp8v_runtime *runtime) {
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    monitor_config_init(&runtime->config);
}

bool pdp8v_runtime_create(struct pdp8v_runtime *runtime, const pdp8_board_spec *board) {
    if (!runtime || !board) {
        return false;
    }

    runtime->memory_words = board->memory_words ? board->memory_words : 4096u;

    runtime->cpu = pdp8_api_create_for_board(board);
    if (!runtime->cpu) {
        return false;
    }

#if defined(PDP8V_ENABLE_TEST_HOOKS)
    if (pdp8v_should_fail_console_factory()) {
        runtime->console = NULL;
    } else
#endif
    {
        runtime->console = monitor_platform_create_console();
    }
    if (!runtime->console) {
        goto fail;
    }
    (void)pdp8_kl8e_console_set_output_stream(runtime->console, NULL);

    int console_attach = 0;
#if defined(PDP8V_ENABLE_TEST_HOOKS)
    if (pdp8v_should_fail_console_attach()) {
        console_attach = -1;
    } else
#endif
    {
        console_attach = pdp8_kl8e_console_attach(runtime->cpu, runtime->console);
    }
    if (console_attach != 0) {
        goto fail;
    }

    pdp8v_set_console(runtime->console);

#if defined(PDP8V_ENABLE_TEST_HOOKS)
    if (pdp8v_should_fail_printer_factory()) {
        runtime->printer = NULL;
    } else
#endif
    {
        runtime->printer = monitor_platform_create_printer();
    }
    if (!runtime->printer) {
        goto fail;
    }
    (void)pdp8_line_printer_set_stream(runtime->printer, NULL);

    int printer_attach = 0;
#if defined(PDP8V_ENABLE_TEST_HOOKS)
    if (pdp8v_should_fail_printer_attach()) {
        printer_attach = -1;
    } else
#endif
    {
        printer_attach = pdp8_line_printer_attach(runtime->cpu, runtime->printer);
    }
    if (printer_attach != 0) {
        goto fail;
    }

    if (runtime->config.watchdog_present) {
        runtime->watchdog = pdp8_watchdog_create();
        if (runtime->watchdog) {
            if (pdp8_watchdog_attach(runtime->cpu, runtime->watchdog) != 0) {
                pdp8_watchdog_destroy(runtime->watchdog);
                runtime->watchdog = NULL;
            }
        }
    }

    return true;

fail:
    pdp8v_runtime_teardown(runtime);
    return false;
}

void pdp8v_runtime_teardown(struct pdp8v_runtime *runtime) {
    if (!runtime) {
        return;
    }

    if (runtime->watchdog) {
        pdp8_watchdog_destroy(runtime->watchdog);
        runtime->watchdog = NULL;
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
        pdp8_kl8e_console_destroy(runtime->console);
        runtime->console = NULL;
    }

    if (runtime->cpu) {
        pdp8_api_destroy(runtime->cpu);
        runtime->cpu = NULL;
    }

    pdp8v_set_console(NULL);
}

#if defined(PDP8V_ENABLE_TEST_HOOKS)
bool pdp8v_test_attempt_runtime_create(const pdp8_board_spec *board,
                                       struct pdp8v_test_snapshot *snapshot) {
    struct pdp8v_runtime runtime;
    pdp8v_runtime_init(&runtime);

    bool success = pdp8v_runtime_create(&runtime, board);

    if (snapshot) {
        snapshot->cpu_present = runtime.cpu != NULL;
        snapshot->console_present = runtime.console != NULL;
        snapshot->printer_present = runtime.printer != NULL;
        snapshot->watchdog_present = runtime.watchdog != NULL;
        snapshot->global_console_present = pdp8v_get_console() != NULL;
    }

    if (success) {
        pdp8v_runtime_teardown(&runtime);
    }

    return success;
}
#endif
