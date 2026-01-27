# Standard Macros + Print Start Integration Shipping Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Ship a complete integration of StandardMacros and Print Start detection systems with ForgeX/AD5M Pro as reference implementation.

**Architecture:** Sync docs to match implementation, update detection patterns to find ForgeX macros (PURGE_FILAMENT, CLEAR_NOZZLE), add ForgeX-specific heuristics to printer database for mod detection.

**Tech Stack:** C++, Klipper G-code macros, JSON configuration, Moonraker API

---

## Background Context

**AD5M Pro at 192.168.1.67 runs ForgeX mod.** Key macros discovered:
- `LOAD_FILAMENT`, `UNLOAD_FILAMENT`, `PURGE_FILAMENT`
- `PAUSE`, `RESUME`, `CANCEL_PRINT`
- `CLEAR_NOZZLE` (not CLEAN_NOZZLE)
- `START_PRINT` with `preparation_done` variable
- `_START_PRINT` with `print_started` variable
- `SUPPORT_FORGE_X` - definitive ForgeX signature
- `mod_params` object - ForgeX settings container

**ForgeX START_PRINT params:** `SKIP_LEVELING`, `FORCE_LEVELING`, `FORCE_KAMP`, `SKIP_ZOFFSET`

**Other AD5M mods exist** (KlipperMod, etc.) - we add ForgeX-specific heuristics to distinguish.

---

## Task 1: Update STANDARD_MACROS_SPEC.md

**Files:**
- Modify: `docs/STANDARD_MACROS_SPEC.md`

**Step 1: Read current spec**
```bash
cat docs/STANDARD_MACROS_SPEC.md | head -60
```
Expected: See spec with 9 slots, single `bed_level`

**Step 2: Update slot table to match implementation (10 slots)**

Change the Standard Macro Slots table from 9 to 10 slots:

```markdown
| Slot | Purpose | Auto-Detect Patterns | HELIX Fallback |
|------|---------|---------------------|----------------|
| `load_filament` | Load filament | LOAD_FILAMENT, M701 | — |
| `unload_filament` | Unload filament | UNLOAD_FILAMENT, M702 | — |
| `purge` | Purge/prime | PURGE, PURGE_LINE, PRIME_LINE, PURGE_FILAMENT, LINE_PURGE | — |
| `pause` | Pause print | PAUSE, M601 | — |
| `resume` | Resume print | RESUME, M602 | — |
| `cancel` | Cancel print | CANCEL_PRINT | — |
| `bed_mesh` | Bed mesh calibration | BED_MESH_CALIBRATE, G29 | HELIX_BED_MESH_IF_NEEDED |
| `bed_level` | Physical bed leveling | QUAD_GANTRY_LEVEL, QGL, Z_TILT_ADJUST | — |
| `clean_nozzle` | Nozzle cleaning | CLEAN_NOZZLE, NOZZLE_WIPE, WIPE_NOZZLE, CLEAR_NOZZLE | HELIX_CLEAN_NOZZLE |
| `heat_soak` | Chamber/bed soak | HEAT_SOAK, CHAMBER_SOAK, SOAK | — |
```

**Step 3: Update StandardMacroSlot enum in spec**

```cpp
enum class StandardMacroSlot {
    LoadFilament, UnloadFilament, Purge,
    Pause, Resume, Cancel,
    BedMesh, BedLevel, CleanNozzle, HeatSoak
};
```

**Step 4: Verify changes**
```bash
grep -A 15 "Standard Macro Slots" docs/STANDARD_MACROS_SPEC.md
```
Expected: See updated 10-slot table

**Step 5: Commit**
```bash
git add docs/STANDARD_MACROS_SPEC.md
git commit -m "docs(macros): sync STANDARD_MACROS_SPEC with 10-slot implementation"
```

---

## Task 2: Update PRINT_START_INTEGRATION.md

**Files:**
- Modify: `docs/PRINT_START_INTEGRATION.md`

**Step 1: Read current macro variables section**
```bash
grep -A 10 "Macro Variables" docs/PRINT_START_INTEGRATION.md
```
Expected: See `_HELIX_STATE.print_started` and `_START_PRINT.print_started`

**Step 2: Add START_PRINT.preparation_done to Priority 1 signals**

In the "Macro Variables (Priority 1)" section, add:
```markdown
- `gcode_macro START_PRINT` → watches `preparation_done` variable
```

And update the explanation:
```markdown
When any of these become `True`, detection is instant. ForgeX firmware uses
`START_PRINT.preparation_done` and `_START_PRINT.print_started` for this purpose.
```

**Step 3: Add ForgeX note to troubleshooting**

Add to "Quick Detection Not Working" section:
```markdown
4. **ForgeX users**: Detection should be instant via `START_PRINT.preparation_done` signal
```

**Step 4: Commit**
```bash
git add docs/PRINT_START_INTEGRATION.md
git commit -m "docs(print-start): add START_PRINT.preparation_done and ForgeX notes"
```

---

## Task 3: Rename HELIX_BED_LEVEL_IF_NEEDED → HELIX_BED_MESH_IF_NEEDED

**Files:**
- Modify: `config/helix_macros.cfg`

**Step 1: Find current macro**
```bash
grep -n "HELIX_BED_LEVEL_IF_NEEDED" config/helix_macros.cfg
```
Expected: Line ~122 with `[gcode_macro HELIX_BED_LEVEL_IF_NEEDED]`

**Step 2: Rename macro definition**

Change:
```ini
[gcode_macro HELIX_BED_LEVEL_IF_NEEDED]
```
To:
```ini
[gcode_macro HELIX_BED_MESH_IF_NEEDED]
```

**Step 3: Update internal reference**

Change:
```jinja
{% set mesh_age = current_time - printer["gcode_macro HELIX_BED_LEVEL_IF_NEEDED"].last_mesh_time %}
```
To:
```jinja
{% set mesh_age = current_time - printer["gcode_macro HELIX_BED_MESH_IF_NEEDED"].last_mesh_time %}
```

And change:
```jinja
SET_GCODE_VARIABLE MACRO=HELIX_BED_LEVEL_IF_NEEDED VARIABLE=last_mesh_time VALUE={current_time}
```
To:
```jinja
SET_GCODE_VARIABLE MACRO=HELIX_BED_MESH_IF_NEEDED VARIABLE=last_mesh_time VALUE={current_time}
```

**Step 4: Verify no remaining references**
```bash
grep -n "BED_LEVEL_IF_NEEDED" config/helix_macros.cfg
```
Expected: No matches

**Step 5: Commit**
```bash
git add config/helix_macros.cfg
git commit -m "refactor(macros): rename HELIX_BED_LEVEL_IF_NEEDED to HELIX_BED_MESH_IF_NEEDED

The macro runs BED_MESH_CALIBRATE, not physical leveling (QGL/Z-tilt).
Name now matches what it actually does."
```

---

## Task 4: Update StandardMacros Detection Patterns

**Files:**
- Modify: `src/printer/standard_macros.cpp`

**Step 1: Find current patterns**
```bash
grep -A 2 "StandardMacroSlot::Purge" src/printer/standard_macros.cpp
grep -A 2 "StandardMacroSlot::CleanNozzle" src/printer/standard_macros.cpp
```
Expected: See current patterns without ForgeX names

**Step 2: Add PURGE_FILAMENT and LINE_PURGE to Purge slot**

Change:
```cpp
{StandardMacroSlot::Purge, {"PURGE", "PURGE_LINE", "PRIME_LINE"}},
```
To:
```cpp
{StandardMacroSlot::Purge, {"PURGE", "PURGE_LINE", "PRIME_LINE", "PURGE_FILAMENT", "LINE_PURGE"}},
```

**Step 3: Add CLEAR_NOZZLE to CleanNozzle slot**

Change:
```cpp
{StandardMacroSlot::CleanNozzle, {"CLEAN_NOZZLE", "NOZZLE_WIPE", "WIPE_NOZZLE"}},
```
To:
```cpp
{StandardMacroSlot::CleanNozzle, {"CLEAN_NOZZLE", "NOZZLE_WIPE", "WIPE_NOZZLE", "CLEAR_NOZZLE"}},
```

**Step 4: Verify changes**
```bash
grep -E "Purge|CleanNozzle" src/printer/standard_macros.cpp | head -4
```
Expected: See updated patterns

**Step 5: Commit**
```bash
git add src/printer/standard_macros.cpp
git commit -m "feat(macros): add ForgeX detection patterns (PURGE_FILAMENT, CLEAR_NOZZLE)"
```

---

## Task 5: Add BedMesh Fallback Macro

**Files:**
- Modify: `src/printer/standard_macros.cpp`

**Step 1: Find current fallbacks**
```bash
grep -A 15 "FALLBACK_MACROS" src/printer/standard_macros.cpp
```
Expected: See only HELIX_CLEAN_NOZZLE has fallback

**Step 2: Add HELIX_BED_MESH_IF_NEEDED fallback**

Change:
```cpp
{StandardMacroSlot::BedMesh, ""},
```
To:
```cpp
{StandardMacroSlot::BedMesh, "HELIX_BED_MESH_IF_NEEDED"},
```

**Step 3: Verify**
```bash
grep "BedMesh" src/printer/standard_macros.cpp
```
Expected: See fallback assigned

**Step 4: Build to verify**
```bash
make -j
```
Expected: Build succeeds

**Step 5: Commit**
```bash
git add src/printer/standard_macros.cpp
git commit -m "feat(macros): add HELIX_BED_MESH_IF_NEEDED as BedMesh fallback"
```

---

## Task 6: Add ForgeX Detection Heuristics to Printer Database

**Files:**
- Modify: `config/printer_database.json`

**Step 1: Find AD5M Pro entry**
```bash
grep -n "flashforge_ad5m_pro" config/printer_database.json
```
Expected: Line ~102

**Step 2: Add ForgeX-specific heuristics**

Add these heuristics to the AD5M Pro entry (before the existing heuristics):
```json
{
  "type": "macro_match",
  "field": "macros",
  "pattern": "SUPPORT_FORGE_X",
  "confidence": 99,
  "reason": "SUPPORT_FORGE_X macro - definitive ForgeX signature"
},
{
  "type": "object_exists",
  "field": "printer_objects",
  "pattern": "mod_params",
  "confidence": 95,
  "reason": "mod_params object - ForgeX settings container"
},
```

**Step 3: Fix print_start_capabilities**

Replace the current `print_start_capabilities` with correct ForgeX params:
```json
"print_start_capabilities": {
  "macro_name": "START_PRINT",
  "params": {
    "bed_mesh": {
      "param": "SKIP_LEVELING",
      "skip_value": "1",
      "enable_value": "0",
      "default_value": "0",
      "description": "Skip bed mesh calibration"
    }
  }
}
```

**Step 4: Apply same changes to AD5M (non-Pro) entry**

Find `flashforge_adventurer_5m` entry and apply same heuristics and capabilities.

**Step 5: Validate JSON**
```bash
python3 -c "import json; json.load(open('config/printer_database.json'))"
```
Expected: No output (valid JSON)

**Step 6: Commit**
```bash
git add config/printer_database.json
git commit -m "feat(db): add ForgeX detection heuristics and fix AD5M capabilities

- Add SUPPORT_FORGE_X macro detection (99% confidence)
- Add mod_params object detection (95% confidence)
- Fix print_start_capabilities to use actual ForgeX params
- Remove incorrect DISABLE_PRIMING/DISABLE_SKEW_CORRECT params"
```

---

## Task 7: Build and Test

**Step 1: Full build**
```bash
make -j
```
Expected: Build succeeds

**Step 2: Run unit tests**
```bash
make test-run
```
Expected: All tests pass

**Step 3: Test with mock printer**
```bash
./build/bin/helix-screen --test -vv 2>&1 | grep -i "StandardMacros\|ForgeX"
```
Expected: See StandardMacros initialization logs

**Step 4: (If AD5M available) Deploy and test**
```bash
make pi-test  # or manual deployment
```
Check logs for:
- ForgeX detection confidence
- StandardMacros auto-detection of PURGE_FILAMENT, CLEAR_NOZZLE
- Start a print and verify "Preparing Print" → completion transition

---

## Verification Checklist

- [ ] `make -j` succeeds
- [ ] `make test-run` passes
- [ ] STANDARD_MACROS_SPEC.md shows 10 slots
- [ ] PRINT_START_INTEGRATION.md mentions START_PRINT.preparation_done
- [ ] helix_macros.cfg has HELIX_BED_MESH_IF_NEEDED (not BED_LEVEL)
- [ ] StandardMacros detects PURGE_FILAMENT, CLEAR_NOZZLE
- [ ] Printer database has ForgeX-specific heuristics

---

## Out-of-Box Experience for ForgeX

**What users get without helix_macros.cfg:**
- ✅ Instant completion detection (via `_START_PRINT.print_started`)
- ✅ Heating phase detection (proactive temp monitoring)
- ❌ Granular phase progress ("Leveling...", "Meshing...", "Purging...")

**To get full phase tracking:** Install helix_macros.cfg and add `HELIX_PHASE_*` calls to START_PRINT

---

## Future Work (Not in This Plan)

**Phase Injection Feature:**
- Extend MacroModificationManager to optionally inject `HELIX_PHASE_*` signals into existing macros
- Would give ForgeX users rich phase feedback without modifying their macros
