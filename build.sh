#!/bin/bash
# Build soup.mo inside the ML lua_fix tree and check it can load on the 7D.
#   ./build.sh          build
#   ./build.sh clean    clean the module dir inside the ML tree first
set -e
P=$(cd "$(dirname "$0")" && pwd)
ML=~/ml-lua_fix
M=$ML/modules/soup
SYM=$ML/platform/7D.203/zip/ML/modules/7D_203.sym
[ -f "$SYM" ] || SYM=~/ml-official/lua_fix/ML/modules/7D_203.sym

mkdir -p "$M/engine" "$P/build"
cp "$P"/module/soup/* "$M"/
cp "$P"/engine/*.c "$P"/engine/*.h "$M"/engine/
[ "$1" = clean ] && make -C "$M" clean SHELL=/bin/bash >/dev/null

make -C "$M" all SHELL=/bin/bash 2>&1 | grep -v -E '^make\[[0-9]+\]: (Entering|Leaving)' | tee "$P/build/build.log" | tail -25
[ -f "$M/soup.mo" ] || { echo "BUILD FAILED"; exit 1; }
cp "$M/soup.mo" "$P/build/"

NM=~/gcc-arm-none-eabi-5_4-2016q3/bin/arm-none-eabi-nm
echo "=== soup.mo: $(stat -c %s "$M/soup.mo") bytes"
missing=0
for s in $($NM -u "$M/soup.mo" | awk '{print $2}' | sort -u); do
    grep -q -E "[[:space:]]$s\$" "$SYM" || { echo "MISSING in 7D_203.sym: $s"; missing=1; }
done
echo "undefined symbols: $($NM -u "$M/soup.mo" | wc -l), all exported by the 7D autoexec: $([ $missing = 0 ] && echo yes || echo NO)"
echo "=== globals defined by soup.mo that the core also defines (clashes):"
for s in $($NM -g --defined-only "$M/soup.mo" | awk '{print $3}'); do
    grep -q -E "[[:space:]]$s\$" "$SYM" && echo "  CLASH: $s"
done
echo "(end of clash list)"
