# md-emulator: SidecarTridge Multi-device firmware on the host, for Hatari.
# Copyright (C) 2026 Neil Rackett
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   make           build the example firmware (examples/hello)
#   make test      the runtime's self-test
#   make hatari    Hatari 2.6.1 with Multi-device support (~/.cache/md-emulator)
#   make clean

MDFW := tools/mdfw
CC ?= cc
TEST_CFLAGS := -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter \
	-Iruntime/shim -Iinclude -Iruntime

.PHONY: all example test hatari clean
all: example

example:
	$(MDFW) build -f examples/hello/mdfw.ini

build/runtime_test: runtime/*.c runtime/*.h runtime/shim/*.h runtime/shim/*/*.h \
		include/*.h tests/runtime_test.c
	@mkdir -p build
	$(CC) $(TEST_CFLAGS) runtime/*.c tests/runtime_test.c -lpthread -o $@

test: build/runtime_test
	rm -rf build/test-sd
	build/runtime_test build/test-sd

hatari:
	hatari/build-hatari.sh

clean:
	rm -rf build/runtime_test build/test-sd examples/*/build
