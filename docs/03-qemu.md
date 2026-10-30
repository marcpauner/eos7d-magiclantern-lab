# 3. Emulating the 7D with qemu-eos

## Build

```bash
git clone --branch qemu-eos-v4.2.1 --single-branch https://github.com/reticulatedpines/qemu-eos ~/qemu-eos
cd ~/qemu-eos && git apply ~/eos7d-magiclantern-lab/patches/qemu-eos-7d-rom0.patch
mkdir ~/qemu-eos-build && cd ~/qemu-eos-build
../qemu-eos/configure --target-list=arm-softmmu --enable-plugins --disable-docs \
    --enable-vnc --enable-gtk --disable-vte --disable-werror
make -j"$(nproc)" && make plugins && cp tests/plugin/libmagiclantern.so arm-softmmu/plugins/
```

`--disable-werror`: QEMU 4.2 predates GCC 11, whose new warnings would
otherwise stop the build.

Put the ROM dumps of **your own** camera in `~/qemu-eos/roms/7D/` and pass
`-r ~/qemu-eos/roms` to `run_qemu.py` (its default is `~/roms`).

## The ROM0 patch

The stock 7D model maps no ROM0. Anything that reads `0xF0000000` (the
Rescue tool on qemu-eos's test card, and Magic Lantern's own ROM backup)
hits an unmapped address and QEMU aborts with
`io_readx: Assertion '0' failed`. On the real 7D that range is readable
(the dump is 16 MB, mostly zeros), so the patch maps 16 MB there.

## What works, and where it stops

| Step | Result |
|---|---|
| Canon bootloader loads `AUTOEXEC.BIN` from the virtual CF | yes |
| Magic Lantern Rescue | runs, reports *Model ID 0x250, Canon EOS 7D* |
| ML boot hack (`copy_and_restart`, cache patch) | yes |
| Canon firmware start | `K250S ICU Firmware Version 2.0.3` |
| Slave waits for the master CPU | `ERROR Wait Master Wakeup Timeout` |
| Canon GUI, ML menus, modules | **no** |

The master DIGIC is not emulated, so the slave's startup never brings the
GUI up. Magic Lantern then waits forever in `src/boot-hack.c`:

```c
// wait for firmware to initialize
while (!bmp_vram_raw()) msleep(100);
```

and `ml_init` (menus, module loading) never starts. **On the 7D, QEMU is
only an early-boot smoke test**: it catches a build that breaks loading or
the boot hack, nothing after that.

## Tools

* [`tools/qemu/boot_ml.sh`](../tools/qemu/boot_ml.sh) `ML_ZIP [SECONDS] [key@time ...]`
  builds a virtual CF card from an ML zip and boots it headless, taking a
  screenshot every 5 s.
* [`tools/qemu/qemu_shots.py`](../tools/qemu/qemu_shots.py) is the runner
  behind it (screenshots and key presses through the QEMU monitor).

Building ML with `make zip SHELL=/bin/bash CONFIG_QEMU=y` makes it print its
own messages into the QEMU log. **Such a build hangs a real camera**: keep
it in a separate clone and never put it on the card.

Useful keys (qemu `sendkey` names): `delete` Trash (ML menu),
`bracket_left`/`bracket_right` main dial, `pgup`/`pgdn` rear dial,
`spc` SET, `m` MENU, `p` PLAY.
