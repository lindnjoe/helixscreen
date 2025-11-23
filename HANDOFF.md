# Session Handoff Document

**Last Updated:** 2025-11-22
**Current Focus:** Bed mesh centering bug identified, fix ready to implement

---

## 🔥 URGENT FIX NEEDED

### Bed Mesh Visualization Off-Center Bug

**Problem:** The bed mesh is rendering significantly left of center in the viewport.

**Debug Evidence:**
```
Canvas dimensions: 520x410, center at (260, 205)
Mesh center before offset: (222, 231) - 38 pixels left
After "centering": bounds show [0,520] - using full width incorrectly
```

**Root Cause:** In `src/bed_mesh_renderer.cpp` lines 525-526, the centering calculation incorrectly subtracts the current offset from already-offset projected points, creating incorrect bounds and cumulative offset errors.

**DETAILED FIX INSTRUCTIONS:**

### Step 1: Fix the bounds calculation in center_mesh_in_viewport() (Lines 514-544)

Find this code block starting at line 522:
```cpp
// Calculate bounds of the projected mesh WITHOUT the current offset
int min_x = INT_MAX, max_x = INT_MIN;
int min_y = INT_MAX, max_y = INT_MIN;
for (int row = 0; row < renderer->rows; row++) {
    for (int col = 0; col < renderer->cols; col++) {
        // Get coordinates WITHOUT the current offset applied
        int x = renderer->projected_points[row][col].screen_x - renderer->view_state.center_offset_x;
        int y = renderer->projected_points[row][col].screen_y - renderer->view_state.center_offset_y;
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
}
```

**CHANGE TO:**
```cpp
// Calculate bounds of the projected mesh
int min_x = INT_MAX, max_x = INT_MIN;
int min_y = INT_MAX, max_y = INT_MIN;
for (int row = 0; row < renderer->rows; row++) {
    for (int col = 0; col < renderer->cols; col++) {
        // Use the projected coordinates directly - they already have offset applied
        int x = renderer->projected_points[row][col].screen_x;
        int y = renderer->projected_points[row][col].screen_y;
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
}
```

### Step 2: Remove redundant centering call (Line 544)

Find this line after the gradient rendering (around line 544):
```cpp
center_mesh_in_viewport(renderer, canvas_width, canvas_height);
```

**DELETE THIS LINE ENTIRELY** - The centering is already done at line 489 before rendering starts.

### Step 3: Ensure centering_done flag is properly used

Check that around line 515-516, the centering_done flag is being checked:
```cpp
if (renderer->centering_done) {
    return;  // Already centered for this configuration
}
```

And that it's set to true at the end of center_mesh_in_viewport (around line 541):
```cpp
renderer->centering_done = true;
```

### Step 4: Clean up the debug log message (Optional but helpful)

Around line 538, update the debug message to be clearer:
```cpp
spdlog::info("[CENTERING_ONCE] Fixed offset calculated: ({},{}) | Mesh center was: ({},{}) | Canvas center: ({},{})",
             renderer->view_state.center_offset_x, renderer->view_state.center_offset_y,
             center_x, center_y,
             canvas_center_x, canvas_center_y);
```

### WHY THIS FIXES IT:

1. **The Bug:** The projected_points already have center_offset applied from project_3d_to_2d() at lines 762-764:
   ```cpp
   result.screen_x = static_cast<int>(canvas_width / 2 + perspective_x) + view->center_offset_x;
   result.screen_y = static_cast<int>(canvas_height * BED_MESH_Z_ORIGIN_VERTICAL_POS + perspective_y) + view->center_offset_y;
   ```

2. **The Problem:** By subtracting the offset again in center_mesh_in_viewport(), we're calculating bounds based on "double-offset" coordinates, which gives wrong bounds.

3. **The Solution:** Use the projected coordinates as-is for bounds calculation since they already include the offset.

### TESTING THE FIX:

After making changes:
```bash
# Rebuild
make -j

# Run with debug to see centering logs
./build/bin/helix-ui-proto bed-mesh --test -vv 2>&1 | grep CENTERING

# Should see bounds centered around x=260, not [0,520]
# Take a screenshot to verify visual centering
./scripts/screenshot.sh helix-ui-proto bedmesh-fixed "bed-mesh --test"
```

**Expected Results:**
- Mesh should appear centered in viewport
- Debug logs should show reasonable bounds (not [0,520])
- CENTERING_ONCE should only appear once per render
- Visual should match Mainsail reference (centered mesh)

**Files to modify:** Only `src/bed_mesh_renderer.cpp`

---

## ✅ CURRENT STATE

### Recently Completed

**Bed Mesh DRAW_POST Architecture & UI Refinements (2025-11-22)** ⭐ **NEW**
- ✅ **Complete DRAW_POST migration from canvas buffers:**
  - Changed from canvas widget to base `lv_obj` with `LV_EVENT_DRAW_POST` callback
  - Renderer now draws directly to layer (no 720KB buffer allocation)
  - Touch events work automatically (canvas blocked all input)
  - Widget properly clips all rendering to bounds
- ✅ **Touch drag rotation with quality toggle:**
  - Horizontal drag = spin (rotation_z), vertical drag = tilt (rotation_x)
  - Press/release events sync dragging flag between widget and renderer
  - Solid color rendering during drag for performance
  - Gradient interpolation when static for quality
  - `LV_EVENT_PRESS_LOST` handler for drags outside widget
  - Safety check detects missed release events
- ✅ **Fixed axis clipping bug:**
  - Axis lines clamped to canvas bounds (no overflow)
  - Axis labels only drawn when fully inside canvas
  - Secondary bounds check prevents any off-canvas rendering
- ✅ **UI layout improvements:**
  - Changed flex ratio from 70/30 to 80/20 (canvas now 80% width)
  - Removed XML padding (`style_pad_all="0"`)
  - Mesh utilizes full available render area
- ✅ **Zoom constant refactor:**
  - Renamed `BED_MESH_CAMERA_ZOOM_OUT` to `BED_MESH_CAMERA_ZOOM_LEVEL`
  - Changed semantics: 1.0 = neutral, <1.0 = zoomed out, >1.0 = zoomed in
  - Changed value from 0.85 to 1.176 (1/0.85) to maintain same visual appearance
  - Changed code from multiply to divide (matches G-code viewer pattern)
- ✅ **Added "No mesh loaded" message:**
  - Centered gray text when renderer fails (no mesh data)
  - Draws using `lv_draw_label()` in draw callback
- ✅ **Debug logging cleanup:**
  - Changed noisy per-frame messages from debug to trace level
  - Requires `-vvv` to see quad rendering details
- ✅ **Command-line support:**
  - Added `-p bed-mesh` panel option to main.cpp

**Files Modified:**
- `include/bed_mesh_renderer.h` - Camera constants (`ZOOM_LEVEL`), API signatures
- `src/bed_mesh_renderer.cpp` - DRAW_POST rendering, axis clipping, zoom math
- `src/ui_bed_mesh.cpp` - Touch handlers, dragging sync, no-data message
- `src/ui_panel_bed_mesh.cpp` - Panel setup
- `src/main.cpp` - Command-line `-p bed-mesh` support
- `ui_xml/bed_mesh_panel.xml` - Layout (80/20 split, removed padding)

**Commit:** `3b7da62` - Complete DRAW_POST architecture with all UI refinements

**Testing:** Run with mock data:
```bash
cd /Users/pbrown/Code/Printing/helixscreen-bedmesh-mainsail
./build/bin/helix-ui-proto -p bed-mesh --test
```

**G-Code Viewer Performance & Multicolor Rendering (2025-11-21)**
- ✅ **Phase 1 - Performance optimizations + multicolor rendering fixes:**
  - Fixed multicolor rendering bug (OrcaCube_ABS_Multicolor.gcode now displays properly)
  - Configured tool color palette in async geometry builder
  - Added multicolor detection to prevent color override (get_geometry_color_count())
  - Added callback-based async load completion API (gcode_viewer_load_callback_t)
  - Implemented ui_gcode_viewer_get_filename() API
  - Enhanced file info display with filename and simplified filament type
  - Fixed XML long_mode="wrap" syntax
  - Added FPS tracking for performance monitoring (debug level)
- ✅ **Phase 2 - Vertex sharing diagnostics and analysis:**
  - Added vertex sharing statistics tracking
  - Discovered 92.4% vertex sharing rate (34,496/37,347 segments)
  - Original estimate of ~3% was incorrect - vertex sharing already highly optimized
  - Analyzed framebuffer RGB conversion (already optimal)
  - Concluded no further optimization needed

**Performance Metrics (OrcaCube_ABS_Multicolor.gcode):**
- Geometry build: 0.056s
- First render: 99.2ms
- Memory: 10.85 MB
- Vertex sharing: 92.4% (34,496/37,347 segments)
- Normal palette: 10k entries (down from 44k via hash map optimization)

**Commits:**
- `ebf361e` - perf(gcode): Phase 2 - vertex sharing diagnostics and analysis
- `8102c34` - perf(gcode): Phase 1 performance optimizations + multicolor rendering fixes

**N-Sided Elliptical Tube Rendering (2025-11-21)**
- ✅ Implemented configurable N-sided tube cross-sections (N=4, 8, or 16)
- ✅ Elliptical cross-section (width ≠ height) matching FDM extrusion geometry
- ✅ Phase 1: Infrastructure and config (tube_sides parameter)
- ✅ Phase 2: Data structures (vectors instead of hardcoded arrays)
- ✅ Phase 3: Vertex generation (N-based loops with elliptical positioning)
- ✅ Phase 4: Triangle strip generation (N-based with triangle fans for caps)
- ✅ Fixed face winding order for correct backface culling
- ✅ Performance: Hash map optimization for palette lookups (146× speedup)
- ✅ Performance: Re-enabled segment simplification (54% reduction)
- ✅ Cleaned up unused variables and obsolete code
- ✅ Default: N=4 (diamond, fastest) for best performance
- ✅ Optional: N=8 (octagonal), N=16 (circular, matches OrcaSlicer quality)

**Configuration:**
```json
{
  "gcode_viewer": {
    "tube_sides": 4  // Options: 4 (default), 8, 16
  }
}
```

**Commits:**
- `3bf1c42` - docs: archive GCODE_TUBE_NSIDED_REFACTOR.md
- `428b471` - config: change default gcode-test panel file to OrcaCube AD5M
- `d945752` - config: change default tube_sides from 16 to 4
- `26e2a33` - fix(gcode): correct face winding order for N-sided tubes
- `e4d9088` - refactor: clean up unused variables and obsolete code
- `774344b` - feat(gcode): Enable N=8 and N=16 elliptical tube cross-sections
- `62ba4b5` - wip(gcode): Phase 4 - Triangle strip generation for N-sided tubes
- `b3d2368` - wip(gcode): Phase 3 - Vertex generation refactor for N-sided tubes
- `0ab2f42` - wip(gcode): Phase 2 - Data structure refactor for N-sided tubes

**Async G-Code Rendering (2025-11-20/21)**
- ✅ Non-blocking geometry building in background thread
- ✅ UI remains responsive during large file loads
- ✅ Callback-based completion notification
- ✅ File info panel updates automatically after load

---

## 🚀 NEXT PRIORITIES

### 1. **Bed Mesh Mainsail Appearance Refinement** (HIGH PRIORITY) ⭐ **NEXT**

**Goal:** Match Mainsail web UI bed mesh visualization appearance

**Reference:** Mainsail screenshot (see project files)

**Focus Areas (User Priority):**
1. **Grid styling & numeric axis labels** - Add mm tick marks like Mainsail shows
2. **Statistics display** - Add X/Y coordinates to Min/Max values

**Tasks:**
- [ ] Lighten grid line color (darker gray → lighter gray for visibility)
- [ ] Add numeric axis tick labels (0mm, 50mm, 100mm, etc.) on X/Y/Z axes
- [ ] Add X/Y coordinates to Min/Max statistics ("Max [X, Y] = Z mm")
- [ ] Compare visual appearance with Mainsail reference screenshot

**Deferred (out of scope for now):**
- Z-scale slider control
- Toggle controls (Scale gradient, Probed, Mesh, Flat, Wireframe)
- Color gradient palette changes

### 2. **G-Code Viewer - Layer Controls** (LOW PRIORITY)

**Remaining Tasks:**
- [ ] Add layer slider/scrubber for preview-by-layer
- [ ] Add layer range selector (start/end layer)
- [ ] Show current layer number in UI
- [ ] Test with complex multi-color G-code files

**Command-Line Testing:**
```bash
# Test with custom camera angles
./build/bin/helix-ui-proto -p gcode-test --test --gcode-az 90 --gcode-el -10 --gcode-zoom 5

# Test with multi-color file and debug colors
./build/bin/helix-ui-proto -p gcode-test --test \
  --gcode-file assets/test_gcode/multi_color_cube.gcode \
  --gcode-debug-colors

# Test specific camera position for screenshots
./build/bin/helix-ui-proto -p gcode-test --test \
  --gcode-file my_print.gcode \
  --gcode-az 45 --gcode-el 30 --gcode-zoom 2.0 \
  --screenshot 2
```

### 3. **Print Select Integration** (LOW PRIORITY)

**Goal:** Add "Preview" button to print file browser

**Tasks:**
- [ ] Add preview button to file list items in print_select_panel.xml
- [ ] Fetch G-code via Moonraker HTTP API
- [ ] Open viewer in overlay panel
- [ ] Show filename and stats (layers, print time)

---

## 📋 Critical Patterns Reference

### Pattern #0: Per-Face Debug Coloring (NEW)

**Purpose:** Diagnose 3D geometry issues by coloring each face differently

**Usage:**
```cpp
// Enable in renderer constructor or via method
geometry_builder_->set_debug_face_colors(true);

// Colors assigned:
// - Top face: Red (#FF0000)
// - Bottom face: Blue (#0000FF)
// - Left face: Green (#00FF00)
// - Right face: Yellow (#FFFF00)
// - Start cap: Magenta (#FF00FF)
// - End cap: Cyan (#00FFFF)
```

**Implementation:**
- Creates 6 color palette entries when enabled
- Assigns colors based on vertex face membership
- Vertex order: [0-1]=bottom, [2-3]=right, [4-5]=top, [6-7]=left
- Logs once per build: "DEBUG FACE COLORS ACTIVE: ..."

**When to Use:**
- Diagnosing twisted/kinked geometry
- Verifying face orientation and winding order
- Checking vertex ordering correctness
- Validating normal calculations

### Pattern #1: G-Code Camera Angles

**Exact test angle for geometry verification:**
```cpp
// In gcode_camera.cpp reset():
azimuth_ = 85.5f;    // Horizontal rotation
elevation_ = -2.5f;  // Slight downward tilt
zoom_level_ = 10.0f; // 10x zoom (preserved by fit_to_bounds)
```

**Debug Overlay (requires -vv flag):**
- Shows "Az: 85.5° El: -2.5° Zoom: 10.0x" in upper-left
- Only visible with debug logging enabled
- Helps reproduce exact viewing conditions

### Pattern #2: LV_SIZE_CONTENT Bug

**NEVER use `height="LV_SIZE_CONTENT"` or `height="auto"` with complex nested children in flex layouts.**

```xml
<!-- ❌ WRONG - collapses to 0 height -->
<ui_card height="LV_SIZE_CONTENT" flex_flow="column">
  <text_heading>...</text_heading>
  <lv_dropdown>...</lv_dropdown>
</ui_card>

<!-- ✅ CORRECT - uses flex grow -->
<ui_card flex_grow="1" flex_flow="column">
  <text_heading>...</text_heading>
  <lv_dropdown>...</lv_dropdown>
</ui_card>
```

### Pattern #3: Bed Mesh Widget API (DRAW_POST Architecture)

**Custom LVGL widget using direct layer rendering:**

```cpp
#include "ui_bed_mesh.h"

// Set mesh data (triggers automatic redraw)
std::vector<const float*> row_pointers;
// ... populate row_pointers ...
ui_bed_mesh_set_data(widget, row_pointers.data(), rows, cols);

// Update rotation (triggers automatic redraw)
ui_bed_mesh_set_rotation(widget, tilt_angle, spin_angle);

// Force redraw
ui_bed_mesh_redraw(widget);
```

**Architecture (DRAW_POST pattern):**
- Base `lv_obj` (NOT canvas) with `LV_EVENT_DRAW_POST` callback
- Renderer draws directly to layer (no buffer allocation)
- Touch events work automatically (`LV_OBJ_FLAG_CLICKABLE`)
- All rendering clipped to widget bounds
- Touch drag toggles rendering quality:
  - **Dragging:** Solid colors (fast)
  - **Static:** Gradient interpolation (quality)

**Widget automatically manages:**
- Renderer lifecycle (create on init, destroy on delete)
- Dragging state synchronization (widget ↔ renderer)
- Bounds checking (clips all coordinates to widget bounds)
- Touch event handling (press/pressing/release/press_lost)

### Pattern #4: Reactive Subjects for Mesh Data

```cpp
// Initialize subjects
static lv_subject_t bed_mesh_dimensions;
static char dimensions_buf[64] = "No mesh data";
lv_subject_init_string(&bed_mesh_dimensions, dimensions_buf,
                       prev_buf, sizeof(dimensions_buf), "No mesh data");

// Update when mesh changes
snprintf(dimensions_buf, sizeof(dimensions_buf), "%dx%d points", rows, cols);
lv_subject_copy_string(&bed_mesh_dimensions, dimensions_buf);
```

```xml
<!-- Bind label to subject -->
<lv_label name="mesh_dimensions_label" bind_text="bed_mesh_dimensions"/>
```

### Pattern #5: Thread Management - NEVER Block UI Thread

**CRITICAL:** NEVER use blocking operations like `thread.join()` in code paths triggered by UI events.

```cpp
// ❌ WRONG - Blocks LVGL main thread
if (connect_thread_.joinable()) {
    connect_thread_.join();  // UI FREEZES HERE
}

// ✅ CORRECT - Non-blocking cleanup
connect_active_ = false;
if (connect_thread_.joinable()) {
    connect_thread_.detach();
}
```

### Pattern #6: G-code Viewer Widget API

**Custom LVGL widget for G-code 3D visualization:**

```cpp
#include "ui_gcode_viewer.h"

// Create viewer widget
lv_obj_t* viewer = ui_gcode_viewer_create(parent);

// Load G-code file
ui_gcode_viewer_load_file(viewer, "/path/to/file.gcode");

// Change view
ui_gcode_viewer_set_view(viewer, GCODE_VIEW_ISOMETRIC);
ui_gcode_viewer_set_view(viewer, GCODE_VIEW_TOP);

// Reset camera
ui_gcode_viewer_reset_view(viewer);
```

```xml
<!-- Use in XML -->
<gcode_viewer name="my_viewer" width="100%" height="400"/>
```

**Widget features:**
- Touch drag to rotate camera
- Automatic fit-to-bounds framing
- Preset view buttons
- State management (EMPTY/LOADING/LOADED/ERROR)

---

## 📚 Key Documentation

- **G-code Visualization:** `docs/GCODE_VISUALIZATION.md` - Complete system design and integration guide
- **Bed Mesh Analysis:** `docs/GUPPYSCREEN_BEDMESH_ANALYSIS.md` - GuppyScreen renderer analysis
- **Implementation Patterns:** `docs/BEDMESH_IMPLEMENTATION_PATTERNS.md` - Code templates
- **Renderer API:** `docs/BEDMESH_RENDERER_INDEX.md` - bed_mesh_renderer.h reference
- **Widget APIs:** `include/ui_bed_mesh.h`, `include/ui_gcode_viewer.h` - Custom widget public APIs

---

## 🐛 Known Issues

1. **Missing Bed Mesh UI Features**
   - ✅ Grid lines - IMPLEMENTED
   - ✅ Info labels - IMPLEMENTED
   - ✅ Rotation sliders - IMPLEMENTED
   - ❌ Axis labels not implemented
   - ❌ Mesh profile selector not implemented
   - ❌ Variance/deviation statistics not displayed

2. **No Bed Mesh Profile Switching**
   - Can fetch multiple profiles from Moonraker
   - No UI to switch between profiles

3. **G-code Viewer Not Integrated**
   - Standalone test panel works (`-p gcode-test`)
   - Not yet integrated with print select panel
   - No "Preview" button in file browser

---

## 🔍 Debugging Tips

**Bed Mesh Testing:**
```bash
# Git worktree location (feature branch)
cd /Users/pbrown/Code/Printing/helixscreen-bedmesh-mainsail

# Run with mock mesh data
./build/bin/helix-ui-proto -p bed-mesh --test

# Debug logging levels:
# -v    = info  (panel setup, mesh data loaded)
# -vv   = debug (rotation updates, draw callbacks)
# -vvv  = trace (per-frame quad rendering details)

# Take screenshot
./scripts/screenshot.sh helix-ui-proto bedmesh-test "bed-mesh --test"
```

**Bed Mesh Touch Drag Debugging:**
- Touch press: Logs "Press at (x, y), switching to solid"
- Touch drag: Logs "Drag (dx, dy) -> rotation(tilt, spin)"
- Touch release: Logs "Release - final rotation(...), switching to gradient"
- Missed release: Logs warning with indev state, forces gradient mode

**Rotation Mapping:**
- Horizontal drag (dx) = spin (rotation_z, left/right rotation)
- Vertical drag (dy) = tilt (rotation_x, up/down rotation)
- Inverted Y for intuitive control (drag down = tilt away)

**G-Code Viewer Testing:**
```bash
# Run with debug logging and camera overlay
./build/bin/helix-ui-proto -p gcode-test -vv --test

# Test multi-color files
./build/bin/helix-ui-proto -p gcode-test --test-file assets/test_gcode/multi_color_cube.gcode

# Camera overlay shows (upper-left, only with -vv):
# "Az: 85.5° El: -2.5° Zoom: 10.0x"
```

**Screenshot Current State:**
```bash
# Take screenshot
./scripts/screenshot.sh helix-ui-proto gcode-test gcode-test

# View screenshot
open /tmp/ui-screenshot-gcode-test.png
```

**Per-Face Debug Coloring (if needed):**
```cpp
// Enable in gcode_tinygl_renderer.cpp:31
geometry_builder_->set_debug_face_colors(true);

// Colors: Top=Red, Bottom=Blue, Left=Green, Right=Yellow,
//         StartCap=Magenta, EndCap=Cyan
```
