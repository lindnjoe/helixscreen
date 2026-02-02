#!/bin/bash
# Copyright (C) 2025-2026 356C LLC
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Regenerate Noto Sans text fonts for LVGL
#
# This script generates LVGL font files for UI text rendering.
# Includes extended Unicode ranges for internationalization:
# - Basic Latin (0x20-0x7F): ASCII
# - Latin-1 Supplement (0xA0-0xFF): Western European (é, ñ, ü, etc.)
# - Latin Extended-A (0x100-0x17F): Central European
# - Cyrillic (0x400-0x4FF): Russian, Ukrainian, etc.
# - Common symbols and punctuation
#
# Source: Google Noto Sans (assets/fonts/NotoSans-*.ttf)

set -e
cd "$(dirname "$0")/.."

# Add node_modules/.bin to PATH for lv_font_conv
export PATH="$PWD/node_modules/.bin:$PATH"

# Font source files
FONT_REGULAR=assets/fonts/NotoSans-Regular.ttf
FONT_LIGHT=assets/fonts/NotoSans-Light.ttf
FONT_BOLD=assets/fonts/NotoSans-Bold.ttf

# Check fonts exist
for FONT in "$FONT_REGULAR" "$FONT_LIGHT" "$FONT_BOLD"; do
    if [ ! -f "$FONT" ]; then
        echo "ERROR: Font not found: $FONT"
        echo "Download Noto Sans from: https://fonts.google.com/noto/specimen/Noto+Sans"
        exit 1
    fi
done

# Unicode ranges for internationalization
# Format: comma-separated ranges for lv_font_conv --range parameter
UNICODE_RANGES=""
UNICODE_RANGES+="0x20-0x7F"      # Basic Latin (ASCII)
UNICODE_RANGES+=",0xA0-0xFF"     # Latin-1 Supplement (Western European)
UNICODE_RANGES+=",0x100-0x17F"   # Latin Extended-A (Central European)
UNICODE_RANGES+=",0x400-0x4FF"   # Cyrillic (Russian, Ukrainian, etc.)
UNICODE_RANGES+=",0x2013-0x2014" # En/Em dashes
UNICODE_RANGES+=",0x2018-0x201D" # Smart quotes
UNICODE_RANGES+=",0x2022"        # Bullet
UNICODE_RANGES+=",0x2026"        # Ellipsis
UNICODE_RANGES+=",0x20AC"        # Euro sign
UNICODE_RANGES+=",0x2122"        # Trademark

# Font sizes
SIZES_REGULAR="10 12 14 16 18 20 24 26 28"
SIZES_LIGHT="10 12 14 16 18"
SIZES_BOLD="14 16 18 20 24 28"

echo "Regenerating Noto Sans text fonts for LVGL..."
echo "  Unicode ranges: Basic Latin, Latin-1, Latin Extended-A, Cyrillic"
echo ""

# Generate Regular weight
echo "Regular weight:"
for SIZE in $SIZES_REGULAR; do
    OUTPUT="assets/fonts/noto_sans_${SIZE}.c"
    echo "  Generating noto_sans_${SIZE} -> $OUTPUT"
    lv_font_conv \
        --font "$FONT_REGULAR" --size "$SIZE" --bpp 4 --format lvgl \
        --range "$UNICODE_RANGES" \
        --no-compress \
        -o "$OUTPUT"
done

# Generate Light weight
echo ""
echo "Light weight:"
for SIZE in $SIZES_LIGHT; do
    OUTPUT="assets/fonts/noto_sans_light_${SIZE}.c"
    echo "  Generating noto_sans_light_${SIZE} -> $OUTPUT"
    lv_font_conv \
        --font "$FONT_LIGHT" --size "$SIZE" --bpp 4 --format lvgl \
        --range "$UNICODE_RANGES" \
        --no-compress \
        -o "$OUTPUT"
done

# Generate Bold weight
echo ""
echo "Bold weight:"
for SIZE in $SIZES_BOLD; do
    OUTPUT="assets/fonts/noto_sans_bold_${SIZE}.c"
    echo "  Generating noto_sans_bold_${SIZE} -> $OUTPUT"
    lv_font_conv \
        --font "$FONT_BOLD" --size "$SIZE" --bpp 4 --format lvgl \
        --range "$UNICODE_RANGES" \
        --no-compress \
        -o "$OUTPUT"
done

echo ""
echo "Done! Generated text fonts with extended Unicode support."
echo ""
echo "Supported character sets:"
echo "  - ASCII (0x20-0x7F)"
echo "  - Western European: é, è, ê, ñ, ü, ö, ß, etc."
echo "  - Central European: ą, ę, ł, ő, etc."
echo "  - Cyrillic: А-Яа-я (Russian, Ukrainian, etc.)"
echo ""
echo "Rebuild the project: make -j"
