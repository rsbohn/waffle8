ALL: unit TEST.PRG HECK.PRG

CC=cl65 -t cx16

unit: test.c scan.h scan.c
	cc -o $@ test.c scan.c

HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
MONITOR_OBJS = tools/monitor.c src/emulator/main.c src/emulator/board.c

monitor: $(MONITOR_OBJS)
	$(HOST_CC) $(HOST_CFLAGS) $(filter %.c,$^) -o $@

OBJLIST=scan.o graphics.o
HECK.PRG: heck.c $(OBJLIST)
	$(CC) -o $@ $< $(OBJLIST)

TD.PRG: tiledemo.c
	$(CC) -o $@ $<

TEST.PRG: test.c scan.c scan.h
	$(CC) -o $@ test.c scan.c




clean:
	-@rm dump* memory.bin monitor
