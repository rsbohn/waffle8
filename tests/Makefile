CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -I../src/emulator
LDFLAGS ?=

TEST_BINARY := pdp8_tests
TEST_SOURCES := test_emulator.c
CORE_SOURCES := ../src/emulator/main.c \
	../src/emulator/board.c \
	../src/emulator/kl8e_console.c

$(TEST_BINARY): $(TEST_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES) $(CORE_SOURCES) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TEST_BINARY)
