# Belt Tension Visualization - UX Design

**Date:** 2026-02-03
**Status:** Design in progress

---

## CoreXY Belt Path Understanding

### Kinematics (from Wikipedia)

```
ΔX = ½(ΔA + ΔB)   → X movement: both motors same direction
ΔY = ½(ΔA - ΔB)   → Y movement: motors opposite direction
```

**Key insight:** Each motor alone produces DIAGONAL movement. The firmware combines them for X/Y.

### What Klipper's TEST_RESONANCES Tests

- `AXIS=1,1` → Tests diagonal motion combining both motors → excites one belt path
- `AXIS=1,-1` → Tests perpendicular diagonal → excites the other belt path

These are NOT "Belt A" and "Belt B" in a simple sense - they're the RESONANT RESPONSE of each belt path when excited along its primary direction.

### Physical Belt Layout (Voron-style CoreXY)

```
Top View - Simplified:

    [Motor A]─────────────────────[Idler]
         │                           │
         │    ┌─────────────────┐    │
         │    │                 │    │
         │    │   [Toolhead]    │    │
         │    │                 │    │
         │    └─────────────────┘    │
         │                           │
    [Idler]─────────────────────[Motor B]

    Rear ←───────────────────────→ Rear
                    Front
```

**Reality is more complex:**
- Belts run on different Z-levels (stacked or crossed)
- Multiple idler pulleys route the belts
- Each belt forms a continuous loop through the carriage

**For our UI purposes:** Don't try to show exact belt routing. Instead:
- Show "Motor A path" vs "Motor B path"
- Use colors (e.g., red vs blue) to differentiate
- Focus on the RESULTS (frequency) not the mechanical details

---

## User Goals

1. **Check if belts need adjustment** - Quick health check
2. **Know which belt to adjust** - A vs B identification
3. **Know which direction to adjust** - Tighten vs loosen
4. **Verify adjustment worked** - Confirm fix
5. **Match belts to each other** - Equal tension for CoreXY

---

## User Knowledge Levels

| Level | What they know | What they need |
|-------|---------------|----------------|
| **Beginner** | "My prints look off" | Simple pass/fail, "tighten tensioner A" |
| **Intermediate** | Knows about belt tension, has used apps | Frequency numbers, comparison |
| **Expert** | Understands resonance, has done this before | Raw data, fine control |

**Design for beginners, expose detail for experts.**

---

## Target Frequencies

| Printer Type | Target | Source |
|--------------|--------|--------|
| Voron 2.4 | 110 Hz @ 150mm span | [Voron Docs](https://docs.vorondesign.com/tuning/secondary_printer_tuning.html) |
| Voron Z belts | 140 Hz | Voron Docs |
| Prusa CORE One | 96 Hz upper, 92 Hz lower | [Prusa KB](https://help.prusa3d.com/article/adjusting-belt-tension-core-one-l-core-one_845048) |
| Generic CoreXY | 100-120 Hz | Community consensus |

**Recommendation:** Default 110 Hz, configurable per-printer in settings.

---

## Workflow Design (Hybrid Approach)

### Entry Point
Advanced Panel → Calibration → Belt Tension

### Screen 1: Start

```
┌─────────────────────────────────────┐
│  Belt Tension Check          [?]    │
├─────────────────────────────────────┤
│                                     │
│  Check and balance your CoreXY      │
│  belt tensions.                     │
│                                     │
│  This will vibrate each belt path   │
│  and measure the resonant frequency.│
│                                     │
│  Takes about 30 seconds.            │
│                                     │
│  Target: 110 Hz  [Change]           │
│                                     │
│          [ Start Check ]            │
│                                     │
└─────────────────────────────────────┘
```

### Screen 2: Progress

```
┌─────────────────────────────────────┐
│  Testing Belt Tensions...           │
├─────────────────────────────────────┤
│                                     │
│    Measuring A path...              │
│                                     │
│    ████████████░░░░░░░░  60%        │
│                                     │
│    Sweeping 45 → 133 Hz             │
│                                     │
└─────────────────────────────────────┘
```

### Screen 3: Results (Simple)

```
┌─────────────────────────────────────┐
│  Results                            │
├─────────────────────────────────────┤
│                                     │
│   ┌─────────┐      ┌─────────┐      │
│   │ Path A  │      │ Path B  │      │
│   │         │      │         │      │
│   │  108 Hz │      │  95 Hz  │      │
│   │   ✓     │      │   ⚠     │      │
│   └─────────┘      └─────────┘      │
│                                     │
│   Target: 110 Hz (±10 is OK)        │
│   Difference: 13 Hz                 │
│                                     │
│   ┌─────────────────────────────┐   │
│   │ ⚠ Tighten Path B tensioner  │   │
│   │                             │   │
│   │ Path B is 15 Hz low.        │   │
│   │ Tighten until both match.   │   │
│   └─────────────────────────────┘   │
│                                     │
│   [Test Again]  [Show Graph]        │
│                                     │
└─────────────────────────────────────┘
```

### Screen 4: Graph Detail (expandable)

```
┌─────────────────────────────────────┐
│  Frequency Response        [Back]   │
├─────────────────────────────────────┤
│                                     │
│  ┌─────────────────────────────┐    │
│  │        ╱╲ A                 │    │
│  │       ╱  ╲    ╱╲ B          │    │
│  │      ╱    ╲  ╱  ╲           │    │
│  │     ╱      ╲╱    ╲          │    │
│  │────╱────────────────────────│    │
│  │  0    50   100   150  Hz    │    │
│  └─────────────────────────────┘    │
│                                     │
│  Path A: 108 Hz (peak amplitude)    │
│  Path B: 95 Hz (peak amplitude)     │
│                                     │
│  Similarity: 72%                    │
│  (Target: >90% for best prints)     │
│                                     │
└─────────────────────────────────────┘
```

---

## Visual Design Elements

### Status Colors

| Status | Color | Meaning |
|--------|-------|---------|
| ✓ Good | Green (`#A3BE8C`) | Within ±10 Hz of target |
| ⚠ Warning | Orange (`#EBCB8B`) | 10-20 Hz from target |
| ✗ Bad | Red (`#BF616A`) | >20 Hz from target, or A/B diff >15 Hz |

### Path Identification

Instead of "Belt A/B" (confusing), use:
- **"Path A"** and **"Path B"** (matches Klipper terminology)
- Colors: Blue for A, Orange for B (colorblind friendly)
- For adjustment instructions: reference MOTOR positions
  - "Adjust the rear-left tensioner" (for Voron)
  - Or show simplified diagram with highlighted tensioner

---

## Tensioner Location Guidance

Different printers have tensioners in different locations. Options:

1. **Generic advice:** "Adjust your A motor tensioner"
2. **Printer-specific:** Detect printer model and show correct location
3. **Diagram:** Show generic CoreXY layout with "adjust here" highlight

**Recommendation:** Start with generic, add printer-specific later via printer database.

---

## Configuration Needed

Add to Settings → Calibration:

```
Belt Tension Target: [110] Hz
Belt Tension Tolerance: [±10] Hz
```

Could also auto-detect from printer database (Voron, Prusa CORE, etc.)

---

## Technical Implementation Notes

### Moonraker API Call

```gcode
TEST_RESONANCES AXIS=1,1 OUTPUT=raw_data    ; Path A
TEST_RESONANCES AXIS=1,-1 OUTPUT=raw_data   ; Path B
```

This generates CSV files in `/tmp/` that need to be:
1. Retrieved via Moonraker file API
2. Parsed (time, x, y, z accelerometer data)
3. FFT processed to get frequency spectrum
4. Peak detected to find resonant frequency

### Reusable Components

- `ui_frequency_response_chart.cpp` - already exists!
- `InputShaperCalibrator` pattern - state machine for workflow
- MoonrakerAPI G-code execution - already exists

### New Code Needed

1. `BeltTensionCalibrator` class (orchestrator)
2. `MoonrakerAPI::test_belt_resonance()` method
3. CSV parser for raw resonance data
4. FFT implementation (or use existing if we have one)
5. `ui_panel_belt_tension.cpp` + XML

---

## Future: Live Mode (Phase 2)

For advanced users who want real-time feedback:

1. Stream accelerometer data via `adxl345/dump_adxl345`
2. Compute rolling FFT
3. Show live frequency as user plucks belt
4. "Lock" button to freeze reading

This is significantly more complex but very cool for experts.

---

## Non-CoreXY Printers

| Type | Belt Test Approach |
|------|-------------------|
| Cartesian | X and Y are independent, test separately |
| Bedslinger | X belt on gantry, Y belt on bed - both testable |
| Delta | Usually no belts (or very different system) |

**Recommendation:** Focus on CoreXY first (`AXIS=1,1` and `1,-1`). Add Cartesian later with `AXIS=X` and `AXIS=Y`. Skip Delta.

---

## Open Questions

1. **Do we need live streaming?** Phase 2 stretch goal
2. **Printer-specific tensioner guidance?** Nice-to-have
3. **Integration with input shaper?** Could recommend re-running input shaper after belt adjustment
4. **Historical tracking?** Save results over time to detect belt wear

---

## Next Steps

1. [ ] Validate UX with user ← YOU ARE HERE
2. [ ] Mockup XML layout
3. [ ] Implement BeltTensionCalibrator backend
4. [ ] Build UI panel
5. [ ] Test on real CoreXY printer
