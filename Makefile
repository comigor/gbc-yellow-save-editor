GBDK_HOME ?= /opt/gbdk
LCC := $(GBDK_HOME)/bin/lcc
CFLAGS := -Isrc -Ivendor/fatfs -DX7_FATFS -Wf--max-allocs-per-node -Wf50000
SOURCES := src/boot.c src/main.c src/browser.c src/ui.c src/storage_ui.c src/storage.c src/save_memory.c src/yellow.c src/yellow_data.c src/x7_disk.c src/checksum.c vendor/fatfs/ff.c

ifdef YELLOW_ROM
BUILD := build/private
ROM_NAME := yellow-editor-sprites
ROM_TITLE := YELLOWSPRITE
ROM_BANKS := 16
CFLAGS += -DYELLOW_GRAPHICS -I$(BUILD)/graphics
SOURCES += src/graphics.c
GRAPHICS_BANKS := 0 1 2 3 4 5 6 7
GRAPHICS_OBJECTS := $(addprefix $(BUILD)/graphics/front_,$(addsuffix .o,$(GRAPHICS_BANKS))) $(BUILD)/graphics/icons.o
GRAPHICS_STAMP := $(BUILD)/graphics/.stamp
else
BUILD := build
ROM_NAME := yellow-editor
ROM_TITLE := YELLOWEDIT
ROM_BANKS := 8
endif
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES)) $(GRAPHICS_OBJECTS)

.PHONY: all check FORCE
all: $(BUILD)/$(ROM_NAME).gbc

$(BUILD)/%.o: %.c $(wildcard src/*.h) vendor/fatfs/ff.h vendor/fatfs/ffconf.h $(GRAPHICS_STAMP)
	mkdir -p $(@D)
	$(LCC) $(CFLAGS) -c -o $@ $<

ifdef YELLOW_ROM
$(GRAPHICS_STAMP): FORCE tools/extract_yellow_graphics.py tools/yellow_graphics.json
	python3 tools/extract_yellow_graphics.py --rom "$(YELLOW_ROM)" --output $(BUILD)/graphics
	@touch $@

$(BUILD)/graphics/%.o: $(GRAPHICS_STAMP)
	$(LCC) $(CFLAGS) -c -o $@ $(BUILD)/graphics/$*.c
endif

$(BUILD)/$(ROM_NAME).gbc: $(OBJECTS) tools/check_map.py
	$(LCC) -Wl-yt0x19 -Wl-yo$(ROM_BANKS) -Wm-yn$(ROM_TITLE) -Wm-yC -Wl-m -Wl-j -o $@ $(OBJECTS)
	python3 tools/check_map.py $(BUILD)/$(ROM_NAME).map

check:
	python3 tests/check.py
