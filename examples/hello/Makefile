# hello: the EmuMD example firmware.
# Copyright (C) 2026 Neil Rackett
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   make          build build/hello.mdfw
#   make run      run it in Hatari (mdfw hatari first; pass TOS=/path/to/tos.img)
#   make cart     rebuild cart/cart_image.h from cart/cart.s (needs stcmd)

MDFW := ../../tools/mdfw
TOS ?=

.PHONY: all run cart clean
all:
	$(MDFW) build

run:
	$(MDFW) run $(if $(TOS),--tos $(TOS))

cart:
	STCMD_NO_TTY=1 ST_WORKING_FOLDER=$(CURDIR) stcmd vasm -Faout -quiet -m68000 -devpac cart/cart.s -o cart/cart.o
	STCMD_NO_TTY=1 ST_WORKING_FOLDER=$(CURDIR) stcmd vlink -brawbin1 cart/cart.o -o cart/cart.bin
	$(MDFW) cart cart/cart.bin -o cart/cart_image.h
	rm -f cart/cart.o

clean:
	rm -rf build cart/cart.bin
