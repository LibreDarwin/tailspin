# Copyright (C) 2026, LibreDarwin
# SPDX-License-Identifier: BSD-3-Clause
# Clean-room reimplementation of Apple's tailspin and tailspind commands.

CONFIG ?= release
SDK ?= /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
CC ?= clang
-include make/$(CONFIG).mk

BUILD_DIR := build/$(CONFIG)
OBJDIR := $(BUILD_DIR)/obj
CFLAGS := $(OPT) -std=c11 -D_DARWIN_C_SOURCE -isysroot "$(SDK)" -I Sources/common -Wall -Wextra
TAILSPIN := $(BUILD_DIR)/tailspin
TAILSPIND := $(BUILD_DIR)/tailspind
STATE_OBJ := $(OBJDIR)/tailspin_state.o $(OBJDIR)/tailspin_capture.o

all: $(TAILSPIN) $(TAILSPIND)

$(TAILSPIN): $(OBJDIR)/tailspin_main.o $(STATE_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TAILSPIND): $(OBJDIR)/tailspind_main.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/tailspin_main.o: Sources/tailspin/main.c Sources/common/tailspin_state.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ Sources/tailspin/main.c

$(OBJDIR)/tailspind_main.o: Sources/tailspind/main.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ Sources/tailspind/main.c

$(OBJDIR)/tailspin_state.o: Sources/common/tailspin_state.c Sources/common/tailspin_state.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ Sources/common/tailspin_state.c

$(OBJDIR)/tailspin_capture.o: Sources/common/tailspin_capture.c Sources/common/tailspin_capture.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ Sources/common/tailspin_capture.c

test: all
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) enable
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set buffer-size 64
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) info >/dev/null
	@config=$$(mktemp -d); ! TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set oncore-sampling-period 999999
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set sampling-option add:test-option; TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set sampling-option remove:test-option
	@file=$$(mktemp); { printf '\007\130\242\131'; dd if=/dev/zero bs=1 count=12 2>/dev/null; printf '\355\130\221\361'; dd if=/dev/zero bs=1 count=12 2>/dev/null; } > $$file; $(TAILSPIN) stat $$file >/dev/null; rm -f $$file
	@file=$$(mktemp); { printf '\007\130\242\131'; dd if=/dev/zero bs=1 count=12 2>/dev/null; printf '\007\130\242\131'; dd if=/dev/zero bs=1 count=12 2>/dev/null; printf '\355\130\221\361'; dd if=/dev/zero bs=1 count=12 2>/dev/null; } > $$file; $(TAILSPIN) stat -s $$file | grep -q '^0x59a25807 2$$'; rm -f $$file

install: all
	install -d $(DESTDIR)/usr/bin $(DESTDIR)/usr/libexec
	install -m 0755 $(TAILSPIN) $(DESTDIR)/usr/bin/tailspin
	install -m 0755 $(TAILSPIND) $(DESTDIR)/usr/libexec/tailspind
	install -d $(DESTDIR)/Library/LaunchDaemons
	install -m 0444 com.apple.tailspind.plist $(DESTDIR)/Library/LaunchDaemons/com.apple.tailspind.plist
	install -d $(DESTDIR)/usr/share/man/man1 $(DESTDIR)/usr/share/man/man8
	install -m 0444 man/tailspin.1 $(DESTDIR)/usr/share/man/man1/tailspin.1
	install -m 0444 man/tailspind.8 $(DESTDIR)/usr/share/man/man8/tailspind.8
	install -d $(DESTDIR)/AppleInternal/tailspin
	install -m 0444 tailspin-README.txt $(DESTDIR)/AppleInternal/tailspin/tailspin-README.txt

clean:
	rm -rf build

.PHONY: all test install clean
