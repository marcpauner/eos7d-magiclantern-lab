#!/bin/bash
# Build labui.mo inside the ML lua_fix tree and check it can load on the 7D.
#   ui/build.sh          build
#   ui/build.sh clean    clean the module dir inside the ML tree first
set -e
UI=$(cd "$(dirname "$0")" && pwd)
ML=~/ml-lua_fix
M=$ML/modules/labui
SYM=~/ml-official/lua_fix/ML/modules/7D_203.sym
NM=~/gcc-arm-none-eabi-5_4-2016q3/bin/arm-none-eabi-nm

mkdir -p "$M" "$UI/build"
cp "$UI"/module/labui/* "$M"/
cp "$UI"/src/lab_*.c "$UI"/src/lab_*.h "$M"/
[ "$1" = clean ] && make -C "$M" clean SHELL=/bin/bash >/dev/null

make -C "$M" all SHELL=/bin/bash 2>&1 | grep -v -E '^make\[[0-9]+\]: (Entering|Leaving)' > "$UI/build/build.log" || true
grep -E 'warning|error|Will|load on|Not checked' "$UI/build/build.log" | grep -v 'Not checked' | head -30
[ -f "$M/labui.mo" ] || { echo "BUILD FAILED"; tail -30 "$UI/build/build.log"; exit 1; }
cp "$M/labui.mo" "$UI/build/"

echo "=== labui.mo: $(stat -c %s "$M/labui.mo") bytes"
missing=0
for s in $($NM -u "$M/labui.mo" | awk '{print $2}' | sort -u); do
    grep -q -E "[[:space:]]$s\$" "$SYM" || { echo "MISSING in 7D_203.sym: $s"; missing=1; }
done
echo "undefined symbols: $($NM -u "$M/labui.mo" | wc -l), all exported by the 7D autoexec: $([ $missing = 0 ] && echo yes || echo NO)"
clash=0
for s in $($NM -g --defined-only "$M/labui.mo" | awk '{print $3}'); do
    grep -q -E "[[:space:]]$s\$" "$SYM" && { echo "CLASH with core: $s"; clash=1; }
done
echo "name clashes with core: $([ $clash = 0 ] && echo none || echo YES)"
