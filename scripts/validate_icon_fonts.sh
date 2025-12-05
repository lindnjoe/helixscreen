#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2025 356C LLC
#
# Validate that all icons in ui_icon_codepoints.h are included in compiled fonts
#
# This script prevents the common bug where icons are added to the codepoints
# header but the font files aren't regenerated, causing missing glyphs at runtime.
#
# Usage:
#   ./scripts/validate_icon_fonts.sh         # Check if fonts are up to date
#   ./scripts/validate_icon_fonts.sh --fix   # Regenerate fonts if needed
#
# Exit codes:
#   0 - All icons present in fonts
#   1 - Missing icons (fonts need regeneration)
#   2 - Script error

set -e
cd "$(dirname "$0")/.."

CODEPOINTS_FILE="include/ui_icon_codepoints.h"
FONT_FILE="assets/fonts/mdi_icons_32.c"  # Check one size, all have same glyphs

# Parse command line
FIX_MODE=false
if [[ "$1" == "--fix" ]]; then
    FIX_MODE=true
fi

# Extract codepoints from ui_icon_codepoints.h
# Format: {"name", "\xF3\xB0\x80\xA6"},  // F0026 alert
# We extract the hex codepoint from the comment (e.g., F0026)
extract_required_codepoints() {
    # Match lines with icon definitions and extract the codepoint from comment
    grep -E '\{"[^"]+",\s*"\\x[^"]+"\}.*//\s*[Ff][0-9A-Fa-f]+' "$CODEPOINTS_FILE" |
    grep -oE '//\s*[Ff][0-9A-Fa-f]+' |
    sed 's|// *||' |
    tr '[:lower:]' '[:upper:]' |
    sort -u
}

# Extract codepoints from compiled font file
# Format: --range 0xF0026,0xF0029,...
extract_font_codepoints() {
    if [[ ! -f "$FONT_FILE" ]]; then
        echo "ERROR: Font file not found: $FONT_FILE" >&2
        echo "Run './scripts/regen_mdi_fonts.sh' to generate fonts" >&2
        exit 2
    fi

    head -5 "$FONT_FILE" |
    grep -oE '0x[Ff][0-9A-Fa-f]+' |
    sed 's/0x//' |
    tr '[:lower:]' '[:upper:]' |
    sort -u
}

echo "Validating icon fonts..."
echo "  Codepoints file: $CODEPOINTS_FILE"
echo "  Font file: $FONT_FILE"
echo ""

# Get codepoints from both sources
REQUIRED=$(extract_required_codepoints)
IN_FONT=$(extract_font_codepoints)

# Find missing codepoints
MISSING=""
MISSING_COUNT=0
while IFS= read -r codepoint; do
    if ! echo "$IN_FONT" | grep -q "^${codepoint}$"; then
        MISSING="${MISSING}${codepoint}\n"
        MISSING_COUNT=$((MISSING_COUNT + 1))
    fi
done <<< "$REQUIRED"

# Report results
REQUIRED_COUNT=$(echo "$REQUIRED" | wc -l | tr -d ' ')
IN_FONT_COUNT=$(echo "$IN_FONT" | wc -l | tr -d ' ')

echo "  Required icons: $REQUIRED_COUNT"
echo "  Icons in font: $IN_FONT_COUNT"
echo ""

if [[ $MISSING_COUNT -gt 0 ]]; then
    echo "❌ MISSING ICONS ($MISSING_COUNT):"
    echo ""
    echo -e "$MISSING" | while IFS= read -r cp; do
        if [[ -n "$cp" ]]; then
            # Find the icon name for this codepoint
            NAME=$(grep -E "//\s*$cp" "$CODEPOINTS_FILE" | head -1 | sed -E 's/.*\{"([^"]+)".*/\1/')
            echo "   0x$cp ($NAME)"
        fi
    done
    echo ""

    if $FIX_MODE; then
        echo "Regenerating fonts..."
        ./scripts/regen_mdi_fonts.sh
        echo ""
        echo "✓ Fonts regenerated. Re-run validation to confirm."
        exit 0
    else
        echo "Run './scripts/validate_icon_fonts.sh --fix' to regenerate fonts"
        echo "Or run './scripts/regen_mdi_fonts.sh' directly"
        exit 1
    fi
else
    echo "✓ All required icons are present in compiled fonts"
    exit 0
fi
