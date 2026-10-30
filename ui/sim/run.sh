#!/bin/bash
# Build the UI for the PC (with AddressSanitizer and UBSan) and render the
# screens to PNG: ui/sim/out/*.png (720x480 bitmap) and *_lcd.png (as the
# 4:3 LCD shows it, 640x480).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/../src
OUT=$HERE/out
mkdir -p "$OUT"
cc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
   -I"$SRC" -o "$OUT/sim" "$HERE/sim.c" "$SRC/lab_ui.c" "$SRC/lab_gfx.c" "$SRC/lab_assets.c"
"$OUT/sim" "$OUT"
python3 - "$OUT" <<'PY'
import glob, os, sys
from PIL import Image
out = sys.argv[1]
for p in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    im = Image.open(p)
    base = p[:-4]
    im.save(base + ".png")
    im.resize((640, 480), Image.LANCZOS).save(base + "_lcd.png")
    os.remove(p)
print("png:", len(glob.glob(os.path.join(out, "*_lcd.png"))), "screens")
PY
