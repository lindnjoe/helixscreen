#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Verify MDI icon codepoints against the official Pictogrammers metadata.

This script:
1. Parses regen_mdi_fonts.sh to extract codepoints and their claimed names
2. Loads MDI metadata from local cache (assets/mdi-icon-metadata.json.gz)
3. Compares each entry and reports mismatches

Exit codes:
  0 - All icons verified successfully
  1 - Verification errors found (mislabeled codepoints)
  2 - Cache file missing (run: make update-mdi-cache)
"""

import gzip
import json
import re
import sys
from pathlib import Path

# Cached MDI metadata (compressed)
MDI_CACHE_FILE = Path(__file__).parent.parent / "assets" / "mdi-icon-metadata.json.gz"


def fetch_mdi_metadata():
    """Load MDI metadata from local cache."""
    if not MDI_CACHE_FILE.exists():
        print(f"ERROR: MDI metadata cache not found at {MDI_CACHE_FILE}")
        print("   Run: make update-mdi-cache")
        sys.exit(2)  # Exit code 2 = missing cache

    print(f"Loading MDI metadata from cache...")
    try:
        with gzip.open(MDI_CACHE_FILE, 'rt', encoding='utf-8') as f:
            data = json.load(f)
        print(f"  Found {len(data)} icons in MDI library")
        return data
    except Exception as e:
        print(f"ERROR: Failed to load MDI metadata: {e}")
        sys.exit(2)


def build_codepoint_lookup(mdi_data):
    """Build a lookup table: codepoint (uppercase) -> icon name."""
    lookup = {}
    for icon in mdi_data:
        cp = icon.get('codepoint', '').upper()
        name = icon.get('name', '')
        if cp and name:
            lookup[cp] = name
    return lookup


def parse_regen_script(script_path):
    """Parse regen_mdi_fonts.sh to extract codepoints and claimed names."""
    entries = []

    # Pattern: MDI_ICONS+=",0xFxxxx"    # comment
    pattern = re.compile(r'MDI_ICONS\+?=.*"?,?(0x[A-Fa-f0-9]+)"?\s*#\s*(.+)')

    with open(script_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            match = pattern.search(line)
            if match:
                codepoint = match.group(1).upper().replace('0X', '')
                comment = match.group(2).strip()
                entries.append({
                    'line': line_num,
                    'codepoint': codepoint,
                    'comment': comment,
                    'raw': line.strip()
                })

    return entries


def extract_icon_name_from_comment(comment):
    """Extract the presumed icon name from the comment."""
    # Comments are like: "arrow-up-down (bidirectional vertical)"
    # or: "home" or: "check-circle-outline (check-circle)"

    # Take the first word/phrase before any parenthesis or dash description
    name = comment.split('(')[0].strip()
    name = name.split(' - ')[0].strip()  # Handle "close (xmark)" style

    # Normalize: comments use dashes, we want to match MDI names
    return name.lower().strip()


def main():
    script_dir = Path(__file__).parent
    regen_script = script_dir / "regen_mdi_fonts.sh"

    if not regen_script.exists():
        print(f"ERROR: Cannot find {regen_script}")
        sys.exit(1)

    # Fetch official MDI data
    mdi_data = fetch_mdi_metadata()
    cp_to_name = build_codepoint_lookup(mdi_data)

    # Also build reverse lookup for suggestions
    name_to_cp = {v: k for k, v in cp_to_name.items()}

    # Parse our script
    print(f"\nParsing {regen_script}...")
    entries = parse_regen_script(regen_script)
    print(f"  Found {len(entries)} icon entries\n")

    # Verify each entry
    errors = []
    warnings = []
    ok_count = 0

    for entry in entries:
        cp = entry['codepoint']
        comment = entry['comment']
        claimed_name = extract_icon_name_from_comment(comment)

        if cp not in cp_to_name:
            errors.append({
                'type': 'INVALID',
                'line': entry['line'],
                'codepoint': cp,
                'comment': comment,
                'message': f"Codepoint {cp} does not exist in MDI!"
            })
            continue

        actual_name = cp_to_name[cp]

        # Check if claimed name matches actual name
        # Allow some flexibility: claimed might be partial or use underscores
        claimed_normalized = claimed_name.replace('_', '-').replace(' ', '-')

        if claimed_normalized == actual_name:
            ok_count += 1
        elif actual_name.startswith(claimed_normalized) or claimed_normalized in actual_name:
            # Close enough (e.g., "wifi" matches "wifi-strength-1")
            ok_count += 1
        else:
            errors.append({
                'type': 'MISMATCH',
                'line': entry['line'],
                'codepoint': cp,
                'claimed': claimed_name,
                'actual': actual_name,
                'comment': comment
            })

    # Report results
    print("=" * 70)
    print("VERIFICATION RESULTS")
    print("=" * 70)
    print(f"\n✓ OK: {ok_count} icons verified correctly")

    if errors:
        print(f"\n✗ ERRORS: {len(errors)} icons have issues:\n")
        for err in errors:
            if err['type'] == 'INVALID':
                print(f"  Line {err['line']}: INVALID CODEPOINT")
                print(f"    Codepoint: 0x{err['codepoint']}")
                print(f"    Comment: {err['comment']}")
                print()
            else:
                print(f"  Line {err['line']}: NAME MISMATCH")
                print(f"    Codepoint: 0x{err['codepoint']}")
                print(f"    Comment says: {err['claimed']}")
                print(f"    Actually is:  {err['actual']}")
                # Check if the claimed name exists elsewhere
                if err['claimed'].replace('_', '-') in name_to_cp:
                    correct_cp = name_to_cp[err['claimed'].replace('_', '-')]
                    print(f"    → '{err['claimed']}' is at 0x{correct_cp}")
                print()
    else:
        print("\n✓ All icon names match their codepoints!")

    print("=" * 70)

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
