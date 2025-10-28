/* TODO: add configuration parsing/unit tests for monitor config
 *
 * This file is a placeholder to remind us to add tests that verify
 * parsing of pdp8.config entries (including the watchdog stanza) and
 * that monitor_platform_init correctly interprets values.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>

#include "../src/monitor_config.h"
#include "../src/monitor_platform.h"

int main(void) {
    struct monitor_config config;
    const pdp8_board_spec *board = NULL;
    bool config_loaded = false;
    int config_result = 0;

    /* Call platform init which will load pdp8.config from the repo root */
    int rc = monitor_platform_init(&config, &board, &config_loaded, &config_result);
    if (rc != 0) {
        fprintf(stderr, "monitor_platform_init failed: %d\n", rc);
        return 2;
    }

    if (!config_loaded) {
        fprintf(stderr, "Expected config to be loaded from pdp8.config\n");
        monitor_platform_shutdown();
        return 3;
    }

    /* Verify watchdog stanza parsed */
    if (!config.watchdog_present) {
        fprintf(stderr, "watchdog_present not set\n");
        monitor_platform_shutdown();
        return 4;
    }

    if (!config.watchdog_enabled) {
        fprintf(stderr, "watchdog_enabled not set\n");
        monitor_platform_shutdown();
        return 5;
    }

    if (!config.watchdog_mode || strcasecmp(config.watchdog_mode, "halt") != 0) {
        fprintf(stderr, "watchdog_mode unexpected: %s\n", config.watchdog_mode ? config.watchdog_mode : "(null)");
        monitor_platform_shutdown();
        return 6;
    }

    if (config.watchdog_periodic) {
        fprintf(stderr, "watchdog_periodic unexpected true\n");
        monitor_platform_shutdown();
        return 7;
    }

    if (config.watchdog_default_count != 5) {
        fprintf(stderr, "watchdog_default_count unexpected: %d\n", config.watchdog_default_count);
        monitor_platform_shutdown();
        return 8;
    }

    if (!config.watchdog_pause_on_halt) {
        fprintf(stderr, "watchdog_pause_on_halt unexpected false\n");
        monitor_platform_shutdown();
        return 9;
    }

    printf("config test OK\n");
    monitor_platform_shutdown();
    /* clear config memory */
    monitor_config_clear(&config);
    /* Now run invalid-config scenarios in a temporary directory */
    char template[] = "/tmp/pdp8cfgXXXXXX";
    char *tmpdir = mkdtemp(template);
    if (!tmpdir) {
        fprintf(stderr, "failed to create temp dir\n");
        return 10;
    }
    char oldcwd[4096];
    if (!getcwd(oldcwd, sizeof oldcwd)) {
        fprintf(stderr, "getcwd failed\n");
        return 11;
    }
    if (chdir(tmpdir) != 0) {
        fprintf(stderr, "chdir to tmp failed\n");
        return 12;
    }

    /* Case 1: missing watchdog stanza */
    FILE *fp = fopen("pdp8.config", "w");
    if (!fp) {
        fprintf(stderr, "failed to create temp pdp8.config\n");
        return 13;
    }
    fputs("device kl8e_console {\n  keyboard_iot = 603x\n}\n", fp);
    fclose(fp);

    struct monitor_config cfg2;
    const pdp8_board_spec *b2 = NULL;
    bool loaded2 = false;
    int res2 = 0;
    if (monitor_platform_init(&cfg2, &b2, &loaded2, &res2) != 0) {
        fprintf(stderr, "monitor_platform_init failed for missing-stanza case\n");
        return 14;
    }
    if (cfg2.watchdog_present) {
        fprintf(stderr, "Expected watchdog_present == false for missing stanza\n");
        monitor_platform_shutdown();
        monitor_config_clear(&cfg2);
        return 15;
    }
    monitor_platform_shutdown();
    monitor_config_clear(&cfg2);

    /* Case 2: out-of-bounds / invalid values */
    fp = fopen("pdp8.config", "w");
    if (!fp) {
        fprintf(stderr, "failed to create temp pdp8.config (case2)\n");
        return 16;
    }
    fputs("device watchdog {\n  enabled = maybe\n  default_count = 99999999999\n  mode = crazy\n  periodic = yes\n  pause_on_halt = nope\n}\n", fp);
    fclose(fp);

    struct monitor_config cfg3;
    const pdp8_board_spec *b3 = NULL;
    bool loaded3 = false;
    int res3 = 0;
    if (monitor_platform_init(&cfg3, &b3, &loaded3, &res3) != 0) {
        fprintf(stderr, "monitor_platform_init failed for invalid-values case\n");
        return 17;
    }
    /* enabled was invalid -> should remain false */
    if (cfg3.watchdog_enabled) {
        fprintf(stderr, "watchdog_enabled should be false for invalid boolean\n");
        monitor_platform_shutdown();
        monitor_config_clear(&cfg3);
        return 18;
    }
    /* default_count out of range -> not set (defaults to 0) */
    if (cfg3.watchdog_default_count != 0) {
        fprintf(stderr, "watchdog_default_count unexpected: %d\n", cfg3.watchdog_default_count);
        monitor_platform_shutdown();
        monitor_config_clear(&cfg3);
        return 19;
    }
    /* mode is stored as string; ensure it contains 'crazy' */
    if (!cfg3.watchdog_mode || strcmp(cfg3.watchdog_mode, "crazy") != 0) {
        fprintf(stderr, "watchdog_mode not recorded as 'crazy'\n");
        monitor_platform_shutdown();
        monitor_config_clear(&cfg3);
        return 20;
    }
    /* periodic parsed 'yes' -> true */
    if (!cfg3.watchdog_periodic) {
        fprintf(stderr, "watchdog_periodic should be true for 'yes'\n");
        monitor_platform_shutdown();
        monitor_config_clear(&cfg3);
        return 21;
    }
    /* pause_on_halt 'nope' invalid -> remains false */
    if (cfg3.watchdog_pause_on_halt) {
        fprintf(stderr, "watchdog_pause_on_halt should be false for invalid value\n");
        monitor_platform_shutdown();
        monitor_config_clear(&cfg3);
        return 22;
    }

    monitor_platform_shutdown();
    monitor_config_clear(&cfg3);

    /* cleanup and return to original cwd */
    chdir(oldcwd);
    /* remove temp files */
    remove("/tmp/pdp8cfgXXXXXX/pdp8.config");
    rmdir(tmpdir);

    printf("invalid config tests OK\n");
    return 0;
}

