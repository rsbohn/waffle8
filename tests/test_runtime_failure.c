#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>

#include "../src/pdp8v_runtime.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/monitor_platform.h"

#if !defined(PDP8V_ENABLE_TEST_HOOKS)
#error "test_runtime_failure requires PDP8V_ENABLE_TEST_HOOKS"
#endif

static int fail_count = 0;

static void assert_true(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "Assertion failed: %s\n", message);
        fail_count++;
    }
}

static void validate_snapshot(const struct pdp8v_test_snapshot *snapshot,
                              bool cpu,
                              bool console,
                              bool printer,
                              bool global_console) {
    assert_true(snapshot->cpu_present == cpu, "CPU presence mismatch");
    assert_true(snapshot->console_present == console, "Console presence mismatch");
    assert_true(snapshot->printer_present == printer, "Printer presence mismatch");
    assert_true(snapshot->global_console_present == global_console, "Global console state mismatch");
}

/* Simple stubs that match the monitor_platform interface for this test */
pdp8_kl8e_console_t *monitor_platform_create_console(void) {
    return pdp8_kl8e_console_create(NULL, NULL);
}

pdp8_line_printer_t *monitor_platform_create_printer(void) {
    return pdp8_line_printer_create(NULL);
}

int monitor_platform_init(struct monitor_config *config,
                          const pdp8_board_spec **out_board,
                          bool *config_loaded,
                          int *config_result) {
    (void)config;
    (void)out_board;
    (void)config_loaded;
    (void)config_result;
    return 0;
}

void monitor_platform_shutdown(void) {
}

void monitor_platform_console_putc(uint8_t ch) {
    (void)ch;
}

void monitor_platform_console_flush(void) {
}

void monitor_platform_printer_putc(uint8_t ch) {
    (void)ch;
}

void monitor_platform_printer_flush(void) {
}

bool monitor_platform_poll_keyboard(uint8_t *out_ch) {
    (void)out_ch;
    return false;
}

void monitor_platform_idle(void) {
}

bool monitor_platform_readline(char *buffer, size_t buffer_length) {
    (void)buffer;
    (void)buffer_length;
    return false;
}

uint64_t monitor_platform_time_us(void) {
    return 0u;
}

void monitor_platform_sleep_until(uint64_t target_time_us) {
    (void)target_time_us;
}

void monitor_platform_enqueue_key(uint8_t ch);

int main(void) {
    const pdp8_board_spec *board = pdp8_board_host_simulator();
    struct pdp8v_test_snapshot snapshot;

    pdp8v_test_reset_injections();

    bool success = pdp8v_test_attempt_runtime_create(board, &snapshot);
    assert_true(success, "Runtime create should succeed without injections");
    if (success) {
        validate_snapshot(&snapshot, true, true, true, true);
    }

    struct pdp8v_test_injections injections = {0};
    injections.fail_console_attach = true;
    pdp8v_test_set_injections(&injections);

    success = pdp8v_test_attempt_runtime_create(board, &snapshot);
    assert_true(!success, "Runtime create should fail when console attach fails");
    if (!success) {
        validate_snapshot(&snapshot, false, false, false, false);
    }

    pdp8v_test_reset_injections();
    injections.fail_printer_factory = true;
    pdp8v_test_set_injections(&injections);

    success = pdp8v_test_attempt_runtime_create(board, &snapshot);
    assert_true(!success, "Runtime create should fail when printer factory fails");
    if (!success) {
        validate_snapshot(&snapshot, false, false, false, false);
    }

    if (fail_count == 0) {
        printf("runtime failure tests OK\n");
    }
    return fail_count == 0 ? 0 : 1;
}
