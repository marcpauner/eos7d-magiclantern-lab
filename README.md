# EOS 7D Magic Lantern lab

Bringing new software to a **Canon EOS 7D (2009)** through
[Magic Lantern](https://www.magiclantern.fm/): a reproducible build
environment for the legacy 7D port, emulation with qemu-eos, and a port of
the [Soup Engine](https://github.com/rebelt2i/soup-engine) film looks as a
Magic Lantern module.

> Not affiliated with Canon or the Magic Lantern project. Running unofficial
> software on a camera is at your own risk.

## Status

| Part | State |
|---|---|
| Build environment (WSL2, GCC 5.4) | done: rebuilds the official `lua_fix.2020Dec28.7D203` with byte-identical modules |
| Magic Lantern on the camera | official `lua_fix` build installed, Canon firmware 2.0.3 |
| QEMU (qemu-eos) | boots the 7D slave and ML's boot hack, stops before the GUI (master CPU not emulated) |
| `soup.mo` film looks | runs on the camera, output byte-identical to the desktop build; playback issue open |

## What's here

```
docs/            how everything was built and what was found (start here)
module/soup/     the Soup Engine integration, as a Magic Lantern module
engine/          Soup Engine look engine, vendored unchanged
stocks/          film stocks (ML/FILM on the card)
patches/         fixes for Magic Lantern lua_fix and qemu-eos
tools/           CR2 checks, PC develop test, contact sheets, QEMU runner
build.sh         builds soup.mo and checks it can load on the 7D
```

1. [Build environment](docs/01-environment.md)
2. [Building Magic Lantern for the 7D](docs/02-magic-lantern.md)
3. [Emulating the 7D with qemu-eos](docs/03-qemu.md)
4. [Soup Engine as a 7D module](docs/04-soup-module.md)

## Quick start

```bash
# toolchain and packages: docs/01-environment.md
hg clone -u lua_fix https://foss.heptapod.net/magic-lantern/magic-lantern ~/ml-lua_fix
cd ~/ml-lua_fix && patch -p1 < ~/eos7d-magiclantern-lab/patches/ml-lua_fix-make-4.3.patch
cd platform/7D.203 && make zip -j8 SHELL=/bin/bash   # stock ML for the 7D
~/eos7d-magiclantern-lab/build.sh                    # soup.mo
```

To try the module: copy `soup.mo` to `ML/modules/` and `stocks/*` to
`ML/FILM/` on a card that already runs ML lua_fix, enable *soup* in the
*Modules* tab, restart, and use the **F** tab.

## Not in this repository

Canon firmware (`.FIR`), ROM dumps and photos. Firmware and ROM contents are
Canon's copyrighted code; dump your own camera's ROM if you need it for QEMU.

## Credits and license

GPL v2, like Magic Lantern and Soup Engine. See [NOTICE.md](NOTICE.md) for
where each part comes from.

---

## Resumen en español

Proyecto para darle software nuevo a una **Canon EOS 7D (2009)** con Magic
Lantern:

* **Entorno de compilación reproducible** para el port antiguo de la 7D
  (WSL2 + GCC 5.4). Reproduce la build oficial de 2020 con módulos idénticos
  byte a byte.
* **Emulación con qemu-eos**: arranca el procesador esclavo y el arranque de
  ML, pero se detiene antes de la interfaz porque no se emula el procesador
  maestro.
* **Soup Engine como módulo de ML** (`soup.mo`): simulaciones de película
  dentro de la cámara. Funciona en la 7D real y su resultado es idéntico al
  del PC; queda abierto un problema con la reproducción.

La documentación detallada está en `docs/` (en inglés).
