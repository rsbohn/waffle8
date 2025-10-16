#ifndef MONITOR_PLATFORM_H
#define MONITOR_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emulator/kl8e_console.h"
#include "emulator/line_printer.h"
#include "emulator/pdp8_board.h"

struct monitor_config;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform glue for the PDP-8 monitor.
 *
 * The monitor core (tools/monitor.c) talks exclusively to this interface so
 * that a single code base can target the historical POSIX host as well as the
 * upcoming Adafruit Fruit Jam build.  Each platform provides an implementation
 * that adapts local I/O, configuration storage, and timing primitives to the
 * monitor's expectations.  The monitor core provides the
 * monitor_platform_enqueue_key() entry point so USB/keyboard backends can feed
 * characters into the KL8E queue without reaching into emulator internals.
 */

/**
 * Initialise platform services and return the board wiring the monitor should
 * expose to the emulator core.
 *
 * Implementations may populate @config with platform-specific defaults (for
 * example, KL8E device configuration on POSIX or baked-in Fruit Jam resources).
 * The returned pointer must remain valid for the lifetime of the monitor run.
 *
 * Returns 0 on success or a negative error code.
 */
int monitor_platform_init(struct monitor_config *config,
                          const pdp8_board_spec **out_board,
                          bool *config_loaded,
                          int *config_result);

/**
 * Create and return the KL8E console wiring for this platform.
 */
pdp8_kl8e_console_t *monitor_platform_create_console(void);

/**
 * Create and return the line printer peripheral for this platform.
 */
pdp8_line_printer_t *monitor_platform_create_printer(void);

/**
 * Tear down any resources allocated by monitor_platform_init().
 */
void monitor_platform_shutdown(void);

/**
 * Emit a character to the active console/terminal.  Implementations may buffer
 * output internally and release it on monitor_platform_console_flush().
 */
void monitor_platform_console_putc(uint8_t ch);

/**
 * Flush any buffered console output to the user-visible surface.
 */
void monitor_platform_console_flush(void);

/**
 * Emit a character destined for the simulated line printer.  Platforms that do
 * not support a discrete printer may route this to the console.
 */
void monitor_platform_printer_putc(uint8_t ch);

/**
 * Flush pending printer output.
 */
void monitor_platform_printer_flush(void);

/**
 * Poll for a pending keyboard character without blocking.
 *
 * Returns true when a character was written to @out_ch.  Returns false when no
 * input is available.
 */
bool monitor_platform_poll_keyboard(uint8_t *out_ch);

/**
 * Allow the platform to service background work (USB/DVI, host event loops,
 * etc.) between emulator bursts.
 */
void monitor_platform_idle(void);

/**
 * Read a line of monitor command input into @buffer.
 *
 * Returns true on success.  Returns false on EOF or when no input source is
 * configured.
 */
bool monitor_platform_readline(char *buffer, size_t buffer_length);

/**
 * Return a monotonically increasing microsecond counter suitable for scheduling
 * emulator time slices.
 */
uint64_t monitor_platform_time_us(void);

/**
 * Sleep until the requested deadline (in microseconds since the epoch reported
 * by monitor_platform_time_us()).  Implementations may clamp or busy-wait when
 * the deadline is in the past.
 */
void monitor_platform_sleep_until(uint64_t target_time_us);

/**
 * Provided by the monitor core: enqueue a character destined for the KL8E
 * console.  Platform backends call this from interrupt handlers or polling
 * loops when new keyboard input becomes available.
 */
void monitor_platform_enqueue_key(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* MONITOR_PLATFORM_H */
