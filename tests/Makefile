CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -I../src/emulator
LDFLAGS ?=

TEST_BINARY := pdp8_tests
TEST_SOURCES := test_emulator.c
TEST_CONFIG_BINARY := test_config
TEST_CONFIG_SOURCE := test_config.c
TEST_RUNTIME_BINARY := test_runtime
TEST_RUNTIME_SOURCE := test_runtime_failure.c
CORE_SOURCES := ../src/emulator/main.c \
	../src/emulator/board.c \
        ../src/emulator/kl8e_console.c \
        ../src/emulator/line_printer.c \
        ../src/emulator/paper_tape.c \
        ../src/emulator/paper_tape_device.c \
        ../src/emulator/magtape_device.c \
        ../src/emulator/watchdog.c \
        ../src/emulator/interrupt_control.c


ALL_TESTS := $(TEST_BINARY) $(TEST_CONFIG_BINARY) $(TEST_RUNTIME_BINARY)

all: $(ALL_TESTS)

$(TEST_CONFIG_BINARY): $(TEST_CONFIG_SOURCE) $(CORE_SOURCES) ../src/monitor_config.c ../src/monitor_platform_posix.c
	$(CC) $(CFLAGS) -o $@ $(TEST_CONFIG_SOURCE) $(CORE_SOURCES) ../src/monitor_config.c ../src/monitor_platform_posix.c $(LDFLAGS)

$(TEST_BINARY): $(TEST_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES) $(CORE_SOURCES) $(LDFLAGS)

$(TEST_RUNTIME_BINARY): $(TEST_RUNTIME_SOURCE) ../src/pdp8v_runtime.c ../src/monitor_config.c $(CORE_SOURCES)
	$(CC) $(CFLAGS) -DPDP8V_ENABLE_TEST_HOOKS -I../src -o $@ $(TEST_RUNTIME_SOURCE) ../src/pdp8v_runtime.c ../src/monitor_config.c $(CORE_SOURCES) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(ALL_TESTS)
