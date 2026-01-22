# Filament Path Visualization for AMS Panel

## Summary

Add schematic filament path visualization to the AMS panel showing routing from spools through hub/selector to extruder. Support both Happy Hare (linear) and AFC (hub) topologies with active path highlighting, load/unload animations, and segment-based error inference.

---

## Prerequisites

**Pull existing AMS implementation from blackmac.local:**
```bash
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/include/ams* /Users/pbrown/code/helixscreen-ams/include/
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/src/ams* /Users/pbrown/code/helixscreen-ams/src/
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/ui_xml/ams* /Users/pbrown/code/helixscreen-ams/ui_xml/
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/src/ui_spool* /Users/pbrown/code/helixscreen-ams/src/
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/include/ui_spool* /Users/pbrown/code/helixscreen-ams/include/
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/src/ui_ams* /Users/pbrown/code/helixscreen-ams/src/
scp -r pbrown@blackmac.local:Code/Printing/helixscreen-ams-feature/include/ui_ams* /Users/pbrown/code/helixscreen-ams/include/
```

Then verify build: `cd /Users/pbrown/code/helixscreen-ams && make -j`

---

## Design Decisions

| Decision | Choice |
|----------|--------|
| **Topology** | Support both: Linear (Happy Hare) and Hub (AFC) - auto-detect |
| **Visual style** | Schematic/diagram - clean lines like circuit diagram |
| **Error display** | Segment-based inference from sensor/state data |
| **Visibility** | Always visible as part of main AMS panel |
| **Implementation** | Custom LVGL canvas widget following `ui_jog_pad.cpp` pattern |

---

## Architecture

### Layout (Vertical, Portrait-Optimized)

**Key principle**: Spools at top, flowing down through hub/selector to extruder at bottom. Spools are integrated as tappable nodes in the path diagram itself.

```
┌─────────────────────────────────────────┐
│  Header: "Multi-Filament"               │
├─────────────────────────────────────────┤
│                                         │
│   ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │  ← Spool row (tappable)
│   │ ◐ 0  │ │ ◐ 1  │ │ ◐ 2  │ │ ◐ 3  │   │
│   └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘   │
│      │        │        │        │       │  ← Lane/gate lines
│      ○        ○        ○        ○       │  ← Prep sensors (AFC)
│      │        │        │        │       │
│      └────────┴────┬───┴────────┘       │
│                    │                    │  ← Merge point
│               ┌────┴────┐               │
│               │   HUB   │               │  ← Hub/Selector
│               └────┬────┘               │
│                    ○                    │  ← Hub sensor
│                    │                    │
│                    │                    │  ← Output tube
│                    ○                    │  ← Toolhead sensor
│                    │                    │
│               ┌────┴────┐               │
│               │ EXTRUDER│               │  ← Nozzle
│               └─────────┘               │
│                                         │
├─────────────────────────────────────────┤
│  Status: Idle            [Unload][Home] │
└─────────────────────────────────────────┘

◐ = Spool with filament color fill
○ = Sensor point
```

### Hub Topology (AFC) - Vertical
```
    [0]     [1]     [2]     [3]      ← Spools at top (tappable nodes)
     │       │       │       │
     ○       ○       ○       ○       ← Prep sensors
     │       │       │       │
     └───────┴───┬───┴───────┘       ← Lanes merge
                 │
            ┌────┴────┐
            │   HUB   │              ← Hub/merger
            └────┬────┘
                 ○                   ← Hub sensor
                 │
                 ○                   ← Toolhead sensor
                 │
            ┌────┴────┐
            │▼ NOZZLE │              ← Extruder
            └─────────┘
```

### Linear Topology (Happy Hare) - Vertical
```
    [0]     [1]     [2]     [3]      ← Spools at top
     │       │       │       │
     └───────┴───┬───┴───────┘       ← Gates to selector
                 │
            ┌────┴────┐
            │SELECTOR │              ← Selector mechanism
            └────┬────┘
                 ║
                 ║                   ← Bowden tube (double line)
                 ║
                 ○                   ← Toolhead sensor
                 │
            ┌────┴────┐
            │▼ NOZZLE │              ← Extruder
            └─────────┘
```

### Visual States
| State | Rendering |
|-------|-----------|
| **Idle lane** | Thin gray dashed line |
| **Available** | Thin gray solid line |
| **Active/loaded** | Thick line in filament color |
| **Loading** | Animated gradient moving downward |
| **Unloading** | Animated gradient moving upward |
| **Error segment** | Thick red pulsing line |
| **Active spool** | Highlighted border + glow |

---

## Data Model

### Unified Path Model

**Philosophy**: Happy Hare and AFC use different terminology but describe the same physical routing stages. We use AFC-inspired naming with a unified "hub" concept that represents either a selector (HH) or merger (AFC).

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     UNIFIED FILAMENT PATH MODEL                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   SPOOL ──── PREP ──── LANE ──── HUB ──── OUTPUT ──── TOOLHEAD ──── NOZZLE
│     │         │         │         │          │           │           │
│   Storage   Entry    Routing   Router     Final       Entry        Loaded
│            sensor    segment   point      tube       sensor
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│  Happy Hare mapping:                                                    │
│   SPOOL=Gate, PREP=Gate sensor, LANE=Gate-to-selector,                  │
│   HUB=Selector, OUTPUT=Bowden, TOOLHEAD=Extruder sensor                 │
├─────────────────────────────────────────────────────────────────────────┤
│  AFC mapping:                                                           │
│   SPOOL=Lane spool, PREP=Prep sensor, LANE=Lane tube,                   │
│   HUB=Hub/Merger, OUTPUT=Output tube, TOOLHEAD=Toolhead sensor          │
└─────────────────────────────────────────────────────────────────────────┘
```

### New Enums (`ams_types.h`)

```cpp
/**
 * @brief Path topology - affects visual rendering only, not segment model
 */
enum class PathTopology {
    LINEAR = 0,  // Happy Hare: selector picks one input
    HUB = 1      // AFC: merger combines inputs
};

/**
 * @brief Unified path segments (AFC-inspired naming)
 *
 * Both Happy Hare and AFC map to these same logical segments.
 * The widget draws them differently based on PathTopology.
 */
enum class PathSegment {
    NONE = 0,        // No segment / idle
    SPOOL = 1,       // At spool (filament storage)
    PREP = 2,        // At entry sensor (prep/gate sensor)
    LANE = 3,        // In lane/gate-to-router segment
    HUB = 4,         // At router (selector or hub/merger)
    OUTPUT = 5,      // In output tube (bowden or hub output)
    TOOLHEAD = 6,    // At toolhead sensor
    NOZZLE = 7       // Fully loaded in nozzle
};

// Convenience: number of segments for iteration
constexpr int PATH_SEGMENT_COUNT = 7;
```

### Mapping Functions

```cpp
// In ams_backend_happy_hare.cpp
PathSegment map_filament_pos_to_segment(int filament_pos) {
    // HH filament_pos: 0=unloaded, 1=homed_gate, 2=in_gate, 3=in_bowden,
    //                  4=end_bowden, 5=homed_extruder, 6=extruder_entry,
    //                  7=in_extruder, 8=loaded
    switch (filament_pos) {
        case 0: return PathSegment::SPOOL;
        case 1:
        case 2: return PathSegment::PREP;     // Gate area
        case 3: return PathSegment::LANE;     // Moving through
        case 4: return PathSegment::HUB;      // At selector
        case 5: return PathSegment::OUTPUT;   // In bowden
        case 6: return PathSegment::TOOLHEAD; // At extruder
        case 7:
        case 8: return PathSegment::NOZZLE;   // Loaded
        default: return PathSegment::NONE;
    }
}

// In ams_backend_afc.cpp
PathSegment infer_segment_from_sensors(bool prep, bool hub, bool toolhead) {
    if (toolhead) return PathSegment::NOZZLE;
    if (hub) return PathSegment::TOOLHEAD;  // Past hub, approaching toolhead
    if (prep) return PathSegment::HUB;       // Past prep, approaching hub
    return PathSegment::SPOOL;               // Not yet at prep
}
```

### New Subjects (`AmsState`)

| Subject | Type | Description |
|---------|------|-------------|
| `ams_path_topology` | int | PathTopology enum (visual only) |
| `ams_path_active_gate` | int | Currently loaded gate (-1=none) |
| `ams_path_filament_segment` | int | PathSegment where filament currently is |
| `ams_path_error_segment` | int | PathSegment with error (for highlighting) |
| `ams_path_anim_progress` | int | Animation progress 0-100 |

---

## Error Inference Logic

Both systems use the **same unified PathSegment enum**. The backend translates system-specific state to the common model.

### Happy Hare → PathSegment
```
printer.mmu.filament_pos → PathSegment
  0 (unloaded)         → SPOOL
  1-2 (gate area)      → PREP
  3 (in bowden)        → LANE
  4 (end bowden)       → HUB
  5 (homed extruder)   → OUTPUT
  6 (extruder entry)   → TOOLHEAD
  7-8 (loaded)         → NOZZLE
```

### AFC → PathSegment
```
Sensor state → PathSegment
  No sensors           → SPOOL (runout at source)
  Prep only            → LANE (between prep and hub)
  Prep + Hub           → OUTPUT (between hub and toolhead)
  All three            → NOZZLE (fully loaded)

Error location = last sensor that was OK
```

### Unified Error Display
The path canvas widget receives `ams_path_error_segment` (a PathSegment value) and highlights that segment in error color regardless of which backend set it.

---

## Implementation Phases

### Phase 1: Setup & Integration ✅ COMPLETE
1. ✅ Pull existing AMS code from blackmac.local
2. ✅ Verify build passes
3. ✅ Test `./build/bin/helix-screen --test -p ams -vv`
4. ✅ Commit pulled files

### Phase 2: Data Model Extensions ✅ COMPLETE
1. ✅ Add `PathTopology`, `PathSegment` enums to `ams_types.h`
   - Added enums with helper functions: `path_topology_to_string()`, `path_segment_to_string()`
   - Added `path_segment_from_happy_hare_pos()` for Happy Hare filament_pos conversion
   - Added `path_segment_from_afc_sensors()` for AFC sensor state inference
2. ✅ Add path subjects to `AmsState`
   - `ams_path_topology`, `ams_path_active_gate`, `ams_path_filament_segment`
   - `ams_path_error_segment`, `ams_path_anim_progress`
   - All registered with XML system for reactive binding
3. ✅ Add `get_topology()`, `get_filament_segment()`, `infer_error_segment()` to `AmsBackend` interface
4. ✅ Implement in `AmsBackendMock`, `AmsBackendHappyHare`, `AmsBackendAfc`
5. ✅ Updated `sync_from_backend()` to sync path subjects

### Phase 3: Path Canvas Widget ✅ COMPLETE
**Files created:**
- `include/ui_filament_path_canvas.h`
- `src/ui_filament_path_canvas.cpp`

**Implementation completed:**
1. ✅ Registered as LVGL XML widget `<filament_path_canvas>`
2. ✅ Theme-aware: Uses responsive fonts, colors, and spacing from globals.xml
3. ✅ Draw via `LV_EVENT_DRAW_POST` callback
4. ✅ Handle tap events for spool/gate selection with callback
5. ✅ XML attributes: `topology`, `gate_count`, `active_gate`, `filament_segment`, `error_segment`, `anim_progress`, `filament_color`

**Visual layout (vertical, top to bottom):**
- Gate entry points at top (one per gate, distributed horizontally)
- Prep sensor dots (AFC/HUB topology only)
- Lane lines converging diagonally to center
- Hub/Selector box with label
- Output sensor dot
- Toolhead sensor dot
- Nozzle icon at bottom

**Visual states implemented:**
- Idle: Thin gray lines (theme-aware color)
- Active: Thick lines in filament color
- Error: Thick lines in error color

**Theme integration:**
- Colors from `ui_theme_get_color()` with dark/light mode support
- Sizes from `ui_theme_get_spacing()` for responsiveness
- Font from `lv_xml_get_const("font_small")` for responsive typography

**Registered in main.cpp** alongside other AMS widgets.

### Phase 4: Panel Integration & State Binding ✅ COMPLETE

**Goal:** Add the path canvas to the AMS panel and wire it to state subjects.

**Completed:**
1. ✅ Modified `ui_xml/ams_panel.xml`:
   - Added `<filament_path_canvas>` below the slot grid in left column
   - No border/card styling - tight integration with slots above
   - Reduced gap spacing (`space_xs`) between slot card and path canvas
2. ✅ Modified `src/ui_panel_ams.cpp`:
   - Added `path_canvas_` member and `setup_path_canvas()` method
   - Added observers for `path_filament_segment` and `path_topology` subjects
   - Implemented `update_path_canvas_from_backend()` to sync all path state
   - Wired gate click callback to trigger filament load
3. ✅ Updated `AmsBackendMock` to cycle through path segments during load/unload:
   - Load: SPOOL → PREP → LANE → HUB → OUTPUT → TOOLHEAD → NOZZLE
   - Unload: NOZZLE → TOOLHEAD → OUTPUT → HUB → LANE → PREP → SPOOL → NONE
   - Emits STATE_CHANGED events at each segment for animation

**Extruder/Print Head Visual (Bambu-style):**
- Tall rectangular body with 2:1 height-to-width ratio
- Large circular fan duct on front face (dark blade area, lighter center hub)
- Proper isometric 3D projection:
  - Front face: rectangle with vertical gradient
  - Side face: parallelogram with vertical edges, diagonal top/bottom tilting up-right
  - Top face: parallelogram tilting up-right, matching side face angle
- Small tapered nozzle tip at bottom
- Extruder shifted left so filament line bisects center of top surface
- All dimensions responsive via `extruder_scale` (derived from `space_md`)

### Phase 5: Animation ✅ COMPLETE
1. ✅ Used LVGL `lv_anim_t` for progress animation
2. ✅ Draw animated "filament segment" moving along path during load/unload
3. ✅ Pulse animation for error segments
4. ✅ Thread-safety: AmsState uses recursive_mutex, lv_async_call for UI updates

### Phase 6: Real Backend Integration ✅ COMPLETE
1. ✅ Parse AFC sensor states (`AFC_stepper`, `AFC_hub`, `AFC_extruder`)
2. ✅ Implement `compute_filament_segment_unlocked()` with real sensor logic
3. ✅ Implement `infer_error_segment()` in AFC backend
4. ✅ AFC version detection via `afc-install` database namespace
5. ✅ Renamed "Home" → "Reset" (universal terminology for HH/AFC)
6. ✅ Added 31 unit tests for AFC backend

### Phase 7: Polish ✅ COMPLETE
1. ✅ Responsive sizing verified across small/medium/large screens
2. Live testing with real BoxTurtle tracked in `AMS_IMPLEMENTATION_PLAN.md`

---

## Status: COMPLETE

All phases of filament path visualization are complete. This plan is archived.
Live hardware testing is tracked in `AMS_IMPLEMENTATION_PLAN.md`.

---

## Files to Create

| File | Purpose |
|------|---------|
| `include/ui_filament_path_canvas.h` | Path canvas widget header |
| `src/ui_filament_path_canvas.cpp` | Path canvas widget implementation |

## Files to Modify

| File | Changes |
|------|---------|
| `include/ams_types.h` | Add PathTopology, PathSegment enums |
| `include/ams_state.h` | Add path subjects |
| `src/ams_state.cpp` | Initialize path subjects |
| `include/ams_backend.h` | Add get_topology(), infer_error_segment() |
| `src/ams_backend_mock.cpp` | Implement path methods |
| `src/ams_backend_happy_hare.cpp` | Parse filament_pos, implement inference |
| `src/ams_backend_afc.cpp` | Parse sensors, implement inference |
| `ui_xml/ams_panel.xml` | Replace slot grid with integrated path canvas (full width, vertical) |
| `src/ui_panel_ams.cpp` | Add path state observers |
| `src/main.cpp` | Register ui_filament_path_canvas widget |

---

## Reference Patterns

- **Custom canvas widget**: `src/ui_jog_pad.cpp` - draw callback, state struct, theme colors
- **Animation**: `src/ui_heating_animator.cpp` - LVGL lv_anim_t usage
- **Gradient drawing**: `src/ui_gradient_canvas.cpp` - canvas buffer management

---

## Testing

```bash
# Build
cd /Users/pbrown/code/helixscreen-ams && make -j

# Run with mock AMS
./build/bin/helix-screen --test -p ams -s large -vv

# Screenshot
HELIX_AUTO_SCREENSHOT=1 HELIX_AUTO_QUIT_MS=3000 ./build/bin/helix-screen --test -p ams -vv
```

---

## Estimated Time

| Phase | Time |
|-------|------|
| Setup & pull code | 30 min |
| Data model | 1 hr |
| Path canvas widget | 2-3 hrs |
| State binding | 1 hr |
| Animation | 1-2 hrs |
| Backend integration | 1-2 hrs |
| Polish | 1 hr |
| **Total** | **7-10 hrs** |
