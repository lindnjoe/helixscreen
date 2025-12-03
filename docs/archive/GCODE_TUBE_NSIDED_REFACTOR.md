# G-Code Tube Geometry: N-Sided Elliptical Cross-Section Refactor Plan

**Author**: Claude Code
**Date**: 2025-11-21
**Status**: Planning Phase - NOT YET IMPLEMENTED
**Risk Level**: HIGH - Complex geometry code with many interdependencies

---

## Executive Summary

This document provides a comprehensive plan for refactoring the G-code tube geometry builder from hardcoded 4-sided diamond cross-sections to runtime-configurable N-sided elliptical cross-sections (N = 4, 8, or 16).

**Goal**: Match OrcaSlicer's rendering quality with smooth, rounded tube appearance while maintaining performance on TinyGL software renderer.

**Key Insight**: The cross-section is an **ellipse** (oval), not a circle, because:
- Horizontal radius = `extrusion_width / 2` (~0.2mm typical)
- Vertical radius = `layer_height / 2` (~0.1mm typical)
- Aspect ratio ≈ 2:1 (width > height for typical FDM printing)

This accurately models real FDM extrusion geometry.

---

## 1. Current Implementation Analysis

### 1.1 File Structure

**Primary file**: `src/gcode_geometry_builder.cpp`
**Function**: `GeometryBuilder::generate_ribbon_vertices()` (lines 465-837)
**Supporting file**: `include/gcode_geometry_builder.h` (TubeCap typedef)

### 1.2 Current Hardcoded Values

The current implementation uses a **4-sided diamond** cross-section:

```cpp
// Line 544-547: Four vertex offsets
const glm::vec3 d_up = half_height * perp_up;      // Vertical +Z
const glm::vec3 d_down = -half_height * perp_up;   // Vertical -Z
const glm::vec3 d_right = half_width * right;      // Horizontal +X
const glm::vec3 d_left = -half_width * right;      // Horizontal -X
```

**Vertex layout** (4 vertices per ring):
- Index 0: UP (90° in perp_up direction)
- Index 1: RIGHT (0° in right direction)
- Index 2: DOWN (270° in perp_up direction)
- Index 3: LEFT (180° in right direction)

**Face layout** (4 faces connecting adjacent vertices):
- Face 0: TOP-RIGHT (vertices UP→RIGHT)
- Face 1: RIGHT-BOTTOM (vertices RIGHT→DOWN)
- Face 2: BOTTOM-LEFT (vertices DOWN→LEFT)
- Face 3: LEFT-TOP (vertices LEFT→UP)

### 1.3 Vertex Generation Structure

**For first segment**:
1. Start cap: 4 vertices with axial normals (-dir)
2. Prev side face vertices: 8 vertices (2 per face × 4 faces) with radial normals
3. Curr side face vertices: 8 vertices (2 per face × 4 faces) with radial normals
4. End cap: 4 vertices with axial normals (-dir)
5. **Total**: 24 vertices

**For subsequent segments**:
1. Prev side face vertices: 8 vertices (2 per face × 4 faces)
2. Curr side face vertices: 8 vertices (2 per face × 4 faces)
3. End cap: 4 vertices
4. **Total**: 20 vertices

**Key insight**: Each face needs **separate vertices** even though they share positions, because:
- Different normals per face (for Phong shading)
- Different colors per face (for debug mode)
- Triangle strips require specific vertex ordering

### 1.4 Triangle Strip Generation

**Side faces** (4 strips):
```cpp
// Each strip: [prev_v1, prev_v2, curr_v1, curr_v2]
// Creates 2 triangles per face
geometry.strips.push_back({prev_v1, prev_v2, curr_v1, curr_v2});
```

**Start cap** (1 strip for 4-vertex quad):
```cpp
// Special ordering: [UP, LEFT, RIGHT, DOWN]
// Creates 2 triangles: (UP,LEFT,RIGHT) + (LEFT,DOWN,RIGHT)
geometry.strips.push_back({cap_up, cap_left, cap_right, cap_down});
```

**End cap** (1 strip for 4-vertex quad):
```cpp
// Reversed winding: [v0, v1, v3, v2]
// Creates 2 triangles with opposite winding from start cap
geometry.strips.push_back({v0, v1, v3, v2});
```

### 1.5 Current Index Arithmetic

**First segment base calculation**:
```cpp
uint32_t base = idx_start - 20;  // 4 cap + 8 prev + 8 curr = 20
uint32_t start_cap_base = base + 0;
uint32_t prev_faces_base = base + 4;
uint32_t curr_faces_base = base + 12;
```

**Subsequent segment base calculation**:
```cpp
uint32_t base = idx_start - 16;  // 8 prev + 8 curr = 16
uint32_t prev_faces_base = base + 0;
uint32_t curr_faces_base = base + 8;
```

### 1.6 Hardcoded Sections Requiring Changes

**Lines 508-536**: Face color assignments (4 hardcoded colors)
**Lines 544-559**: Vertex offset calculations (4 hardcoded offsets)
**Lines 556-559**: Face normal calculations (4 hardcoded normals)
**Lines 573-650**: Vertex generation (hardcoded UP/RIGHT/DOWN/LEFT references)
**Lines 660-679**: Current vertex generation (hardcoded 4 faces × 2 vertices)
**Lines 681-686**: End cap tracking (hardcoded 4 indices)
**Lines 708-775**: Triangle strip generation (hardcoded 4+1+1 strips)
**Lines 777-823**: End cap generation (hardcoded 4 vertices)
**Lines 825-834**: Triangle count validation (hardcoded formula)

---

## 2. Target Implementation Design

### 2.1 Configuration

**Config location**: `helixconfig.json` → `gcode_viewer.tube_sides`
**Valid values**: 4, 8, 16
**Default**: 16 (matches OrcaSlicer)

**Reading config**:
```cpp
int tube_sides = Config::get_instance()->get<int>("/gcode_viewer/tube_sides", 16);
if (tube_sides != 4 && tube_sides != 8 && tube_sides != 16) {
    spdlog::warn("Invalid tube_sides={}, defaulting to 16", tube_sides);
    tube_sides = 16;
}
const int N = tube_sides;
```

### 2.2 Elliptical Cross-Section Mathematics

**Parametric equation** for N-sided ellipse:
```cpp
float angle_step = 2.0f * M_PI / N;

for (int i = 0; i < N; i++) {
    float angle = i * angle_step;  // 0°, 360°/N, 2×360°/N, ...

    // Vertex position offset from center line
    glm::vec3 offset = half_width * std::cos(angle) * right +
                       half_height * std::sin(angle) * perp_up;

    // Face normal (perpendicular to face, not vertex)
    float face_angle = (i + 0.5f) * angle_step;  // Midpoint between vertices
    glm::vec3 face_normal = glm::normalize(
        half_width * std::cos(face_angle) * right +
        half_height * std::sin(face_angle) * perp_up
    );
}
```

**Why face angle ≠ vertex angle**:
- Vertex angle: Position of vertex on ellipse perimeter
- Face angle: Normal direction perpendicular to the face between two adjacent vertices
- Face angle = (i + 0.5) × angle_step = midpoint angle

**Ellipse properties**:
- When `half_width == half_height` → circle
- When `half_width > half_height` → horizontal ellipse (typical FDM)
- When `half_width < half_height` → vertical ellipse (rare)

### 2.3 Vertex Ordering Convention

**Target ordering**: Start from RIGHT (0°) and proceed counter-clockwise (CCW):

```
For N=4:
  i=0: 0°   → RIGHT  (+right, 0)
  i=1: 90°  → UP     (0, +perp_up)
  i=2: 180° → LEFT   (-right, 0)
  i=3: 270° → DOWN   (0, -perp_up)

For N=8:
  i=0: 0°   → RIGHT
  i=1: 45°  → UP-RIGHT
  i=2: 90°  → UP
  i=3: 135° → UP-LEFT
  i=4: 180° → LEFT
  i=5: 225° → DOWN-LEFT
  i=6: 270° → DOWN
  i=7: 315° → DOWN-RIGHT

For N=16:
  i=0: 0°   → RIGHT
  i=1: 22.5°
  i=2: 45°
  ... (16 evenly spaced vertices)
```

**Change from current ordering**:
- Current: [UP, RIGHT, DOWN, LEFT] = indices [0, 1, 2, 3]
- New: [RIGHT, UP, LEFT, DOWN] = indices [0, 1, 2, 3]

**Implication**: End cap tracking and debug logging will reference different indices, but geometry is identical.

### 2.4 Face Layout

Each face connects vertex i to vertex (i+1) % N:

```cpp
Face i connects:
  - Vertex i (prev ring)
  - Vertex (i+1) % N (prev ring)
  - Vertex i (curr ring)
  - Vertex (i+1) % N (curr ring)
```

**For N=4**: 4 faces
**For N=8**: 8 faces
**For N=16**: 16 faces

### 2.5 Vertex Count Formulas

**First segment**:
- Start cap: N vertices
- Prev side faces: 2N vertices (2 per face × N faces)
- Curr side faces: 2N vertices
- End cap: N vertices
- **Total**: 5N vertices

**Subsequent segments**:
- Prev side faces: 2N vertices
- Curr side faces: 2N vertices
- End cap: N vertices
- **Total**: 4N vertices

**Examples**:
- N=4: First=20, Subsequent=16 (current code)
- N=8: First=40, Subsequent=32
- N=16: First=80, Subsequent=64

### 2.6 Triangle Count Formulas

**Side faces**: N faces × 2 triangles = 2N triangles
**Start cap**: (N-2) triangles (triangle fan)
**End cap**: (N-2) triangles (triangle fan)

**First segment**: 2N + (N-2) + (N-2) = **4N - 4** triangles
**Subsequent segment**: 2N + (N-2) = **3N - 2** triangles

**Examples**:
- N=4: First=12, Subsequent=10
- N=8: First=28, Subsequent=22
- N=16: First=60, Subsequent=46

---

## 3. Triangle Strip Encoding for N-Gons

### 3.1 Problem Statement

**Current renderer limitation**: Only processes `geometry.strips` (4-vertex triangle strips), NOT `geometry.indices` (indexed triangles).

**Challenge**: N-gon caps (N>4) cannot be encoded as a single 4-vertex strip.

**Solutions**:
1. **Option A**: Use multiple 4-vertex strips with degenerate triangles (simpler, keeps renderer unchanged)
2. **Option B**: Add indexed triangle support to renderer (cleaner, but requires renderer changes)

**Recommendation**: **Option A** for this refactor. Option B can be a future optimization.

### 3.2 Degenerate Triangle Strip Encoding

**Degenerate triangle**: A triangle with zero area (two or more vertices at same position).

**Key property**: Degenerate triangles are skipped by the rasterizer, allowing multiple triangles in one strip.

**Triangle fan encoding** using 4-vertex strips with degenerates:

```cpp
// To create N-2 triangles in a fan around vertex v0:
// Triangles: (v0,v1,v2), (v0,v2,v3), (v0,v3,v4), ...

// Encode as multiple 4-vertex strips:
for (int i = 1; i < N - 1; i++) {
    geometry.strips.push_back({
        v0,      // Fan center
        vi,      // Current edge vertex
        vi+1,    // Next edge vertex
        vi+1     // DUPLICATE (creates degenerate triangle)
    });
}

// Each strip creates:
// Triangle 1: (v0, vi, vi+1) - VALID
// Triangle 2: (vi, vi+1, vi+1) - DEGENERATE (ignored)
```

**Example for N=8** (octagon cap):
```
Strip 1: [v0, v1, v2, v2] → Triangle (v0,v1,v2)
Strip 2: [v0, v2, v3, v3] → Triangle (v0,v2,v3)
Strip 3: [v0, v3, v4, v4] → Triangle (v0,v3,v4)
Strip 4: [v0, v4, v5, v5] → Triangle (v0,v4,v5)
Strip 5: [v0, v5, v6, v6] → Triangle (v0,v5,v6)
Strip 6: [v0, v6, v7, v7] → Triangle (v0,v6,v7)

Total: 6 strips, 6 triangles (8-2 = 6)
```

### 3.3 Winding Order for Caps

**Critical**: Winding order determines which side is front-facing.

**Start cap**:
- Normal: `-dir` (points backward from segment)
- View direction: From outside looking at cap (looking in `-dir` direction)
- Winding: **Counter-clockwise (CCW)** when viewed from outside

```cpp
// For start cap triangle fan:
for (int i = 1; i < N - 1; i++) {
    geometry.strips.push_back({
        start_cap_base,         // v0 (fan center)
        start_cap_base + i,     // vi (current)
        start_cap_base + i + 1, // vi+1 (next)
        start_cap_base + i + 1  // Degenerate
    });
}
// Creates triangles: (v0,v1,v2), (v0,v2,v3), ..., (v0,vN-2,vN-1)
// All CCW when looking from outside
```

**End cap**:
- Normal: `-dir` (points backward from segment end)
- View direction: From outside looking at cap (looking in `-dir` direction)
- Winding: **Clockwise (CW)** when viewed from outside (opposite of start)

```cpp
// For end cap triangle fan (reversed winding):
for (int i = 1; i < N - 1; i++) {
    geometry.strips.push_back({
        end_cap_base,             // v0 (fan center)
        end_cap_base + N - i,     // vN-i (reverse order)
        end_cap_base + N - i - 1, // vN-i-1
        end_cap_base + N - i - 1  // Degenerate
    });
}
// Creates triangles: (v0,vN-1,vN-2), (v0,vN-2,vN-3), ..., (v0,v2,v1)
// All CW when looking from outside (matches start cap's opposite face)
```

**Why different winding?**
- Start cap faces backward (from segment start)
- End cap faces backward (from segment end)
- From outside, they face opposite directions
- Same normal direction (-dir for both), but viewed from opposite sides
- Therefore: Opposite winding order

### 3.4 Side Face Strips (Unchanged)

Side faces continue to use simple 4-vertex strips (one per face):

```cpp
// Each face i connects vertex i to vertex (i+1) % N
for (int i = 0; i < N; i++) {
    int next_i = (i + 1) % N;
    geometry.strips.push_back({
        prev_faces_base + 2*i,      // prev ring, vertex i
        prev_faces_base + 2*i + 1,  // prev ring, vertex i+1
        curr_faces_base + 2*i,      // curr ring, vertex i
        curr_faces_base + 2*i + 1   // curr ring, vertex i+1
    });
}
// Creates 2 triangles per face (standard triangle strip behavior)
```

---

## 4. Implementation Plan

### 4.1 Phase 1: Preparation (No Functional Changes)

**Goal**: Add infrastructure without changing behavior.

**Changes**:
1. Add config reading and validation at function start
2. Add `const int N = tube_sides;` variable
3. Add debug logging: `spdlog::debug("Generating tube with N={} sides", N);`
4. Add assertions for vertex/triangle counts (compare actual vs expected)
5. Test: Verify N=4 works with all new infrastructure in place

**Files modified**:
- `src/gcode_geometry_builder.cpp` (add ~10 lines at function start)

**Test command**:
```bash
# Ensure N=4 (current behavior) still works
echo '{"gcode_viewer": {"tube_sides": 4}}' > /tmp/test_config.json
./build/bin/helix-screen -p gcode-test --gcode-file assets/4-segment-square.gcode
```

**Success criteria**:
- No visual changes to rendering
- No change in vertex/triangle counts
- Debug log shows "Generating tube with N=4 sides"

---

### 4.2 Phase 2: Data Structure Refactor

**Goal**: Replace hardcoded arrays with dynamically-sized vectors.

**Changes**:

**1. Update TubeCap typedef** (`include/gcode_geometry_builder.h:322`):
```cpp
// OLD:
using TubeCap = std::array<uint32_t, 4>;

// NEW:
using TubeCap = std::vector<uint32_t>;  // Size determined at runtime
```

**2. Replace hardcoded color variables** with vector (lines ~508-536):
```cpp
// OLD:
uint8_t face_color_top_right = color_idx;
uint8_t face_color_right_bottom = color_idx;
uint8_t face_color_bottom_left = color_idx;
uint8_t face_color_left_top = color_idx;

if (debug_face_colors_) {
    face_color_top_right = add_to_color_palette(geometry, DebugColors::TOP);
    // ... etc
}

// NEW:
std::vector<uint8_t> face_colors(N, color_idx);

if (debug_face_colors_) {
    constexpr uint32_t DEBUG_COLORS[] = {
        DebugColors::TOP,    // Red
        DebugColors::RIGHT,  // Yellow
        DebugColors::BOTTOM, // Blue
        DebugColors::LEFT    // Green
    };
    for (int i = 0; i < N; i++) {
        uint32_t color = DEBUG_COLORS[i % 4];  // Cycle through 4 colors
        face_colors[i] = add_to_color_palette(geometry, color);
    }
}
```

**3. Replace hardcoded offsets** with vector (lines ~544-548):
```cpp
// OLD:
const glm::vec3 d_up = half_height * perp_up;
const glm::vec3 d_right = half_width * right;
const glm::vec3 d_down = -half_height * perp_up;
const glm::vec3 d_left = -half_width * right;

// NEW:
const float angle_step = 2.0f * M_PI / N;
std::vector<glm::vec3> vertex_offsets(N);

for (int i = 0; i < N; i++) {
    float angle = i * angle_step;
    vertex_offsets[i] = half_width * std::cos(angle) * right +
                        half_height * std::sin(angle) * perp_up;
}
```

**4. Replace hardcoded normals** with vector (lines ~556-559):
```cpp
// OLD:
glm::vec3 face_normal_top = glm::normalize(perp_up + right);
glm::vec3 face_normal_right = glm::normalize(right - perp_up);
glm::vec3 face_normal_bottom = glm::normalize(-perp_up - right);
glm::vec3 face_normal_left = glm::normalize(-right + perp_up);

// NEW:
std::vector<glm::vec3> face_normals(N);

for (int i = 0; i < N; i++) {
    // Face normal points outward from face center (midpoint between vertices)
    float face_angle = (i + 0.5f) * angle_step;
    glm::vec3 face_center_offset =
        half_width * std::cos(face_angle) * right +
        half_height * std::sin(face_angle) * perp_up;
    face_normals[i] = glm::normalize(face_center_offset);
}
```

**Files modified**:
- `include/gcode_geometry_builder.h` (1 line change)
- `src/gcode_geometry_builder.cpp` (replace ~50 lines)

**Test command**:
```bash
# Test N=4 with new data structures
./build/bin/helix-screen -p gcode-test --gcode-file assets/4-segment-square.gcode --gcode-debug-colors
```

**Success criteria**:
- Rendering still identical to Phase 1
- Debug colors show same pattern (Red/Yellow/Blue/Green)
- Vertex offsets match previous hardcoded values (within floating-point tolerance)

---

### 4.3 Phase 3: Vertex Generation Refactor

**Goal**: Replace hardcoded vertex generation loops with N-based loops.

**This is the CRITICAL phase** where most bugs will appear.

**Changes**:

**1. Start cap vertices** (first segment only, lines ~573-603):
```cpp
// OLD:
if (is_first_segment) {
    // 4 hardcoded start cap vertices
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_up), ...});
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_right), ...});
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_down), ...});
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_left), ...});
    idx_start += 4;

// NEW:
if (is_first_segment) {
    glm::vec3 cap_normal_start = -dir;
    uint16_t cap_normal_idx = add_to_normal_palette(geometry, cap_normal_start);
    uint8_t start_cap_color_idx = debug_face_colors_
        ? add_to_color_palette(geometry, DebugColors::START_CAP)
        : face_colors[0];

    // N start cap vertices
    for (int i = 0; i < N; i++) {
        glm::vec3 pos = prev_pos + vertex_offsets[i];
        geometry.vertices.push_back({
            quant.quantize_vec3(pos),
            cap_normal_idx,
            start_cap_color_idx
        });
    }
    idx_start += N;
```

**2. Prev side face vertices** (first segment only, lines ~604-625):
```cpp
// OLD:
    // 4 faces × 2 vertices = 8 vertices (hardcoded)
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_up), normal_top, face_color_top_right});
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_right), normal_top, face_color_top_right});
    // ... (8 total vertices)
    idx_start += 8;
}

// NEW:
    // N faces × 2 vertices per face
    for (int i = 0; i < N; i++) {
        int next_i = (i + 1) % N;
        glm::vec3 pos_v1 = prev_pos + vertex_offsets[i];
        glm::vec3 pos_v2 = prev_pos + vertex_offsets[next_i];
        uint16_t normal_idx = add_to_normal_palette(geometry, face_normals[i]);

        geometry.vertices.push_back({
            quant.quantize_vec3(pos_v1),
            normal_idx,
            face_colors[i]
        });
        geometry.vertices.push_back({
            quant.quantize_vec3(pos_v2),
            normal_idx,
            face_colors[i]
        });
    }
    idx_start += 2*N;
}
```

**3. Prev side face vertices** (subsequent segments, lines ~630-650):
```cpp
// OLD:
} else {
    // 8 prev vertices for subsequent segments (hardcoded)
    // Face 1 (TOP-RIGHT): uses UP and RIGHT edge positions
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_up), ...});
    geometry.vertices.push_back({quant.quantize_vec3(prev_pos + d_right), ...});
    // ... (8 total)
    idx_start += 8;
}

// NEW:
} else {
    // Subsequent segments: Generate 2N prev vertices (same as first segment prev logic)
    for (int i = 0; i < N; i++) {
        int next_i = (i + 1) % N;
        glm::vec3 pos_v1 = prev_pos + vertex_offsets[i];
        glm::vec3 pos_v2 = prev_pos + vertex_offsets[next_i];
        uint16_t normal_idx = add_to_normal_palette(geometry, face_normals[i]);

        geometry.vertices.push_back({
            quant.quantize_vec3(pos_v1),
            normal_idx,
            face_colors[i]
        });
        geometry.vertices.push_back({
            quant.quantize_vec3(pos_v2),
            normal_idx,
            face_colors[i]
        });
    }
    idx_start += 2*N;
}
```

**4. Curr side face vertices** (all segments, lines ~660-679):
```cpp
// OLD:
// Generate curr vertices - one set per FACE (4 faces hardcoded)
// Face 1 (TOP-RIGHT): uses UP and RIGHT edge positions
geometry.vertices.push_back({quant.quantize_vec3(pos_up), ...});
geometry.vertices.push_back({quant.quantize_vec3(pos_right), ...});
// ... (8 total)
idx_start += 8;

// NEW:
// Generate curr vertices - 2 per face, N faces
for (int i = 0; i < N; i++) {
    int next_i = (i + 1) % N;
    glm::vec3 pos_v1 = curr_pos + vertex_offsets[i];
    glm::vec3 pos_v2 = curr_pos + vertex_offsets[next_i];
    uint16_t normal_idx = add_to_normal_palette(geometry, face_normals[i]);

    geometry.vertices.push_back({
        quant.quantize_vec3(pos_v1),
        normal_idx,
        face_colors[i]
    });
    geometry.vertices.push_back({
        quant.quantize_vec3(pos_v2),
        normal_idx,
        face_colors[i]
    });
}
idx_start += 2*N;
```

**5. End cap tracking** (lines ~681-686):
```cpp
// OLD:
TubeCap end_cap;
end_cap[0] = idx_start - 8 + 0;  // Face1's UP vertex
end_cap[1] = idx_start - 8 + 1;  // Face1's RIGHT vertex
end_cap[2] = idx_start - 8 + 3;  // Face2's DOWN vertex
end_cap[3] = idx_start - 8 + 5;  // Face3's LEFT vertex

// NEW:
TubeCap end_cap(N);
uint32_t curr_faces_base = idx_start - 2*N;
for (int i = 0; i < N; i++) {
    // Track first vertex of each face (vertex i)
    end_cap[i] = curr_faces_base + 2*i;
}
```

**Files modified**:
- `src/gcode_geometry_builder.cpp` (replace ~120 lines)

**Test command**:
```bash
# Test N=4 after vertex refactor
./build/bin/helix-screen -p gcode-test --gcode-file assets/straight-line-10-segments.gcode

# Test N=8 (first test with new geometry!)
echo '{"gcode_viewer": {"tube_sides": 8}}' > ~/.config/helixscreen/helixconfig.json
./build/bin/helix-screen -p gcode-test --gcode-file assets/straight-line-10-segments.gcode
```

**Success criteria**:
- N=4: Rendering unchanged (regression test)
- N=8: Renders 8-sided octagonal tube (may have missing caps if Phase 4 not complete)
- No crashes or assertion failures
- Vertex count matches formula: First=5N, Subsequent=4N

**Expected issues at this stage**:
- Caps may render incorrectly (Phase 4 fixes this)
- Side faces should render correctly

---

### 4.4 Phase 4: Triangle Strip Generation Refactor

**Goal**: Update strip generation for N-sided geometry and proper caps.

**This phase fixes the caps and completes the geometry.**

**Changes**:

**1. Update index base calculations** (lines ~708-760):
```cpp
// OLD (first segment):
uint32_t base = idx_start - 20;  // 4 cap + 8 prev + 8 curr
uint32_t start_cap_base = base + 0;
uint32_t prev_faces_base = base + 4;
uint32_t curr_faces_base = base + 12;

// NEW (first segment):
uint32_t base = idx_start - 5*N;  // N cap + 2N prev + 2N curr
uint32_t start_cap_base = base;
uint32_t prev_faces_base = base + N;
uint32_t curr_faces_base = base + N + 2*N;

// OLD (subsequent):
uint32_t base = idx_start - 16;  // 8 prev + 8 curr
uint32_t prev_faces_base = base + 0;
uint32_t curr_faces_base = base + 8;

// NEW (subsequent):
uint32_t base = idx_start - 4*N;  // 2N prev + 2N curr
uint32_t prev_faces_base = base;
uint32_t curr_faces_base = base + 2*N;
```

**2. Side face strips** (replace ~4 hardcoded strips with loop):
```cpp
// OLD:
// Four side faces
geometry.strips.push_back({face1_prev_start, face1_prev_start+1, face1_curr_start, face1_curr_start+1});
geometry.strips.push_back({face2_prev_start, face2_prev_start+1, face2_curr_start, face2_curr_start+1});
geometry.strips.push_back({face3_prev_start, face3_prev_start+1, face3_curr_start, face3_curr_start+1});
geometry.strips.push_back({face4_prev_start, face4_prev_start+1, face4_curr_start, face4_curr_start+1});

// NEW:
// N side faces (one strip per face)
for (int i = 0; i < N; i++) {
    geometry.strips.push_back({
        prev_faces_base + 2*i,      // prev ring, vertex i
        prev_faces_base + 2*i + 1,  // prev ring, vertex i+1
        curr_faces_base + 2*i,      // curr ring, vertex i
        curr_faces_base + 2*i + 1   // curr ring, vertex i+1
    });
}
```

**3. Start cap strips** (first segment only):
```cpp
// OLD:
// Start cap - single 4-vertex strip with special ordering
geometry.strips.push_back({start_cap_up, start_cap_left, start_cap_right, start_cap_down});

// NEW:
// Start cap - triangle fan encoded as multiple 4-vertex strips with degenerates
for (int i = 1; i < N - 1; i++) {
    geometry.strips.push_back({
        start_cap_base,         // v0 (fan center)
        start_cap_base + i,     // vi (current edge)
        start_cap_base + i + 1, // vi+1 (next edge)
        start_cap_base + i + 1  // Duplicate (degenerate triangle)
    });
}
```

**4. End cap vertices** (lines ~777-803):
```cpp
// OLD:
// Create 4 new vertices at the SAME POSITIONS as end_cap but with axial normals
geometry.vertices.push_back({geometry.vertices[end_cap[0]].position, end_cap_normal_idx, end_cap_color_idx});  // UP
geometry.vertices.push_back({geometry.vertices[end_cap[1]].position, end_cap_normal_idx, end_cap_color_idx});  // RIGHT
geometry.vertices.push_back({geometry.vertices[end_cap[2]].position, end_cap_normal_idx, end_cap_color_idx});  // DOWN
geometry.vertices.push_back({geometry.vertices[end_cap[3]].position, end_cap_normal_idx, end_cap_color_idx});  // LEFT
idx_start += 4;

// NEW:
// Create N new vertices at the SAME POSITIONS as end_cap vertices but with axial normals
uint32_t idx_end_cap_start = idx_start;
for (int i = 0; i < N; i++) {
    geometry.vertices.push_back({
        geometry.vertices[end_cap[i]].position,
        end_cap_normal_idx,
        end_cap_color_idx
    });
}
idx_start += N;
```

**5. End cap strips**:
```cpp
// OLD:
// End cap strip with reversed winding: [v0, v1, v3, v2]
geometry.strips.push_back({idx_end_cap_start + 0, idx_end_cap_start + 1,
                           idx_end_cap_start + 3, idx_end_cap_start + 2});

// NEW:
// End cap - triangle fan with REVERSED winding (CW instead of CCW)
for (int i = 1; i < N - 1; i++) {
    geometry.strips.push_back({
        idx_end_cap_start,             // v0 (fan center)
        idx_end_cap_start + N - i,     // vN-i (reverse order)
        idx_end_cap_start + N - i - 1, // vN-i-1
        idx_end_cap_start + N - i - 1  // Duplicate (degenerate)
    });
}
```

**6. Triangle count validation** (lines ~825-834):
```cpp
// OLD:
int triangle_count = 12;  // 2*4 side faces + 2 start cap + 2 end cap
if (!is_first_segment) {
    triangle_count = 10;  // 2*4 side faces + 2 end cap
}

// NEW:
int side_triangles = 2*N;  // 2 triangles per side face, N faces
int start_cap_triangles = is_first_segment ? (N-2) : 0;
int end_cap_triangles = N-2;
int triangle_count = side_triangles + start_cap_triangles + end_cap_triangles;

// Equivalent formula:
// First segment: 2N + (N-2) + (N-2) = 4N - 4
// Subsequent: 2N + (N-2) = 3N - 2
```

**Files modified**:
- `src/gcode_geometry_builder.cpp` (replace ~80 lines)

**Test command**:
```bash
# Test all N values with caps
for N in 4 8 16; do
    echo '{"gcode_viewer": {"tube_sides": '$N'}}' > ~/.config/helixscreen/helixconfig.json
    ./build/bin/helix-screen -p gcode-test --gcode-file assets/4-segment-square.gcode \
        --skip-splash --screenshot 2 --timeout 3
    mv /tmp/ui-screenshot-*.bmp /tmp/test-N${N}.bmp
done

# Compare visually
for N in 4 8 16; do
    magick /tmp/test-N${N}.bmp /tmp/test-N${N}.png
done
```

**Success criteria**:
- All N values render complete tubes with start and end caps
- N=4: Identical to before refactor (regression test)
- N=8: Octagonal tubes, noticeably rounder than N=4
- N=16: Smooth circular/elliptical tubes matching OrcaSlicer quality
- No visual artifacts (gaps, missing faces, inside-out caps)

---

### 4.5 Phase 5: Testing and Validation

**Goal**: Comprehensive testing across all configurations.

**Test suite**:

**1. Regression testing (N=4)**:
```bash
# Must be pixel-identical to pre-refactor version
./build/bin/helix-screen -p gcode-test --gcode-file assets/4-segment-square.gcode \
    --skip-splash --screenshot 2 --timeout 3
# Compare with baseline screenshot from before refactor
```

**2. Visual quality testing**:
```bash
# Simple geometry
for N in 4 8 16; do
    # Straight line
    ./build/bin/helix-screen ... --gcode-file assets/straight-line-10-segments.gcode

    # Square with corners
    ./build/bin/helix-screen ... --gcode-file assets/4-segment-square.gcode

    # Complex (3-layer test)
    ./build/bin/helix-screen ... --gcode-file "assets/3 Layer Thin Test.gcode"
done
```

**3. Debug color testing**:
```bash
# Verify face colors cycle correctly for N > 4
for N in 4 8 16; do
    ./build/bin/helix-screen -p gcode-test \
        --gcode-file assets/4-segment-square.gcode \
        --gcode-debug-colors --screenshot 2
done

# Expected:
# N=4: Red, Yellow, Blue, Green (4 distinct faces)
# N=8: R,Y,B,G,R,Y,B,G (colors repeat twice)
# N=16: R,Y,B,G repeated 4 times (16 faces, 4 colors)
```

**4. Geometry validation**:
```bash
# Add debug logging to print actual vs expected counts
# In generate_ribbon_vertices():
spdlog::debug("Segment {}: vertices={}, expected={}",
              segment_idx,
              geometry.vertices.size() - start_vertex_count,
              is_first_segment ? 5*N : 4*N);

spdlog::debug("Segment {}: strips={}, expected={}",
              segment_idx,
              geometry.strips.size() - start_strip_count,
              is_first_segment ? (N + (N-2) + (N-2)) : (N + (N-2)));
```

**5. Performance profiling**:
```bash
# Measure frame times for different N values
for N in 4 8 16; do
    # Run with frame time logging enabled
    ./build/bin/helix-screen -p gcode-test \
        --gcode-file "assets/3 Layer Thin Test.gcode" \
        -vv 2>&1 | grep "frame_time"
done

# Expected results (TinyGL software renderer):
# N=4:  ~16ms per frame (baseline)
# N=8:  ~24ms per frame (1.5x vertices)
# N=16: ~32ms per frame (2x vertices)
# All should maintain >30 FPS
```

**6. Edge case testing**:
```bash
# Single segment
./build/bin/helix-screen --gcode-file assets/single-90-corner.gcode

# Diagonal segments
./build/bin/helix-screen --gcode-file assets/diagonal-then-horizontal.gcode

# Many segments
./build/bin/helix-screen --gcode-file assets/straight-line-10-segments.gcode
```

**Files modified**:
- None (testing only)

**Success criteria**:
- All tests pass for N=4, 8, 16
- N=4 regression test shows zero visual difference
- N=16 matches OrcaSlicer rendering quality
- Performance acceptable (<40ms per frame for complex files)
- No crashes, artifacts, or rendering bugs

---

## 5. Risk Analysis and Mitigation

### 5.1 High-Risk Areas

**Risk 1: Index arithmetic errors**

**Problem**: Off-by-one errors in base calculations will cause crashes or corruption.

**Mitigation**:
- Add assertions after every index calculation
- Use named constants instead of magic numbers
- Test with address sanitizer: `make SANITIZE=address`

**Example assertion**:
```cpp
uint32_t base = idx_start - 5*N;
assert(base >= 0 && base < geometry.vertices.size());
assert(start_cap_base + N <= geometry.vertices.size());
```

**Risk 2: Triangle winding errors**

**Problem**: Incorrect winding causes backface culling to hide faces.

**Mitigation**:
- Test with backface culling disabled first
- Visually verify with debug colors
- Check normals point outward (use wireframe mode if available)

**Testing**:
```bash
# Disable backface culling in renderer for testing
# If faces appear, but disappear with culling → winding is wrong
```

**Risk 3: Degenerate triangle strip encoding**

**Problem**: Incorrect degenerate encoding produces wrong triangle count or visual artifacts.

**Mitigation**:
- Verify triangle count matches formula: (N-2) per cap
- Test caps separately (render only cap triangles)
- Add logging for each generated strip

**Validation**:
```cpp
// For N=8, expect 6 cap triangles
int expected_cap_triangles = N - 2;
int actual_cap_strips = 0;
// Count strips added...
assert(actual_cap_strips == expected_cap_triangles);
```

**Risk 4: Performance degradation**

**Problem**: N=16 could be too slow for TinyGL software renderer.

**Mitigation**:
- Profile early (Phase 4)
- If too slow, make N=8 the default instead of N=16
- Add LOD (level-of-detail): use N=4 for distant objects

**Performance targets**:
- Simple files (<1000 segments): <20ms per frame @ N=16
- Complex files (>5000 segments): <40ms per frame @ N=16
- If not met: default to N=8 instead

**Risk 5: TubeCap size mismatch**

**Problem**: `std::vector` TubeCap could be sized incorrectly, causing out-of-bounds access.

**Mitigation**:
- Always initialize with size: `TubeCap end_cap(N);`
- Assert size before access: `assert(end_cap.size() == N);`
- Use `.at()` instead of `[]` during development (bounds checking)

---

### 5.2 Regression Prevention

**Strategy**: Maintain pixel-perfect compatibility for N=4.

**Approach**:
1. Take baseline screenshots before refactor (Phase 0)
2. After each phase, compare N=4 output to baseline
3. If ANY difference appears → stop and debug before proceeding

**Baseline capture**:
```bash
# Before starting refactor
for file in assets/*.gcode; do
    ./build/bin/helix-screen -p gcode-test --gcode-file "$file" \
        --skip-splash --screenshot 2 --timeout 3
    mv /tmp/ui-screenshot-*.bmp "/tmp/baseline-$(basename "$file" .gcode).bmp"
done
```

**Regression check** (after each phase):
```bash
# After phase X
for file in assets/*.gcode; do
    ./build/bin/helix-screen -p gcode-test --gcode-file "$file" \
        --skip-splash --screenshot 2 --timeout 3

    baseline="/tmp/baseline-$(basename "$file" .gcode).bmp"
    current="/tmp/ui-screenshot-*.bmp"

    # Pixel-by-pixel comparison (must be identical)
    if ! cmp -s "$baseline" "$current"; then
        echo "REGRESSION: $file differs from baseline!"
        exit 1
    fi
done
```

---

## 6. Code Comments and Documentation

### 6.1 Inline Documentation Requirements

Every modified function must include:

**1. Function-level comment** explaining N-sided approach:
```cpp
/**
 * Generate N-sided elliptical tube geometry for a G-code segment.
 *
 * Creates a tube with N vertices arranged in an ellipse around the segment path.
 * Each vertex is positioned using parametric ellipse equation:
 *   offset_i = half_width * cos(2πi/N) * right + half_height * sin(2πi/N) * perp_up
 *
 * Geometry structure:
 * - First segment: N start cap vertices + 2N prev side vertices + 2N curr side vertices + N end cap vertices
 * - Subsequent: 2N prev side vertices + 2N curr side vertices + N end cap vertices
 *
 * Triangle strips:
 * - N side face strips (4 vertices each, 2 triangles per strip)
 * - (N-2) start cap triangle fan strips (encoded as 4-vertex degenerate strips)
 * - (N-2) end cap triangle fan strips (reversed winding)
 *
 * @param N Number of sides (4, 8, or 16)
 * @param segment The G-code segment to generate geometry for
 * @param geometry Output geometry (vertices, strips, palettes)
 * @param quant Quantization parameters for int16 vertex compression
 * @param prev_start_cap Optional: reuse end cap from previous segment (not yet implemented)
 * @return TubeCap containing N vertex indices for segment end (for next segment's connection)
 */
```

**2. Section comments** for complex logic:
```cpp
// Generate N vertices in elliptical arrangement
// Vertex angle: i * 2π/N (evenly spaced around perimeter)
// Position: center + cos(angle)*right*width/2 + sin(angle)*up*height/2
for (int i = 0; i < N; i++) {
    // ...
}

// Generate triangle fan for cap using degenerate strips
// Each strip: [v0, vi, vi+1, vi+1] creates triangle (v0,vi,vi+1)
// The repeated vertex creates a degenerate triangle that's ignored
for (int i = 1; i < N - 1; i++) {
    // ...
}
```

**3. Formula comments** for calculations:
```cpp
// Face normal points perpendicular to face center (not vertex)
// Face center is at midpoint angle between two adjacent vertices
float face_angle = (i + 0.5f) * angle_step;  // Midpoint: (i + i+1)/2 = i+0.5
```

### 6.2 External Documentation Updates

**File**: `docs/GCODE_VIEWER_CONFIG.md`

**Add section**:
```markdown
## Tube Cross-Section Configuration

The G-code viewer renders extrusion paths as 3D tubes with elliptical cross-sections. The number of sides used to approximate the ellipse can be configured:

### Setting: `tube_sides`

**Path**: `gcode_viewer.tube_sides`
**Type**: Integer
**Valid values**: 4, 8, 16
**Default**: 16

**Description**: Number of sides in the polygonal approximation of the tube cross-section.

**Trade-offs**:
- **4 sides** (diamond): Fastest rendering, lowest memory, angular appearance
- **8 sides** (octagon): Balanced performance and quality, slightly rounded
- **16 sides** (near-circular): Smoothest appearance, matches OrcaSlicer quality, highest vertex count

**Performance impact**:
- N=4:  Baseline (100%)
- N=8:  ~150% vertices and triangles
- N=16: ~200% vertices and triangles

For the TinyGL software renderer, N=16 is recommended for modern hardware (>2 GHz CPU). Use N=8 on slower embedded systems.

**Example**:
```json
{
  "gcode_viewer": {
    "tube_sides": 16
  }
}
```

**Visual comparison**:

| N=4 (Diamond) | N=8 (Octagon) | N=16 (Circular) |
|---------------|---------------|-----------------|
| Angular facets visible | Slightly rounded, octagonal outline | Smooth, circular appearance |
| Fastest | Medium | Most detailed |
```

---

## 7. Rollback Plan

If the refactor encounters insurmountable issues:

**Option 1: Revert entire refactor**
```bash
git revert <commit_hash>
git push
```

**Option 2: Disable new feature, keep infrastructure**
```cpp
// Force N=4 for now, keeping refactored code
int tube_sides = 4;  // TODO: Re-enable config after fixing bugs
// int tube_sides = Config::get_instance()->get<int>("/gcode_viewer/tube_sides", 16);
```

**Option 3: Make it opt-in**
```json
{
  "gcode_viewer": {
    "tube_sides": 4,  // Keep at 4 for stability
    "_enable_experimental_nsided": false  // Future: flip to true when ready
  }
}
```

---

## 8. Success Criteria

**The refactor is successful when ALL of the following are true**:

### 8.1 Functional Requirements
- [ ] N=4 produces identical output to pre-refactor code (pixel-perfect regression test)
- [ ] N=8 renders 8-sided octagonal tubes with correct geometry
- [ ] N=16 renders smooth circular/elliptical tubes matching OrcaSlicer quality
- [ ] All test G-code files render without artifacts
- [ ] Debug face colors work correctly for all N values

### 8.2 Code Quality Requirements
- [ ] No compiler warnings
- [ ] No runtime assertions or crashes
- [ ] Code passes `make lint` (if linting is configured)
- [ ] All functions have comprehensive inline comments
- [ ] Documentation updated in `docs/GCODE_VIEWER_CONFIG.md`

### 8.3 Performance Requirements
- [ ] N=4: Frame time unchanged from baseline (±5%)
- [ ] N=8: Frame time <150% of N=4 baseline
- [ ] N=16: Frame time <200% of N=4 baseline
- [ ] Complex files (3-layer test): Maintains >25 FPS @ N=16

### 8.4 Geometry Validation
- [ ] Vertex counts match formulas: First=5N, Subsequent=4N
- [ ] Triangle counts match formulas: First=4N-4, Subsequent=3N-2
- [ ] Strip counts correct: N side + (N-2) start + (N-2) end
- [ ] No degenerate geometry (zero-area triangles except intentional ones)
- [ ] Normals point outward (verified with debug rendering)

### 8.5 User Experience
- [ ] Default config (N=16) matches OrcaSlicer quality
- [ ] Configuration is discoverable (documented, commented)
- [ ] Invalid values fall back gracefully with warning
- [ ] No breaking changes to existing config files

---

## 9. Timeline Estimate

**Total estimated time**: 12-16 hours of focused development

**Phase breakdown**:
- Phase 0 (Baseline capture): 1 hour
- Phase 1 (Preparation): 2 hours
- Phase 2 (Data structures): 2 hours
- Phase 3 (Vertex generation): 4 hours ⚠️ HIGH RISK
- Phase 4 (Triangle strips): 3 hours ⚠️ HIGH RISK
- Phase 5 (Testing): 2 hours
- Documentation: 2 hours

**Critical path**: Phases 3-4 (vertex and strip generation)

**Recommended schedule**:
- Day 1: Phases 0-2 (infrastructure, no risk)
- Day 2: Phase 3 (vertex generation, test thoroughly before proceeding)
- Day 3: Phase 4 (triangle strips, test with all N values)
- Day 4: Phase 5 + documentation (validation and polish)

---

## 10. Post-Refactor Optimization Opportunities

**After this refactor is complete and stable**, consider these improvements:

### 10.1 Vertex Sharing (Future)
Currently, vertices at segment junctions are duplicated. Implement proper vertex sharing:
- Save ~50% vertices for multi-segment paths
- Requires tracking previous segment's end cap indices
- Complexity: Medium

### 10.2 Level-of-Detail (Future)
Automatically reduce N for distant or small segments:
- Use N=16 for nearby geometry
- Use N=8 for medium distance
- Use N=4 for far distance or tiny segments
- Complexity: High

### 10.3 Indexed Triangle Rendering (Future)
Add support for `geometry.indices` in renderer:
- Cleaner cap encoding (no degenerate strips needed)
- Slightly better performance (fewer vertices sent to GPU)
- Requires renderer changes
- Complexity: Medium

### 10.4 Corner Smoothing (Future)
Implement OrcaSlicer's miter join algorithm:
- Smooth corners by adjusting inner/outer vertices
- Requires post-processing pass after geometry generation
- Significant visual improvement at corners
- Complexity: High

---

## Appendix A: Quick Reference

### Vertex Count Formulas
```
First segment:    5N (N cap + 2N prev + 2N curr + N cap)
Subsequent:       4N (2N prev + 2N curr + N cap)

N=4:  First=20,  Subsequent=16
N=8:  First=40,  Subsequent=32
N=16: First=80,  Subsequent=64
```

### Triangle Count Formulas
```
First segment:    4N - 4  (2N side + (N-2) start_cap + (N-2) end_cap)
Subsequent:       3N - 2  (2N side + (N-2) end_cap)

N=4:  First=12,  Subsequent=10
N=8:  First=28,  Subsequent=22
N=16: First=60,  Subsequent=46
```

### Strip Count Formulas
```
First segment:    3N - 4  (N side + (N-2) start_cap + (N-2) end_cap)
Subsequent:       2N - 2  (N side + (N-2) end_cap)

N=4:  First=8,   Subsequent=6
N=8:  First=20,  Subsequent=14
N=16: First=44,  Subsequent=30
```

### Index Base Calculations
```cpp
// First segment
uint32_t base = idx_start - 5*N;
uint32_t start_cap_base = base;
uint32_t prev_faces_base = base + N;
uint32_t curr_faces_base = base + N + 2*N;

// Subsequent segment
uint32_t base = idx_start - 4*N;
uint32_t prev_faces_base = base;
uint32_t curr_faces_base = base + 2*N;
```

---

## Appendix B: Testing Checklist

Copy this checklist and mark items as complete during implementation:

### Phase 1: Preparation
- [ ] Config reading and validation added
- [ ] Debug logging added
- [ ] N=4 regression test passes (no visual change)

### Phase 2: Data Structures
- [ ] TubeCap typedef updated to vector
- [ ] Face colors vector working
- [ ] Vertex offsets vector working
- [ ] Face normals vector working
- [ ] N=4 regression test passes

### Phase 3: Vertex Generation
- [ ] Start cap vertices (N vertices)
- [ ] Prev side vertices first segment (2N vertices)
- [ ] Prev side vertices subsequent (2N vertices)
- [ ] Curr side vertices (2N vertices)
- [ ] End cap tracking (N indices)
- [ ] N=4 regression test passes
- [ ] N=8 renders (side faces work, caps may be wrong)
- [ ] Vertex counts validated for all N

### Phase 4: Triangle Strips
- [ ] Index base calculations updated
- [ ] Side face strips (N strips)
- [ ] Start cap strips ((N-2) strips with degenerates)
- [ ] End cap vertices (N vertices)
- [ ] End cap strips ((N-2) strips with reversed winding)
- [ ] Triangle count validation
- [ ] N=4 regression test passes
- [ ] N=8 complete geometry with caps
- [ ] N=16 complete geometry with caps

### Phase 5: Testing
- [ ] All test files render correctly
- [ ] Debug colors work for N=4, 8, 16
- [ ] Performance acceptable for all N
- [ ] No crashes or artifacts
- [ ] Geometry counts match formulas

### Documentation
- [ ] Inline comments added
- [ ] Function docstrings updated
- [ ] GCODE_VIEWER_CONFIG.md updated
- [ ] This plan document reviewed

---

**END OF REFACTOR PLAN**
