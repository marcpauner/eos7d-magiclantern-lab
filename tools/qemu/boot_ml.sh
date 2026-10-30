#!/bin/bash
# Boot a Magic Lantern zip on the emulated 7D, headless, with screenshots.
#
#   tools/qemu/boot_ml.sh ML_ZIP [SECONDS] [key@time ...]
#
# Builds a virtual CF card from qemu-eos's bootable test image: removes its
# rescue AUTOEXEC.BIN, adds the zip's autoexec.bin + ML/, and the ROM backups
# in ML/LOGS (as on the real card, so ML skips its ROM backup). Output goes to
# $OUT (default ~/qemu-tests/boot_ml): qemu.stdout/.stderr and tNNN.png.
# Build ML with CONFIG_QEMU=y to see its own messages in the log; such a
# build must never go to the real camera.
set -e
ZIP=${1:?usage: boot_ml.sh ML_ZIP [SECONDS] [key@time ...]}
SECS=${2:-60}
shift $(( $# > 1 ? 2 : 1 ))
HERE=$(cd "$(dirname "$0")" && pwd)
QEMU_EOS=${QEMU_EOS:-~/qemu-eos}
QEMU_BUILD=${QEMU_BUILD:-~/qemu-eos-build}
ROM_DIR=${ROM_DIR:-$QEMU_EOS/roms}
OUT=${OUT:-~/qemu-tests/boot_ml}
QI=$QEMU_BUILD/qemu-img

rm -rf "$OUT"; mkdir -p "$OUT/zip"
unzip -q "$ZIP" -d "$OUT/zip"
$QI convert -O raw "$QEMU_EOS/magiclantern/disk_images/cf.qcow2" "$OUT/cf.raw"
# FAT16 partition of the test image starts at sector 99
IMG="$OUT/cf.raw@@$((99 * 512))"
mdel -i "$IMG" ::/AUTOEXEC.BIN
mcopy -i "$IMG" "$OUT/zip/autoexec.bin" ::/
mcopy -i "$IMG" -s "$OUT/zip/ML" ::/
mmd -i "$IMG" -D s ::/ML/LOGS 2>/dev/null || true
mcopy -i "$IMG" "$ROM_DIR/7D/ROM0.BIN" "$ROM_DIR/7D/ROM1.BIN" ::/ML/LOGS/
$QI convert -O qcow2 "$OUT/cf.raw" "$OUT/cf.qcow2" && rm "$OUT/cf.raw"
cp "$QEMU_EOS/magiclantern/disk_images/sd.qcow2" "$OUT/"

timeout $((SECS + 60)) python3 "$HERE/qemu_shots.py" "$OUT" "$OUT/cf.qcow2" "$OUT/sd.qcow2" 1 "$SECS" "$@"
pkill -x qemu-system-arm || true
echo "log and screenshots in $OUT"
