# Notice

This repository is distributed under the GNU General Public License,
version 2 (see `LICENSE`). It contains work from these projects:

| Path | Origin | Changes |
|---|---|---|
| `engine/` | Soup Engine look engine (`src/engine`), snapshot of the multi-camera fork [rebelt2i/soup-engine](https://github.com/rebelt2i/soup-engine), itself based on [jiijii0/soup-engine](https://github.com/jiijii0/soup-engine). GPL v2. | none |
| `stocks/` | Soup Engine film stocks (`card/ML/FILM`), same snapshot. | none |
| `module/soup/film_menu.c`, `module/soup/film_develop.c` | Soup Engine Film-in-ML integration (`integrations/magiclantern/_common/src`), same snapshot. Copyright (C) 2025 Soup Engine contributors. | listed in each file's header and in `docs/04-soup-module.md` |
| `ui/fonts/SpaceGrotesk.ttf` | [Space Grotesk](https://github.com/floriankarsten/space-grotesk), from the Google Fonts repository. SIL Open Font License 1.1 (`ui/fonts/OFL-SpaceGrotesk.txt`). | none; rasterised into `ui/src/lab_assets.c` |
| `ui/fonts/JetBrainsMono.ttf` | [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono), from the Google Fonts repository. SIL Open Font License 1.1 (`ui/fonts/OFL-JetBrainsMono.txt`). | none; rasterised into `ui/src/lab_assets.c` |
| `module/soup/soup.c`, `soup_cam.h`, `Makefile`, `README.rst`, `build.sh`, `tools/`, `patches/`, `docs/`, `ui/` (except the fonts) | written for this repository | — |

The module is built against [Magic Lantern](https://www.magiclantern.fm/)
(GPL), Mercurial branch `lua_fix`, and is tested with
[qemu-eos](https://github.com/reticulatedpines/qemu-eos) (GPL). Neither is
included here; `patches/` holds the small changes applied to them.

The Soup Engine snapshot was the `master` branch zip of the multi-camera
fork, SHA-256 starting `8aec2c1f56a8cee1`.
