# Copyright (C) 2026, LibreDarwin
# SPDX-License-Identifier: BSD-3-Clause
# Clean-room reimplementation of Apple's tailspin and tailspind commands.

CONFIG ?= release
SDK ?= /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
KTRACE_ROOT ?= ../ktrace
KTRACE_LIB := $(KTRACE_ROOT)/build/libktrace.a
CC ?= clang
-include make/$(CONFIG).mk

BUILD_DIR := build/$(CONFIG)
OBJDIR := $(BUILD_DIR)/obj
CFLAGS := $(OPT) -std=c11 -D_DARWIN_C_SOURCE -isysroot "$(SDK)" -I Sources/common -I "$(KTRACE_ROOT)/Sources" -Wall -Wextra
TAILSPIN := $(BUILD_DIR)/tailspin
TAILSPIND := $(BUILD_DIR)/tailspind
LIBTAILSPIN := $(BUILD_DIR)/libtailspin.dylib
TAILSPIN_LOG_OBJ := $(OBJDIR)/tailspin_log.o
STATE_OBJ := $(OBJDIR)/tailspin_state.o $(OBJDIR)/tailspin_capture.o
LIBTAILSPIN_OBJ := $(OBJDIR)/tailspin_library.o $(OBJDIR)/tailspin_exports.o $(OBJDIR)/tailspin_log_pic.o $(OBJDIR)/tailspin_state_pic.o $(OBJDIR)/tailspin_capture_pic.o

all: $(KTRACE_LIB) $(TAILSPIN) $(TAILSPIND) $(LIBTAILSPIN)

$(KTRACE_LIB):
	$(MAKE) -C "$(KTRACE_ROOT)" all

$(TAILSPIN): $(OBJDIR)/tailspin_main.o $(STATE_OBJ) $(TAILSPIN_LOG_OBJ) $(KTRACE_LIB)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -framework CoreFoundation -framework Foundation -o $@ $^

$(TAILSPIND): $(OBJDIR)/tailspind_main.o $(OBJDIR)/tailspin_capture.o $(OBJDIR)/tailspin_state.o $(KTRACE_LIB)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(LIBTAILSPIN): $(LIBTAILSPIN_OBJ) $(KTRACE_LIB)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -dynamiclib -install_name /usr/lib/libtailspin.dylib -framework Foundation -o $@ $^

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

$(OBJDIR)/tailspin_log.o: Sources/libtailspin/tailspin_log.m Sources/libtailspin/tailspin_log.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -fobjc-arc -fblocks -c -o $@ Sources/libtailspin/tailspin_log.m

$(OBJDIR)/tailspin_library.o: Sources/libtailspin/tailspin_library.c Sources/libtailspin/tailspin_library.h Sources/common/tailspin_private_abi.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -fblocks -c -o $@ Sources/libtailspin/tailspin_library.c

$(OBJDIR)/tailspin_exports.o: Sources/libtailspin/tailspin_exports.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -c -o $@ Sources/libtailspin/tailspin_exports.c

$(OBJDIR)/tailspin_state_pic.o: Sources/common/tailspin_state.c Sources/common/tailspin_state.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -c -o $@ Sources/common/tailspin_state.c

$(OBJDIR)/tailspin_capture_pic.o: Sources/common/tailspin_capture.c Sources/common/tailspin_capture.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -c -o $@ Sources/common/tailspin_capture.c

$(OBJDIR)/tailspin_log_pic.o: Sources/libtailspin/tailspin_log.m Sources/libtailspin/tailspin_log.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -fobjc-arc -fblocks -c -o $@ Sources/libtailspin/tailspin_log.m

test: all
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) enable
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set buffer-size 64
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) info >/dev/null
	@config=$$(mktemp -d); printf 'buffer-size-mb=0\ncputrace=invalid\n' > $$config/config; ! TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) info >/dev/null 2>&1
	@config=$$(mktemp -d); ! TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set oncore-sampling-period 999999
	@config=$$(mktemp -d); TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set sampling-option add:test-option; TAILSPIN_CONFIG_PATH=$$config/config $(TAILSPIN) set sampling-option remove:test-option
	@file=$$(mktemp); { printf '\007\130\242\131'; dd if=/dev/zero bs=1 count=12 2>/dev/null; printf '\355\130\221\361'; dd if=/dev/zero bs=1 count=12 2>/dev/null; } > $$file; $(TAILSPIN) stat $$file >/dev/null; rm -f $$file
	@file=$$(mktemp); { printf '\007\130\242\131'; dd if=/dev/zero bs=1 count=12 2>/dev/null; printf '\007\130\242\131'; dd if=/dev/zero bs=1 count=12 2>/dev/null; printf '\355\130\221\361'; dd if=/dev/zero bs=1 count=12 2>/dev/null; } > $$file; $(TAILSPIN) stat -s $$file | grep -q '^0x59a25807 2$$'; rm -f $$file
	@file=$$(mktemp); { printf '\000\003\252\125\001\000\000\000\000\000\000\000\000\000\000\000'; printf '\023\200\000\000\001\000\000\000\060\000\000\000\000\000\000\000'; dd if=/dev/zero bs=1 count=48 2>/dev/null; printf '\006\200\000\000\001\000\004\000\116\044\000\000\000\000\000\000'; dd if=/dev/zero bs=1 count=9294 2>/dev/null; } > $$file; $(TAILSPIN) stat -v $$file | grep -q 'Format: Apple tailspin'; rm -f $$file
	@! $(TAILSPIN) augment >/dev/null 2>&1
	@file=$$(mktemp); ! $(TAILSPIN) augment -s $$file >/dev/null 2>&1; rm -f $$file
	@$(TAILSPIN) save --no-symbolicate --gather-os-logs --gather-os-signposts --scrub-output --cputrace >/dev/null 2>&1; test $$? -eq 64
	@$(TAILSPIN) save --for-last-seconds invalid >/dev/null 2>&1; test $$? -eq 64
	@$(TAILSPIN) save --for-last-seconds 18446744073709551615 >/dev/null 2>&1; test $$? -eq 64
	@$(TAILSPIN) set cputrace invalid >/dev/null 2>&1; test $$? -eq 64

install: all
	install -d $(DESTDIR)/usr/bin $(DESTDIR)/usr/libexec $(DESTDIR)/usr/lib
	install -m 0755 $(TAILSPIN) $(DESTDIR)/usr/bin/tailspin
	install -m 0755 $(TAILSPIND) $(DESTDIR)/usr/libexec/tailspind
	install -m 0755 $(LIBTAILSPIN) $(DESTDIR)/usr/lib/libtailspin.dylib
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
