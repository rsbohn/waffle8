#define _POSIX_C_SOURCE 199309L
#include "watchdog.h"

#include "pdp8.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* control register layout: [11..9] = cmd (3 bits), [8..0] = count (9 bits) */
#define WATCHDOG_COUNT_MASK 0x01FFu
#define WATCHDOG_CMD_MASK 0x0E00u

enum watchdog_cmd {
    WD_CMD_DISABLE = 0,
    WD_CMD_RESET_ONE_SHOT = 1,
    WD_CMD_RESET_PERIODIC = 2,
    WD_CMD_HALT_ONE_SHOT = 3,
    WD_CMD_HALT_PERIODIC = 4,
    WD_CMD_INTERRUPT_ONE_SHOT = 5,
    WD_CMD_INTERRUPT_PERIODIC = 6,
};

struct pdp8_watchdog {
    uint16_t configured_count; /* in deciseconds, 9 bits */
    uint8_t cmd;               /* 3 bits */
    int enabled;
    int expired;
    uint64_t expiry_ns; /* monotonic time in ns */
};

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0ull;
    }
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void watchdog_fire(pdp8_t *cpu, pdp8_watchdog_t *wd) {
    if (!cpu || !wd) return;
    wd->expired = 1;
    wd->enabled = (wd->cmd == WD_CMD_RESET_PERIODIC || wd->cmd == WD_CMD_HALT_PERIODIC || 
                   wd->cmd == WD_CMD_INTERRUPT_PERIODIC) ? 1 : 0;
    /* action based on cmd */
    if (wd->cmd == WD_CMD_RESET_ONE_SHOT || wd->cmd == WD_CMD_RESET_PERIODIC) {
        /* Jump to reset vector 0000 */
        pdp8_api_set_pc(cpu, 0u);
    } else if (wd->cmd == WD_CMD_HALT_ONE_SHOT || wd->cmd == WD_CMD_HALT_PERIODIC) {
        /* Raise HALT */
        pdp8_api_set_halt(cpu);
    } else if (wd->cmd == WD_CMD_INTERRUPT_ONE_SHOT || wd->cmd == WD_CMD_INTERRUPT_PERIODIC) {
        /* Request interrupt via ISR polling */
        pdp8_api_request_interrupt(cpu, PDP8_WATCHDOG_DEVICE_CODE);
    }
    /* for periodic modes, expiry will be reloaded by tick handler */
}

static void watchdog_tick(pdp8_t *cpu, void *context, uint64_t now) {
    pdp8_watchdog_t *wd = (pdp8_watchdog_t *)context;
    if (!wd || !cpu) return;
    if (!wd->enabled) return;
    if (wd->configured_count == 0) {
        /* treat zero as immediate expiry */
        if (!wd->expired) {
            watchdog_fire(cpu, wd);
        }
        return;
    }
    if (now >= wd->expiry_ns && !wd->expired) {
        watchdog_fire(cpu, wd);
        if (wd->enabled) {
            /* periodic: schedule next expiry */
            uint64_t delta_ns = (uint64_t)wd->configured_count * 100000000ull; /* deciseconds -> ns */
            wd->expiry_ns = now + delta_ns;
            wd->expired = 0; /* still active until next fire */
        }
    }
}

static void watchdog_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    pdp8_watchdog_t *wd = (pdp8_watchdog_t *)context;
    if (!wd || !cpu) return;

    uint8_t func = (uint8_t)(instruction & 0x7u);
    uint64_t now = now_ns();

    switch (func) {
    case 0x0u: /* NOP - do nothing */
        break;

    case 0x1u: /* ISK - Interrupt Skip if expired flag set */
        if (wd->expired) {
            pdp8_api_request_skip(cpu);
        }
        break;

    case 0x2u: /* WRITE - load control register from AC */
        {
            uint16_t ac = pdp8_api_get_ac(cpu) & 0x0FFFu;
            uint8_t cmd = (uint8_t)((ac >> 9) & 0x7u);
            uint16_t count = (uint16_t)(ac & WATCHDOG_COUNT_MASK);
            wd->cmd = cmd;
            wd->configured_count = (uint16_t)(count & WATCHDOG_COUNT_MASK);
            wd->expired = 0;
            wd->enabled = (wd->cmd != WD_CMD_DISABLE) ? 1 : 0;
            if (wd->enabled) {
                /* start counting immediately */
                uint64_t delta_ns = (uint64_t)wd->configured_count * 100000000ull;
                wd->expiry_ns = now + delta_ns;
            }
        }
        break;

    case 0x3u: /* READ - read control register into AC */
        {
            uint16_t word = (uint16_t)(((wd->cmd & 0x7u) << 9) | (wd->configured_count & WATCHDOG_COUNT_MASK));
            pdp8_api_set_ac(cpu, word);
        }
        break;

    case 0x4u: /* RESTART - restart counter */
        wd->expired = 0;
        if (wd->configured_count == 0) {
            /* immediate expiry on restart if zero */
            wd->expiry_ns = now;
        } else {
            uint64_t delta_ns = (uint64_t)wd->configured_count * 100000000ull;
            wd->expiry_ns = now + delta_ns;
        }
        wd->enabled = (wd->cmd != WD_CMD_DISABLE) ? 1 : 0;
        break;

    case 0x5u:
    case 0x6u:
    case 0x7u:
        /* Undefined functions - in real hardware would raise a fault condition */
        /* For now, we silently ignore, but could log a warning */
        break;
    }
}

pdp8_watchdog_t *pdp8_watchdog_create(void) {
    pdp8_watchdog_t *wd = (pdp8_watchdog_t *)calloc(1, sizeof(pdp8_watchdog_t));
    if (!wd) return NULL;
    wd->configured_count = 0u;
    wd->cmd = WD_CMD_DISABLE;
    wd->enabled = 0;
    wd->expired = 0;
    wd->expiry_ns = 0ull;
    return wd;
}

void pdp8_watchdog_destroy(pdp8_watchdog_t *wd) {
    if (!wd) return;
    free(wd);
}

int pdp8_watchdog_attach(pdp8_t *cpu, pdp8_watchdog_t *wd) {
    if (!cpu || !wd) return -1;
    if (pdp8_api_register_iot(cpu, PDP8_WATCHDOG_DEVICE_CODE, watchdog_iot, wd) != 0) {
        return -1;
    }
    if (pdp8_api_register_tick(cpu, PDP8_WATCHDOG_DEVICE_CODE, watchdog_tick, wd) != 0) {
        pdp8_api_register_iot(cpu, PDP8_WATCHDOG_DEVICE_CODE, NULL, NULL);
        return -1;
    }
    return 0;
}

int pdp8_watchdog_get_status(const pdp8_watchdog_t *wd, struct pdp8_watchdog_status *out_status) {
    if (!wd || !out_status) return -1;
    memset(out_status, 0, sizeof(*out_status));
    out_status->enabled = wd->enabled ? 1 : 0;
    out_status->expired = wd->expired ? 1 : 0;
    out_status->cmd = (int)(wd->cmd & 0x7u);
    out_status->configured_count = (int)wd->configured_count;
    if (!wd->enabled) {
        out_status->remaining_ds = -1;
        return 0;
    }

    uint64_t now = now_ns();
    if (wd->expiry_ns <= now) {
        out_status->remaining_ds = 0;
        return 0;
    }
    uint64_t delta_ns = wd->expiry_ns - now;
    /* convert ns to deciseconds (1 ds = 100ms = 100,000,000 ns) */
    int remaining = (int)(delta_ns / 100000000ull);
    if (remaining < 0) remaining = 0;
    out_status->remaining_ds = remaining;
    return 0;
}
