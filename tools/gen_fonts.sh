#!/usr/bin/env bash
# Erzeugt die UI-Fonts mit Latin-1 (Umlaute, ß, °) aus dem Montserrat, das
# LVGL selbst mitbringt. Die eingebauten lv_font_montserrat_* koennen keine
# Umlaute -- deshalb eigene. Braucht Node (npx lv_font_conv).
set -euo pipefail
cd "$(dirname "$0")/.."

TTF=.pio/libdeps/wt32-sc01-plus/lvgl/scripts/generators/built_in_font/Montserrat-Medium.ttf
[ -f "$TTF" ] || { echo "TTF fehlt -- erst 'pio run', damit lvgl liegt"; exit 1; }

# ASCII, Latin-1, Gedankenstriche, Anfuehrungszeichen, Punkt, Ellipse
RANGE='0x20-0x7F,0xA0-0xFF,0x2013-0x2014,0x2018-0x201E,0x2022,0x2026'

gen() {  # groesse range
    local size=$1 range=$2 out=src/fonts/font_ms_$1.c
    npx --yes lv_font_conv@1.5.2 --font "$TTF" --size "$size" --bpp 4 --format lvgl \
        --lv-include lvgl.h --no-compress --range "$range" \
        -o "$out"
    echo "$out"
}

gen 16 "$RANGE"
gen 20 "$RANGE"
gen 24 "$RANGE"
gen 28 "$RANGE"
gen 48 "$RANGE"
gen 96 '0x20,0x2D,0x30-0x3A'     # nur Ziffern, Doppelpunkt, Minus: die grosse Restzeit
