GBDK_HOME ?= /opt/gbdk
LCC := $(GBDK_HOME)/bin/lcc
CFLAGS := -Isrc -Ivendor/fatfs -DX7_FATFS -Wf--max-allocs-per-node -Wf50000
SOURCES := src/boot.c src/main.c src/browser.c src/ui.c src/storage_ui.c src/storage.c src/save_memory.c src/yellow.c src/yellow_data.c src/x7_disk.c src/checksum.c vendor/fatfs/ff.c
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))

.PHONY: all check
all: build/yellow-editor.gbc

build/%.o: %.c $(wildcard src/*.h) vendor/fatfs/ff.h vendor/fatfs/ffconf.h
	mkdir -p $(@D)
	$(LCC) $(CFLAGS) -c -o $@ $<

build/yellow-editor.gbc: $(OBJECTS) tools/check_map.py
	$(LCC) -Wl-yt0x19 -Wl-yo8 -Wm-ynYELLOWEDIT -Wm-yC -Wl-m -Wl-j -o $@ $(OBJECTS)
	python3 tools/check_map.py build/yellow-editor.map

check:
	python3 tests/check.py
