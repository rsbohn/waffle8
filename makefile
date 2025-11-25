ALL: $(FACTORY_LIB) bin/monitor bin/pdp8v tools/pdp8_bench

HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
MONITOR_OBJS = src/monitor.c \
        src/monitor_config.c \
        src/monitor_platform_posix.c \
	src/emulator/main.c \
        src/emulator/board.c \
        src/emulator/kl8e_console.c \
        src/emulator/line_printer.c \
	src/emulator/watchdog.c \
	src/emulator/interrupt_control.c \
        src/emulator/paper_tape.c \
        src/emulator/paper_tape_device.c \
        src/emulator/paper_tape_punch.c \
        src/emulator/magtape_device.c

PDP8V_OBJS = src/pdp8v.c \
        src/pdp8v_runtime.c \
        src/monitor_config.c \
        src/monitor_platform_posix.c \
	src/emulator/main.c \
        src/emulator/board.c \
        src/emulator/kl8e_console.c \
        src/emulator/line_printer.c \
	src/emulator/watchdog.c \
	src/emulator/interrupt_control.c \
        src/emulator/paper_tape.c \
        src/emulator/paper_tape_device.c \
        src/emulator/paper_tape_punch.c \
        src/emulator/magtape_device.c

PDP8_BENCH_OBJS = tools/pdp8_bench.c \
	src/emulator/main.c \
	src/emulator/board.c \
	src/emulator/kl8e_console.c \
	src/emulator/line_printer.c \
	src/emulator/watchdog.c \
	src/emulator/interrupt_control.c \
	src/emulator/paper_tape.c \
	src/emulator/paper_tape_device.c \
	src/emulator/paper_tape_punch.c \
	src/emulator/magtape_device.c

FACTORY_LIB = factory/libpdp8.so
FACTORY_SOURCES = $(wildcard src/emulator/*.c)

$(FACTORY_LIB): $(FACTORY_SOURCES)
	$(HOST_CC) $(HOST_CFLAGS) -fPIC -shared $^ -o $@

bin/monitor: $(FACTORY_LIB) $(MONITOR_OBJS) | bin
	$(HOST_CC) $(HOST_CFLAGS) -Isrc $(filter %.c,$^) -o $@

bin/pdp8v: $(FACTORY_LIB) $(PDP8V_OBJS) | bin
	$(HOST_CC) $(HOST_CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lncurses

# Compatibility alias for old monitor target
monitor: bin/monitor

bin:
	mkdir -p bin

tools/pdp8_bench: $(PDP8_BENCH_OBJS)
	$(HOST_CC) $(HOST_CFLAGS) $(filter %.c,$^) -o $@

clean:
	-@rm bin/monitor bin/pdp8v $(FACTORY_LIB) tests/pdp8_tests tools/pdp8_bench
	-@rmdir bin 2>/dev/null || true

.SUFFIXES: .ft .pa

%.pa: %.ft
	tools/moet $< $@
