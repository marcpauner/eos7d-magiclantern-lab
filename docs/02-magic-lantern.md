# 2. Building Magic Lantern for the 7D

## Which source tree

The original EOS 7D (two DIGIC 4, Magic Lantern runs on the *slave*) is a
legacy port:

* In the current Git repository
  ([magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified))
  `platform/7D.203` exists but is **not** in `ML_CAMS`, and the build is
  known to be broken
  ([issue #250](https://github.com/reticulatedpines/magiclantern_simplified/issues/250)).
* The last official 7D build, `magiclantern-lua_fix.2020Dec28.7D203.zip`,
  comes from the historical Mercurial repository, branch `lua_fix`,
  changeset **`7e5067c0a8cf`**.

```bash
hg clone -u lua_fix https://foss.heptapod.net/magic-lantern/magic-lantern ~/ml-lua_fix
```

The clone downloaded everything (about 50 MB) and then sat idle waiting on
the server. If that happens, stop it and update offline:
`cd ~/ml-lua_fix && hg verify && hg update lua_fix`.

## Three fixes for a 2026 toolchain

```bash
cd ~/ml-lua_fix/platform/7D.203 && make zip -j8 SHELL=/bin/bash
```

| Symptom | Cause | Fix |
|---|---|---|
| `/bin/sh: Bad substitution` | Ubuntu's `/bin/sh` is `dash`; the makefiles use bash syntax | `SHELL=/bin/bash` on the make command line |
| `${f\#\#...}: bad substitution` | GNU Make 4.3 no longer strips `\` before `#` inside functions | [`patches/ml-lua_fix-make-4.3.patch`](../patches/ml-lua_fix-make-4.3.patch) |
| `python: not found` while making `module_strings.h` | a module script calls `python` | `~/.local/bin/python -> python2` (see [01](01-environment.md)) |

## Is the build trustworthy?

The rebuilt zip was compared file by file with the official
`lua_fix.2020Dec28.7D203` zip:

* the official build was made with the **same compiler**
  (`GCC 5.4.1 20160919`, read from the modules' `.comment` section);
* all **19 modules have byte-identical** `.text`, `.rodata` and `.data`;
  their only differences are the embedded build user and date;
* `ML-SETUP.FIR` is identical (it is a prebuilt binary in the tree);
* `autoexec.bin` differs by 448 bytes, which looks like the embedded build
  information (not investigated further).

So a failure in our own code can be blamed on our code, not on the toolchain.

## Installing on the camera (done by hand, at your own risk)

* ML for the 7D needs Canon firmware **2.0.3** exactly. A camera on 2.0.6
  can be taken back to 2.0.3 from Canon's own firmware-update menu with the
  2.0.3 `.FIR` (keep 2.0.6 around to go back).
* Copy the zip contents to the card and run *Firmware Ver.* again: that
  starts `ML-SETUP.FIR`, which marks the camera bootable.
* The installer and every boot back up the ROM to `ML/LOGS/ROM0.BIN` and
  `ROM1.BIN` (`rom_backup()` in `src/installer.c`, `CONFIG_AUTOBACKUP_ROM`
  in `src/config-defines.h`). Those dumps are what QEMU needs.
* Hold **SET** while powering on to boot without ML (the firmware version
  then shows `-ml-off`).
* The ML menu opens with the **Trash** button.

Canon firmware files and ROM dumps are Canon's copyrighted code: they are
not, and must not be, part of this repository.
