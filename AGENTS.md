# Working on EmuMD

How to change EmuMD itself. Using it (porting a firmware, `mdfw.ini`,
running and testing in Hatari) is in [docs/GUIDE.md](docs/GUIDE.md), and
`skills/emumd` is the skill that firmware projects install with
`mdfw skill install`. Keep this file out of both, and both out of here.

## Layout

- `tools/mdfw`: the command line (Python, standard library only).
- `runtime/`: what every `.mdfw` is built with. Stand-ins for the Pico SDK,
  flash, FatFs, core 1 and the cartridge bus. `runtime/template` covers the
  SidecarTridge template's hardware, `runtime/wifi` covers `[wifi]`, and
  `runtime/bluepad32` covers `[bluetooth]` (Bluepad32's platform interface,
  with scripted pads).
- `include/emumd_plugin.h`: the interface between Hatari and a `.mdfw`.
- `hatari/build-hatari.sh`: builds EmuMD's Hatari from the fork (below).
- `examples/` (hello, wifi), `tests/`, `docs/GUIDE.md`, `skills/emumd/`.

## Principles

- **Stand-ins belong in EmuMD.** When a firmware needs something a host
  computer does not have, add the stand-in to the runtime (or
  `runtime/template`). Don't ask each firmware for glue.
- **Older pins keep working.** Projects pin EmuMD as a submodule and can stay
  on an old version for months. For example, the fork's Hatari builds in
  `~/.cache/emumd/hatari-fork`, so older EmuMDs keep their patched Hatari in
  `~/.cache/emumd/hatari`.
- **Never write inside EmuMD.** When building a firmware, objects and the
  `.mdfw` go to the project's `build/`.

## Changing Hatari

EmuMD's Hatari is the `emumd` branch of
[neilrackett/hatari](https://github.com/neilrackett/hatari). It is
Hatari 2.6.1 with one commit per change. To change it:

1. Commit on the fork's `emumd` branch, one commit per change. Keep fixes
   that aren't specific to the Multi-device free of EmuMD, so that they
   could go upstream as they are.
2. Push the fork.
3. Set `HATARI_COMMIT` in `hatari/build-hatari.sh` to the new commit, then
   push EmuMD. Never push EmuMD naming a commit the fork doesn't have yet.

The new script changes the stamp's hash, so `mdfw run` tells users to run
`mdfw hatari`.

## The plugin interface

`emumd_plugin.h` is in both repos: EmuMD's `include/` and the fork's
`src/includes/`. Keep the two byte-identical; `build-hatari.sh` refuses to
build otherwise. Any change to its structs or calls bumps `EMUMD_PLUGIN_ABI`,
and `PLUGIN_ABI` in `tools/mdfw` with it.

## Testing

- `make test`: the runtime, a firmware's `main()` and Bluetooth's scripted
  pads (`make test-bluetooth` on its own). `make test-wifi` needs libslirp.
- `make example`, then run it headless and look at the screenshot:
  `cd examples/hello && ../../tools/mdfw run --headless --frames 400 --screenshot out.png`.
- Real firmware, against this checkout:
  `make emu-test EMUMD=/path/to/emumd` in md-net and md-js, and
  `make emu EMUMD=...` in md-doom.
- After a Hatari change, run `mdfw hatari` first.

## Docs

- `docs/GUIDE.md` is for people using EmuMD; the README is the overview.
- `skills/emumd/SKILL.md` ships into firmware projects. Keep it in step with
  what `mdfw` does.

## Licences

- Everything is GPL-3.0-or-later, except the files shared with Hatari:
  `hatari/build-hatari.sh` and `include/emumd_plugin.h`. Those are
  GPL-2.0-or-later, like Hatari ([hatari/COPYING](hatari/COPYING)).
- New files start with the copyright and SPDX lines, and C files with a
  `File:`/`Description:` header. See `runtime/runtime.c`.
