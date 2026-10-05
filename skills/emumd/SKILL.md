---
name: emumd
description: Build and run SidecarTridge Multi-device (RP2040) firmware in Hatari with EmuMD. Use when porting a Multi-device firmware to EmuMD, writing or fixing its mdfw.ini or emu/mdfw_app.c, fixing mdfw build or link errors, or running a firmware (and the Atari ST software that talks to it) headlessly to check what the ST shows.
---

# EmuMD

EmuMD builds a Multi-device firmware's own C sources for the host as a
`.mdfw` (a shared library) and runs it in a patched Hatari, on the emulated
cartridge port. The firmware's hardware set-up (PIO, DMA, clocks, SD driver,
Wi-Fi) is left out; EmuMD's runtime stands in for the Pico SDK, flash,
FatFs, core 1 and the ROM3/ROM4 bus. It is not RP2040 emulation: the `.uf2`
is not used.

## Find the tool

In order: `emu/emumd/tools/mdfw` (the usual submodule), `mdfw` on
`PATH`, or `<emumd>/tools/mdfw` — this skill's folder is
`<emumd>/skills/emumd` (resolve symlinks with `pwd -P`). Below,
`mdfw` means that script. Reference: `<emumd>/docs/GUIDE.md`, the API in
`<emumd>/include/mdfw.h`, a minimal firmware in
`<emumd>/examples/hello`.

If the project has no EmuMD yet, add it as a submodule (pins the
version the firmware was tested with):

```sh
git submodule add https://github.com/neilrackett/emumd.git emu/emumd
```

## Porting a firmware

1. `mdfw init` in the firmware repository: writes `mdfw.ini` and
   `emu/mdfw_app.c`.
2. Read the firmware's `main()` (and whatever it calls first, often
   `emul.c`) to separate hardware set-up from logic. In `mdfw.ini`
   `[sources] files`, list the logic plus `emu/mdfw_app.c`. Leave out
   `main.c`, `romemul.c`, `commemul.c`, `sdcard.c`, `hw_config.c`,
   `select.c`, `reset.c`, network, display, USB and settings code unless the
   logic needs them.
3. `[compile] include`: the firmware's include folders. EmuMD's
   stand-ins (Pico SDK headers, `ff.h`, `debug.h`) are searched before them,
   so hardware versions of those headers are skipped automatically.
4. Write `emu/mdfw_app.c`: `init` does what `main()` does after the
   hardware set-up; `poll` is one pass of the main loop, returning true if it
   did work. If the main loop never returns or blocks (framebuffer-template
   apps wait for the ST's VBL in `fb_publish`), give the rest of `main()` as
   `.main` instead: it runs on its own thread as core 0, where sleeps wait
   for emulated time. Keep `init` to what the ST must see at once (ROM4).
   - Load the cartridge image: `mdfw_rom4_load(target_firmware,
     target_firmware_length)` (the firmware build's `target_firmware.h`; or
     `mdfw cart image.bin -o cart.h`).
   - Wherever the firmware takes the ROM4 RAM address (ROM_IN_RAM,
     `0x20030000`, a `rom_base` argument), pass `mdfw_rom4_base()`.
   - ROM3: template code using `commemul_init()`/`commemul_poll()` works
     unchanged; otherwise `mdfw_rom3_set_irq()` + `mdfw_rom3_pop()`.
   - Name/version: `MDFW_NAME` / `MDFW_VERSION` come from `mdfw.ini`.
5. `mdfw build`, then fix what it reports (next section) until it links.
6. Run it headlessly and look (below). Also `mdfw info build/<name>.mdfw`.

## Fixing build errors

| Error | Fix |
| --- | --- |
| `'hardware/xyz.h' file not found` (or another SDK header) | Leave out the source that needs it, or add a minimal header in a shims folder (`[compile] shims = emu/shim`) |
| Linker-script symbols (`__flash_binary_end`, `_pack_flash_start`, config/ROM addresses) | A shim `constants.h` (or similar) defining them on `mdfw_flash[offset]` (2 MB, `XIP_BASE` = its address) or `mdfw_rom4()` |
| `Undefined symbols` / `undefined reference` at link | A left-out file defines it: add the file if it is logic, else stub the function in `emu/mdfw_app.c` |
| Code needs core 1 | `multicore_launch_core1` runs it as a host thread. For determinism a firmware's own "run a job on core 1" helper can run jobs inline in the glue instead |
| `__not_in_flash_func`, `tight_loop_contents`, `__dmb`... | Already provided by `pico.h`; include it |
| A stand-in header is ignored: another header in the firmware's folder includes the original in quotes | Give the stand-in the original's include guard and force-include it first: `cflags = -include prefix.h`, the prefix in the shims folder |
| `cast to smaller integer type`, or flash/config reads wrong after `(uint32_t)&sym - XIP_BASE` | Pointers are 64-bit: do the arithmetic in `uintptr_t`. Pointers passed through the inter-core FIFO need another route (a slot, with the FIFO as the signal) |
| `uint` undeclared (newlib's `sys/types.h` brings it on the RP2040) | `-include sys/types.h` in the prefix |
| `section` attribute not valid (macOS) | The SDK's `__scratch_x("name")`-style macros, which the stand-ins empty; a custom one via a shim |
| Settings or other structures in flash read back wrong | Should not happen: EmuMD builds with `-fshort-enums` like arm-none-eabi. Check for other layout assumptions (`sizeof(void *)`) |

Defines go in `[compile] defines`, one per line (`RELEASE_VERSION=MDFW_VERSION`).
EmuMD defines `PICO_BUILD=1`, `PICO_ON_DEVICE=0` (the SDK's host platform,
so upstream host code paths are taken) and `EMUMD=1`. `.cpp`/`.cc` build
as C++17; `[compile] cxxflags` for C++-only flags.

## Running and checking

```sh
mdfw hatari        # once: builds the patched Hatari into ~/.cache/emumd
                   # and downloads EmuTOS 1.4 alongside it
mdfw run --headless --frames 400 --no-user-config \
         --sd sd --screenshot out.png --log run.log --timeout 300
```

Then read `run.log` and view `out.png` (the last frame). Firmware log lines
are prefixed with its name; add `-V` for its `DPRINTF` output. Pass firmware
options with `-O key=value`, and raw Hatari options after `--`.

- `--headless` without `--frames` stops after 500 frames (10 s at 50 Hz).
  Never run `mdfw run` without `--headless`/`--frames` in automation: it opens
  a window and runs until closed.
- The TOS is EmuTOS 1.4 (UK) unless `--tos` or `mdfw.ini` names another.
- `--no-user-config` ignores the user's Hatari settings (which may add a
  GEMDOS drive, other memory).
- No GEMDOS drive (the default) lets the firmware's cartridge boot code run.
  With `--harddrive DIR` Hatari keeps the first 1 KB of the cartridge, so the
  boot code does not run, but ST programs on that drive can talk to the
  firmware. Autostart one with `-- --auto 'C:\PROG.TOS'`.
- Big ST programs need more RAM: `-- --memsize 4`.
- `--record out.avi` records video and sound, every frame at 50 Hz
  (`-- --avi-fps 60` for a 60 Hz TOS). Let the run end by itself
  (`--frames`): one stopped by `--timeout` leaves the AVI unfinished.
- The ST's text screen is 40 columns in low resolution.
- The emulated Multi-device is infinitely fast: do not draw conclusions
  about speed or timeouts; that needs real hardware.
- To type into the ST: `-- --cmd-fifo FILE`, then write
  `hatari-event keypress 28` (ST scancodes; `keydown`/`keyup` to hold)
  to FILE while it runs. Use `keypress` for typing: with `--headless`'s
  fast-forward, a host pause between `keydown` and `keyup` lasts long
  enough for TOS to repeat the key. `keypress 0` is scancode 0, not the
  digit (that is 11).
- To use the mouse, write `hatari-event mousemove DX DY` (relative, in ST
  pixels), `leftdown` and `leftup` to the same FILE. Pin the pointer
  first, e.g. `mousemove -100 0` four times then `mousemove 0 -100` three
  times (left then up, so it does not open menus on the way), and later
  moves land at known positions.
- A reboot keeps the firmware's variables (the RP2040 would start
  afresh), so `init` must set up what it relies on.
- If mdfw warns that Hatari was built from a different version of EmuMD, run
  `mdfw hatari`.
