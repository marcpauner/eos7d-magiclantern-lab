# 4. Soup Engine as a 7D module

[Soup Engine](https://github.com/rebelt2i/soup-engine) applies film looks
in camera. This repository ports its "Film-in-ML" variant to the 7D as a
regular Magic Lantern module, `soup.mo`.

## How it works in camera

1. With **Film = ON**, a background task watches the camera's file counter.
2. When a new `IMG_nnnn.CR2` has stopped growing, it reads the **full-size
   JPEG preview Canon embeds in the CR2** (not the sensor data).
3. It decodes it at half size, runs the look row by row, and writes
   `FLM_nnnn.JPG` (2592x1728 on the 7D, Canon's Small JPEG size).

Requirements: shoot RAW or RAW+JPEG, with a colour picture style (a
monochrome preview has no colour left to grade).

## Why a module and not a rebuilt autoexec

Upstream Film-in-ML rebuilds Magic Lantern with the Film page compiled in,
pinned to `magiclantern_simplified` `ed3e7c0d`, where the 7D does not
build. But on the 7D:

* every Magic Lantern function the integration uses exists in `lua_fix`
  and is **exported to modules** by the official `7D_203.sym`;
* upstream's only edits to ML's `menu.c` register a tab and call
  `film_menu_init()`; a module gets the same with `menu_add("Film", ...)`,
  and a tab without an icon is drawn with its first letter, "F".

So the official, community-tested `autoexec.bin` stays, and Soup is one
file in `ML/modules/` that can be disabled in the *Modules* tab or deleted.

## Changes from upstream

The engine (`engine/`) and the stocks (`stocks/`) are vendored unchanged.
The integration files under `module/soup/` were changed only where noted
in their headers:

* **"Rewrite CR2" row, OFF by default.** Upstream always rewrites the
  CR2's embedded preview so playback shows the look; here that write into
  the raw file is opt-in (`PREVIEW` line in `ML/SE.CFG`).
* **Progress text.** lua_fix has no `NotifyBoxAt`, and its `NotifyBox`
  passes the finished text to `bmp_printf` as a format string again, so
  the `%` sign is written `%%%%`.
* **The engine is compiled with `-fno-fast-math`.** ML builds with
  `-ffast-math`; under it GCC 5.4 turned repeated multiplies in `grade.c`
  into a call to `__powisf2`, which the 7D autoexec does not export, and
  ML's dependency check reported *"Will NOT load on: 7D"*. Without
  fast-math the module loads, and the camera keeps computing exactly what
  the desktop build computes.

## Build

```bash
./build.sh            # copies module + engine into ~/ml-lua_fix/modules/soup and builds
```

After building, the script checks every undefined symbol of `soup.mo`
against `7D_203.sym`.

## Tests

| Layer | Tool | Result (2026-10-30) |
|---|---|---|
| CR2 format | `tools/cr2info.py *.CR2` | 7D CR2: IFD0 preview 5184x3456, compression 6, JPEG header at about 66-70 KB, inside the 256 KB the module reads first |
| Engine on PC | `tools/pc_develop.sh` (Soup's `make test`, then `previewcheck` over the samples with 5 stocks) | self-tests pass; 25/25 develops at 2592x1728, 0.7-1.2 MB (output buffer is 2 MB) |
| ARM build | `./build.sh` | 0 errors; ML: *"Will load on: 7D"* |
| Camera | 7D, fw 2.0.3, ML lua_fix 2020Dec28 | module loads, F tab appears, about **55 s per photo**; the `FLM_*.JPG` compared are **byte-identical** to the PC output (4 of 4); the original CR2/JPG files are untouched |

`tools/pc_develop.sh` runs Soup Engine's own code, so it is meant to be
launched by hand.

## Known issues

* **Playback shows "no images" while `FLM_*.JPG` sit next to the photos.**
  Suspected cause: DCF file numbers must be unique within a folder, and
  `IMG_2822.JPG` + `FLM_2822.JPG` share one. Removing the `FLM` files
  is the workaround. Planned fix: write developed files to their own DCF
  folder. Not done yet.
* Turning Film on starts developing straight away with the first stock in
  the list, before a stock has been picked.
* 55 s per photo; Canon's hardware JPEG codec could make this much faster,
  which is unexplored.
