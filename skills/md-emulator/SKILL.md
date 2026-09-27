---
name: md-emulator
description: Build and run SidecarTridge Multi-device (RP2040) firmware in Hatari with md-emulator. Use when porting a Multi-device firmware to md-emulator, writing or fixing its mdfw.ini or emu/mdfw_app.c, fixing mdfw build or link errors, or running a firmware (and the Atari ST software that talks to it) headlessly to check what the ST shows.
---

# md-emulator

md-emulator builds a Multi-device firmware's own C sources for the host as a
`.mdfw` (a shared library) and runs it in a patched Hatari, on the emulated
cartridge port. The firmware's hardware set-up (PIO, DMA, clocks, SD driver,
Wi-Fi) is left out; md-emulator's runtime stands in for the Pico SDK, flash,
FatFs, core 1 and the ROM3/ROM4 bus. It is not RP2040 emulation: the `.uf2`
is not used.

## Find the tool

In order: `emu/md-emulator/tools/mdfw` (the usual submodule), `mdfw` on
`PATH`, or `<md-emulator>/tools/mdfw` — this skill's folder is
`<md-emulator>/skills/md-emulator` (resolve symlinks with `pwd -P`). Below,
`mdfw` means that script. Reference: `<md-emulator>/docs/GUIDE.md`, the API in
`<md-emulator>/include/mdfw.h`, a minimal firmware in
`<md-emulator>/examples/hello`.

If the project has no md-emulator yet, add it as a submodule (pins the
version the firmware was tested with):

```sh
git submodule add https://github.com/neilrackett/md-emulator.git emu/md-emulator
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
3. `[compile] include`: the firmware's include folders. md-emulator's
   stand-ins (Pico SDK headers, `ff.h`, `debug.h`) are searched before them,
   so hardware versions of those headers are skipped automatically.
4. Write `emu/mdfw_app.c`: `init` does what `main()` does after the
   hardware set-up; `poll` is one pass of the main loop, returning true if it
   did work.
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

Defines go in `[compile] defines`, one per line (`RELEASE_VERSION=MDFW_VERSION`).

## Running and checking

```sh
mdfw hatari        # once: builds the patched Hatari into ~/.cache/md-emulator
mdfw run --headless --frames 400 --no-user-config --tos TOS.IMG \
         --sd sd --screenshot out.png --log run.log --timeout 300
```

Then read `run.log` and view `out.png` (the last frame). Firmware log lines
are prefixed with its name; add `-V` for its `DPRINTF` output. Pass firmware
options with `-O key=value`, and raw Hatari options after `--`.

- `--headless` without `--frames` stops after 500 frames (10 s at 50 Hz).
  Never run `mdfw run` without `--headless`/`--frames` in automation: it opens
  a window and runs until closed.
- `--no-user-config` ignores the user's Hatari settings (which may add a
  GEMDOS drive, other TOS, other memory); it then needs `--tos`.
- No GEMDOS drive (the default) lets the firmware's cartridge boot code run.
  With `--harddrive DIR` Hatari keeps the first 1 KB of the cartridge, so the
  boot code does not run, but ST programs on that drive can talk to the
  firmware. Autostart one with `-- --auto 'C:\PROG.TOS'`.
- Big ST programs need more RAM: `-- --memsize 4`.
- The ST's text screen is 40 columns in low resolution.
- The emulated Multi-device is infinitely fast: do not draw conclusions
  about speed or timeouts; that needs real hardware.
- If mdfw warns that Hatari was built from a different md-emulator, run
  `mdfw hatari`.
