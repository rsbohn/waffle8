ALL: $(FACTORY_LIB) monitor

HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
MONITOR_OBJS = tools/monitor.c \
	src/emulator/main.c \
	src/emulator/board.c \
	src/emulator/kl8e_console.c \
	src/emulator/line_printer.c \
	src/emulator/paper_tape.c \
	src/emulator/paper_tape_device.c

FACTORY_LIB = factory/libpdp8.so
FACTORY_SOURCES = $(wildcard src/emulator/*.c)

$(FACTORY_LIB): $(FACTORY_SOURCES)
	$(HOST_CC) $(HOST_CFLAGS) -fPIC -shared $^ -o $@

monitor: $(MONITOR_OBJS)
	$(HOST_CC) $(HOST_CFLAGS) $(filter %.c,$^) -o $@




clean:
	-@rm monitor $(FACTORY_LIB)
