CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -I../src/emulator
LDFLAGS ?=

TEST_BINARY := pdp8_tests
TEST_SOURCES := test_emulator.c
CORE_SOURCES := ../src/emulator/main.c \
	../src/emulator/board.c \
	../src/emulator/kl8e_console.c \
	../src/emulator/line_printer.c \
	../src/emulator/paper_tape.c \
	../src/emulator/paper_tape_device.c

$(TEST_BINARY): $(TEST_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES) $(CORE_SOURCES) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TEST_BINARY)
