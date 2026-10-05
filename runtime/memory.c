/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: memory.c
 * Description: The RP2040's 2 MB of flash (mdfw_flash) and the RAM that
 *              the cartridge is served from (mdfw_rom_in_ram: ROM4, then
 *              ROM3), laid out in assembly so that the SidecarTridge
 *              template's linker-script symbols (memmap_rp.ld) are real
 *              symbols at their real offsets: __rom_in_ram_start__ is
 *              ROM4, _config_flash_start is 0x1E0000 into the flash, and
 *              so on, so `&_config_flash_start - XIP_BASE` works as on the
 *              device. Zero-filled, so they take no room in the .mdfw.
 */

#define STR2(x) #x
#define STR(x) STR2(x)
#define PREFIX STR(__USER_LABEL_PREFIX__)

/* #name right here, so a firmware's macro of the same name (a stand-in
 * for the symbol) is not expanded into it. */
#ifdef __APPLE__
#define SECTION ".section __DATA,__bss\n"
#define LABEL(name) \
  ".globl " PREFIX #name "\n.private_extern " PREFIX #name "\n" PREFIX #name ":\n"
#else
#define SECTION ".bss\n"
#define LABEL(name) ".globl " PREFIX #name "\n.hidden " PREFIX #name "\n" PREFIX #name ":\n"
#endif

/* Flash, as the Booster lays it out for every app. */
__asm__(SECTION
        ".p2align 12\n"
        LABEL(mdfw_flash)
        LABEL(__flash_binary_start)
        ".space 0x100000\n"
        LABEL(_rom_temp_start)          /* 0x100000: a ROM image, loaded */
        ".space 0x20000\n"
        LABEL(_booster_app_flash_start) /* 0x120000 */
        ".space 0xC0000\n"
        LABEL(_config_flash_start)      /* 0x1E0000: the apps' settings */
        ".space 0x1E000\n"
        LABEL(_global_lookup_flash_start) /* 0x1FE000: app -> settings sector */
        ".space 0x1000\n"
        LABEL(_global_config_flash_start) /* 0x1FF000: the global settings */
        ".space 0x1000\n");

/* The 128 KB the cartridge is served from: ROM4 ($FA0000), then ROM3. */
__asm__(SECTION
        ".p2align 4\n"
        LABEL(mdfw_rom_in_ram)
        LABEL(__rom_in_ram_start__)
        ".space 0x20000\n");
