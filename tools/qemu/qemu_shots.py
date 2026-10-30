#!/usr/bin/env python3
"""Headless run of the 7D in qemu-eos: boot, take screenshots, optionally press keys.
usage: qemu_shots.py OUTDIR CF_IMAGE SD_IMAGE BOOT(0/1) SECONDS [key@time ...]

Paths default to the layout in docs/03-qemu.md and can be overridden with
QEMU_EOS (source tree), QEMU_BUILD, ROM_DIR (holds 7D/ROM0.BIN, 7D/ROM1.BIN)
and ML_SRC (Magic Lantern tree, used for stubs).
Keys are qemu sendkey names, e.g. delete@30 presses DELETE (ML menu) at 30 s.
"""
import os, sys, time
home = os.path.expanduser("~")
QEMU_EOS = os.environ.get("QEMU_EOS", f"{home}/qemu-eos")
QEMU_BUILD = os.environ.get("QEMU_BUILD", f"{home}/qemu-eos-build")
ROM_DIR = os.environ.get("ROM_DIR", f"{QEMU_EOS}/roms")
ML_SRC = os.environ.get("ML_SRC", f"{home}/ml-lua_fix")
sys.path.insert(0, f"{QEMU_EOS}/magiclantern")
from ml_qemu.run import QemuRunner

out, cf, sd, boot, secs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] == "1", int(sys.argv[5])
keys = sorted((float(k.split("@")[1]), k.split("@")[0]) for k in sys.argv[6:])
os.makedirs(out, exist_ok=True)
os.chdir(out)
with QemuRunner(QEMU_BUILD, ROM_DIR, ML_SRC, "7D",
                sd_file=sd, cf_file=cf, stdout="qemu.stdout", stderr="qemu.stderr",
                display=None, boot=boot, d_args=["debugmsg"]) as q:
    t0 = time.time() - 5.5          # runner already slept 5.5 s waiting for qemu
    shot_every = 5
    next_shot = shot_every
    while True:
        now = time.time() - t0
        if q.qemu_process.poll() is not None:
            print(f"qemu exited early at {now:.0f}s, code {q.qemu_process.returncode}")
            break
        while keys and keys[0][0] <= now:
            print(f"[{now:5.1f}s] key {keys[0][1]}"); q.key_press(keys.pop(0)[1])
        if now >= next_shot:
            name = q.capture_screen(); q.screen_cap_counter += 1
            os.rename(name, f"t{int(next_shot):03d}.png"); next_shot += shot_every
        if now >= secs:
            break
        time.sleep(0.2)
    q.shutdown(force=True)
print("screenshots:", sorted(f for f in os.listdir(".") if f.endswith(".png")))
