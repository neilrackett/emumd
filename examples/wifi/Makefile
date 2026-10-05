# wifi: the EmuMD Wi-Fi example firmware.
# Copyright (C) 2026 Neil Rackett
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   make          build build/wi-fi.mdfw (needs libslirp)
#   make run      run it in Hatari (mdfw hatari first; URL=http://... to
#                 fetch another page)
#   make cart     rebuild cart/cart_image.h from cart/cart.s (needs stcmd)

MDFW := ../../tools/mdfw
URL ?=

.PHONY: all run cart clean
all:
	$(MDFW) build

run:
	$(MDFW) run $(if $(URL),-O url=$(URL))

cart:
	STCMD_NO_TTY=1 ST_WORKING_FOLDER=$(CURDIR) stcmd vasm -Faout -quiet -m68000 -devpac cart/cart.s -o cart/cart.o
	STCMD_NO_TTY=1 ST_WORKING_FOLDER=$(CURDIR) stcmd vlink -brawbin1 cart/cart.o -o cart/cart.bin
	$(MDFW) cart cart/cart.bin -o cart/cart_image.h
	rm -f cart/cart.o

clean:
	rm -rf build cart/cart.bin
