#!/bin/bash
# PC test layer: Soup's own self-tests, then the camera's develop path
# (previewcheck, the desktop twin of film_develop) over the 7D samples.
# Runs Soup Engine code from the downloaded zip, so Marc launches it himself.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
S=~/soup-engine/multicam/soup-engine-master/src
OUT=$REPO/samples/out
STOCKS="soup800 portra velvia acros classicchrome"
FILM_JPEG=$((2 * 1024 * 1024))   # output buffer in film_develop.c

cd "$S"
echo "### Soup self-tests"
make -s -C host clean
make test
make -s out/previewcheck

mkdir -p "$OUT"
echo
echo "### develop 7D samples (scale 2, quality 88, like the camera)"
for f in $REPO/samples/*.CR2; do
    b=$(basename "$f" .CR2)
    for st in $STOCKS; do
        o="$OUT/${b}_$st.jpg"
        line=$(./out/previewcheck "$f" "stocks/$st.fs" 2 "$o" | grep -E 'look hash|->' | tr '\n' ' ')
        sz=$(stat -c %s "$o")
        flag=$([ "$sz" -lt "$FILM_JPEG" ] && echo ok || echo "TOO BIG for camera buffer")
        printf '%s %-14s %8d bytes  %s  %s\n' "$b" "$st" "$sz" "$flag" "$(echo "$line" | grep -oE 'look hash [0-9A-F]+|[0-9]+x[0-9]+' | tr '\n' ' ')"
    done
done
echo
echo "outputs in $OUT"
