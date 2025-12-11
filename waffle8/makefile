# Makefile for building waffle8 snapshot directory
# Usage: make [clean|all|snapshot]

SNAPSHOT_DIR = .
BIN_DIR = $(SNAPSHOT_DIR)/bin
DEMO_DIR = $(SNAPSHOT_DIR)/demo
MAN_DIR = $(SNAPSHOT_DIR)/man/1
MEDIA_DIR = $(SNAPSHOT_DIR)/media

# Source paths relative to parent directory
SRC_ROOT = ..
BIN_SRC = $(SRC_ROOT)/bin
TOOLS_SRC = $(SRC_ROOT)/tools
BOOT_SRC = $(SRC_ROOT)/pdp8
DEMO_SRC = $(SRC_ROOT)/demo
MAN_SRC = $(SRC_ROOT)/man/1
MEDIA_SRC = $(SRC_ROOT)/media

# Binary files
BIN_FILES = $(BIN_DIR)/monitor $(BIN_DIR)/pdp8v \
			$(BIN_DIR)/moet \
            $(BIN_DIR)/8asm \
			$(BIN_DIR)/pdp8_asm.py \
			$(BIN_DIR)/DECtape \
            $(BIN_DIR)/tu56/__init__.py

# Demo files
DEMO_FILES = $(DEMO_DIR)/blkvue.pa $(DEMO_DIR)/cal3.asm \
             $(DEMO_DIR)/clock.asm $(DEMO_DIR)/ptprint.asm \
             $(DEMO_DIR)/tc08write.asm

# Media files
MEDIA_FILES = $(MEDIA_DIR)/lorem.tape $(MEDIA_DIR)/tape0.tu56 \
              $(MEDIA_DIR)/tape1.tu56

# Root files
ROOT_FILES = $(SNAPSHOT_DIR)/pdp8.config \
             $(SNAPSHOT_DIR)/pelican.txt \
             $(SNAPSHOT_DIR)/README.md

.PHONY: all clean snapshot dirs

all: dirs $(BIN_FILES) $(DEMO_FILES) $(MEDIA_FILES) $(ROOT_FILES) man

dirs:
	mkdir -p $(BIN_DIR)/tu56 $(DEMO_DIR) $(MAN_DIR) $(MEDIA_DIR)

# Binary targets
$(BIN_DIR)/monitor: $(BIN_SRC)/monitor
	cp $< $@

$(BIN_DIR)/pdp8v: $(BIN_SRC)/pdp8v
	cp $< $@

$(BIN_DIR)/moet: $(TOOLS_SRC)/moet
	cp $< $@

$(BIN_DIR)/8asm: $(TOOLS_SRC)/8asm
	cp $< $@
	chmod +x $@

$(BIN_DIR)/pdp8_asm.py: $(TOOLS_SRC)/pdp8_asm.py
	cp $< $@

$(BIN_DIR)/DECtape: $(TOOLS_SRC)/DECtape
	cp $< $@

$(BIN_DIR)/tu56/__init__.py: $(TOOLS_SRC)/tu56/__init__.py
	mkdir -p $(BIN_DIR)/tu56
	cp $< $@

# Demo targets
$(DEMO_DIR)/blkvue.pa: $(BOOT_SRC)/blkvue.pa
	cp $< $@

$(DEMO_DIR)/cal3.asm: $(DEMO_SRC)/cal3.asm
	cp $< $@

$(DEMO_DIR)/clock.asm: $(DEMO_SRC)/clock.asm
	cp $< $@

$(DEMO_DIR)/ptprint.asm: $(DEMO_SRC)/ptprint.asm
	cp $< $@

$(DEMO_DIR)/tc08write.asm: $(DEMO_SRC)/tc08write.asm
	cp $< $@

# Media targets
$(MEDIA_DIR)/lorem.tape: ../tapes/lorem.tape
	cp $< $@

$(MEDIA_DIR)/tape0.tu56: $(MEDIA_SRC)/tape0.tu56
	cp $< $@

$(MEDIA_DIR)/tape1.tu56: $(MEDIA_SRC)/tape1.tu56
	cp $< $@

# Root file targets
$(SNAPSHOT_DIR)/pdp8.config: $(SRC_ROOT)/docs/pdp8.config.sample
	cp $< $@

$(SNAPSHOT_DIR)/pelican.txt: $(SRC_ROOT)/pelican.txt
	cp $< $@

$(SNAPSHOT_DIR)/README.md: $(SRC_ROOT)/README.md
	cp $< $@

# Man pages (copy all)
.PHONY: man
man: dirs
	cp $(MAN_SRC)/* $(MAN_DIR)/ 2>/dev/null || true

clean:
	rm -rf $(BIN_DIR) $(DEMO_DIR) $(MAN_DIR) $(MEDIA_DIR)
	rm -f $(ROOT_FILES)

# Create tarball snapshot
snapshot: all
	cd .. && tar czf snap-$$(date +%m%d).tar.gz waffle8/
	@echo "Snapshot created: ../snap-$$(date +%m%d).tar.gz"
