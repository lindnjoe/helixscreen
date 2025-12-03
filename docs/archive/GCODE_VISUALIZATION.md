# G-Code 3D Visualization System

**Status:** Phase 1 Complete - Core Infrastructure & Test Panel
**Target:** LVGL 9.4 UI Prototype (HelixScreen)
**Last Updated:** 2025-11-13

## Overview

This document describes the design and implementation plan for a G-code 3D visualization system for HelixScreen. The system will provide:

1. **3D Preview**: Perspective view of G-code files showing toolpaths and geometry
2. **Layer Visualization**: Render specific layers or layer ranges for progress monitoring
3. **Object Selection**: Support for Klipper's exclude objects feature via interactive selection
4. **Print Progress**: Real-time visualization overlay during active prints

**Design Philosophy:** Visual fidelity is important but not top priority. Focus on accurate shape recognition and object identification for practical use cases (thumbnails, progress monitoring, object exclusion).

---

## Architecture Overview

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    UI Layer (LVGL XML)                       │
│  ┌──────────────────────┐  ┌────────────────────────────┐  │
│  │ GCodeViewerWidget    │  │ GCodePreviewPanel          │  │
│  │ (custom canvas)      │  │ (full-screen preview)      │  │
│  └──────────────────────┘  └────────────────────────────┘  │
│  ┌──────────────────────┐  ┌────────────────────────────┐  │
│  │ ObjectListPanel      │  │ LayerControlPanel          │  │
│  │ (exclusion UI)       │  │ (layer slider/filter)      │  │
│  └──────────────────────┘  └────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              Business Logic Layer (C++)                      │
│  ┌───────────────────────────────────────────────────────┐  │
│  │         GCodeVisualizationManager                     │  │
│  │  - Coordinates parsing, rendering, and state          │  │
│  │  - Integrates parser, renderer, camera                │  │
│  │  - Subject-Observer pattern for reactive updates      │  │
│  └───────────────────────────────────────────────────────┘  │
│         │                    │                    │          │
│    ┌────▼────────┐    ┌─────▼─────────┐    ┌────▼─────┐   │
│    │ GCodeParser │    │ GCodeRenderer │    │ GCodeCam │   │
│    │ - Stream    │    │ - 3D → 2D     │    │ - View   │   │
│    │ - Layer idx │    │ - Line draw   │    │ - Rotate │   │
│    │ - Objects   │    │ - Culling     │    │ - Zoom   │   │
│    └─────────────┘    └───────────────┘    └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│            Data Layer (Moonraker API)                        │
│  - Fetch G-code files via HTTP (with Range support)         │
│  - Get file metadata (layers, objects, thumbnails)          │
│  - Send EXCLUDE_OBJECT commands via WebSocket               │
│  - Monitor print progress for live visualization            │
└─────────────────────────────────────────────────────────────┘
```

### File Organization

```
include/
  gcode_parser.h              // Streaming G-code parser
  gcode_renderer.h            // 3D-to-2D projection & rendering
  gcode_camera.h              // View transformation (rotate/pan/zoom)
  gcode_visualization_manager.h  // Main coordinator
  ui_gcode_viewer.h           // Custom LVGL widget declaration

src/
  gcode_parser.cpp
  gcode_renderer.cpp
  gcode_camera.cpp
  gcode_visualization_manager.cpp
  ui_gcode_viewer.cpp         // Widget implementation (draw callback)
  ui_panel_gcode_preview.cpp  // Full-screen preview panel

ui_xml/
  gcode_viewer.xml            // Reusable viewer component
  gcode_preview_panel.xml     // Full-screen preview panel
  gcode_object_list.xml       // Object exclusion UI

tests/
  test_gcode_parser.cpp       // Unit tests for parser
  test_gcode_projection.cpp   // Unit tests for 3D math
  mock_gcode_files.h          // Test data
```

---

## Quick Start / Integration Guide

### Using the G-Code Viewer Widget

The G-code viewer is implemented as a custom LVGL widget that can be embedded in any panel.

#### 1. In XML (Declarative)

```xml
<!-- Add to any panel XML file -->
<gcode_viewer
    name="my_viewer"
    width="100%"
    height="400"
    style_bg_color="#bg_color"/>
```

#### 2. In C++ Code (Programmatic)

```cpp
#include "ui_gcode_viewer.h"

// Create viewer widget
lv_obj_t* viewer = ui_gcode_viewer_create(parent_obj);

// Load a G-code file
ui_gcode_viewer_load_file(viewer, "/path/to/file.gcode");

// Control camera
ui_gcode_viewer_rotate(viewer, 15.0f, 10.0f);  // Azimuth, elevation (degrees)
ui_gcode_viewer_zoom(viewer, 1.2f);             // Zoom factor (>1 = zoom in)
ui_gcode_viewer_reset_view(viewer);             // Return to default view

// Set preset views
ui_gcode_viewer_set_view(viewer, GCODE_VIEW_ISOMETRIC);
ui_gcode_viewer_set_view(viewer, GCODE_VIEW_TOP);
ui_gcode_viewer_set_view(viewer, GCODE_VIEW_FRONT);
ui_gcode_viewer_set_view(viewer, GCODE_VIEW_SIDE);

// Display options
ui_gcode_viewer_set_show_travels(viewer, false);  // Hide travel moves
ui_gcode_viewer_set_layer_range(viewer, 0, 10);   // Show only layers 0-10

// Get viewer state
ui_gcode_viewer_state_t state = ui_gcode_viewer_get_state(viewer);
// States: EMPTY, LOADING, LOADED, ERROR
```

#### 3. Test Panel Example

Run the standalone test panel:
```bash
./build/bin/helix-screen -p gcode-test
```

The test panel demonstrates:
- Full-screen viewer layout
- View control buttons (Iso/Top/Front/Side/Reset)
- File loading (uses `assets/gcode/test.gcode`)
- Real-time statistics display
- Touch gesture support (drag to rotate)

#### 4. Command-Line Options

Control the G-code viewer camera from the command line:

```bash
# Load a specific G-code file
./build/bin/helix-screen -p gcode-test --gcode-file /path/to/file.gcode

# Set camera using compact format (all parameters optional)
./build/bin/helix-screen -p gcode-test --camera "az:90.5,el:4.0,zoom:15.5"

# Set camera using individual arguments
./build/bin/helix-screen -p gcode-test --gcode-az 90.5 --gcode-el 4.0 --gcode-zoom 15.5

# Partial parameters work too
./build/bin/helix-screen -p gcode-test --camera "az:45"
./build/bin/helix-screen -p gcode-test --camera "el:30,zoom:2.0"

# Mix both styles (last value wins)
./build/bin/helix-screen -p gcode-test --camera "az:45" --gcode-el 30

# Enable debug coloring (each face gets a unique color)
./build/bin/helix-screen -p gcode-test --gcode-debug-colors
```

**Camera Parameters:**
- `az` (azimuth): Horizontal rotation in degrees (0-360)
  - 0° = front view, 90° = right view, 180° = back, 270° = left
- `el` (elevation): Vertical rotation in degrees (-90 to 90)
  - 0° = side view, 90° = top view, -90° = bottom view
- `zoom`: Zoom factor (positive number, 1.0 = default)
  - Values > 1.0 zoom in, < 1.0 zoom out

**Default View:** Isometric (az:45°, el:30°, zoom:1.0)

**Source files:**
- `ui_xml/gcode_test_panel.xml` - Panel layout
- `src/ui_panel_gcode_test.cpp` - Event handlers
- `include/ui_panel_gcode_test.h` - API

### Widget Registration

The widget must be registered before use (done automatically in `main.cpp`):

```cpp
// In main.cpp initialization:
ui_gcode_viewer_register();

// XML component registration:
lv_xml_register_component_from_file("A:ui_xml/gcode_test_panel.xml");
```

### Touch Gestures

The viewer supports intuitive touch interactions:
- **Drag:** Rotate camera (0.5° per pixel)
- **Future:** Pinch-zoom, two-finger pan

### Theme Integration

Colors are automatically loaded from the theme system:
- Extrusion moves: `primary` color
- Travel moves: `secondary_light` color
- Object boundaries: `accent` color
- Highlighted objects: `success` color
- Excluded objects: `text_disabled` color

### Performance Characteristics

**Phase 1 (Current):**
- **Memory:** ~32 bytes/segment, ~1KB/layer
- **Rendering:** ~60 FPS for models up to 10K segments
- **Loading:** Streams from file, no full load required

**Target model sizes:**
- Small prints (<5K segments): Excellent performance
- Medium prints (5-20K segments): Good performance
- Large prints (20-50K segments): Acceptable with LOD
- Very large prints (>50K segments): Future optimization needed

---

## Data Structures

### Core Types

```cpp
// 3D toolpath segment
struct ToolpathSegment {
    glm::vec3 start;         // Start point (X, Y, Z)
    glm::vec3 end;           // End point
    bool is_extrusion;       // true=extrusion, false=travel
    std::string object_name; // Empty if not inside EXCLUDE_OBJECT block
    float extrusion_amount;  // E value delta (for future use)
};

// Layer data (Z-indexed)
struct Layer {
    float z_height;                      // Z coordinate
    std::vector<ToolpathSegment> segments;
    AABB bounding_box;                   // Precomputed bounds
    size_t segment_count_extrusion;      // Quick stats
    size_t segment_count_travel;
};

// Object metadata (from EXCLUDE_OBJECT_DEFINE)
struct GCodeObject {
    std::string name;
    glm::vec2 center;
    std::vector<glm::vec2> polygon;      // Boundary polygon
    AABB bounding_box;
    bool is_excluded;                    // User exclusion state
};

// Parsed G-code file
struct ParsedGCodeFile {
    std::string filename;
    std::vector<Layer> layers;           // Indexed by layer number
    std::map<std::string, GCodeObject> objects;
    AABB global_bounding_box;            // Entire model bounds

    // Metadata
    size_t total_segments;
    float estimated_print_time_minutes;
    float total_filament_mm;
};

// Axis-aligned bounding box
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 size() const { return max - min; }
};
```

---

## Component Specifications

### 1. GCodeParser

**Responsibility:** Parse G-code files incrementally, extract movement commands, build layer index.

**API:**
```cpp
class GCodeParser {
public:
    // Parse single line of G-code
    void parse_line(const std::string& line);

    // Finalize parsing and return data
    ParsedGCodeFile finalize();

    // Get current parsing state (for progress reporting)
    size_t lines_parsed() const;
    float current_z() const;

    // Reset for new file
    void reset();

private:
    // State tracking
    glm::vec3 current_position_{0, 0, 0};
    float current_e_{0};
    std::string current_object_;
    float current_z_{0};

    // Accumulated data
    std::vector<Layer> layers_;
    std::map<std::string, GCodeObject> objects_;

    // Parsing helpers
    bool parse_movement_command(const std::string& line);
    bool parse_exclude_object_command(const std::string& line);
    void add_segment(const glm::vec3& start, const glm::vec3& end, bool extrusion);
    void start_new_layer(float z);
};
```

**Parsing Rules:**
- **Movement Commands**: G0 (travel), G1 (move/extrude)
- **Coordinates**: X, Y, Z (absolute or relative depending on G90/G91)
- **Extrusion**: E parameter present = extrusion, absent = travel
- **Layer Detection**: Z coordinate change triggers new layer
- **Object Boundaries**: `EXCLUDE_OBJECT_START/END` commands
- **Object Metadata**: `EXCLUDE_OBJECT_DEFINE` command

**Performance:**
- Target: 1000 lines/second on embedded hardware
- Streaming: Process line-by-line, no full file buffering
- Memory: ~32 bytes per segment, ~100KB per layer (typical)

---

### 2. GCodeRenderer

**Responsibility:** Transform 3D toolpath data to 2D screen coordinates and render via LVGL canvas.

**API:**
```cpp
class GCodeRenderer {
public:
    // Set viewport size
    void set_viewport(int width, int height);

    // Render layer range to LVGL canvas
    void render(lv_layer_t* layer,
                const ParsedGCodeFile& gcode,
                int layer_start,
                int layer_end,
                const GCodeCamera& camera);

    // Render options
    void set_show_travels(bool show);
    void set_show_extrusions(bool show);
    void set_highlight_object(const std::string& name);
    void set_lod_level(int level);  // 0=full, 1=half, 2=quarter

    // Picking (touch interaction)
    std::optional<std::string> pick_object(
        const glm::vec2& screen_pos,
        const ParsedGCodeFile& gcode,
        const GCodeCamera& camera);

private:
    int viewport_width_{800};
    int viewport_height_{480};
    bool show_travels_{true};
    bool show_extrusions_{true};
    std::string highlighted_object_;
    int lod_level_{0};

    // Rendering helpers
    void render_segment(lv_layer_t* layer,
                       const ToolpathSegment& segment,
                       const glm::mat4& transform);
    void render_object_boundary(lv_layer_t* layer,
                               const GCodeObject& object,
                               const glm::mat4& transform);
    glm::vec2 project_to_screen(const glm::vec3& world_pos,
                                const glm::mat4& transform);
    bool should_render_segment(const ToolpathSegment& segment) const;
};
```

**Rendering Pipeline:**
1. **Frustum Culling**: Skip segments outside view
2. **Transform**: Apply camera view+projection matrix
3. **Project**: 3D world coordinates → 2D screen coordinates
4. **Clip**: Clip lines to viewport bounds
5. **Draw**: Use `lv_draw_line()` with appropriate style

**Rendering Styles:**
```cpp
// Color scheme (from theme)
lv_color_t color_extrusion = ui_theme_parse_color("primary");      // Blue
lv_color_t color_travel = ui_theme_parse_color("secondary_light"); // Gray
lv_color_t color_object_boundary = ui_theme_parse_color("accent"); // Orange
lv_color_t color_highlighted = ui_theme_parse_color("success");    // Green
lv_color_t color_excluded = ui_theme_parse_color("text_disabled"); // Faded gray
```

**LOD Strategy:**
- **Level 0** (zoomed in): Render all segments
- **Level 1** (medium): Render every 2nd segment
- **Level 2** (zoomed out): Render every 4th segment + bounding boxes

---

### 3. GCodeCamera

**Responsibility:** Manage view transformation (rotation, pan, zoom) and projection matrix.

**API:**
```cpp
class GCodeCamera {
public:
    // Camera controls
    void rotate(float delta_azimuth, float delta_elevation);
    void pan(float delta_x, float delta_y);
    void zoom(float factor);  // 1.0 = no change, >1.0 = zoom in, <1.0 = zoom out
    void reset();
    void fit_to_bounds(const AABB& bounds);

    // Preset views
    void set_top_view();
    void set_front_view();
    void set_side_view();
    void set_isometric_view();

    // Get matrices
    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix() const;
    glm::mat4 get_view_projection_matrix() const;

    // Configuration
    void set_projection_type(ProjectionType type);  // ORTHOGRAPHIC or PERSPECTIVE
    void set_viewport_size(int width, int height);

    // Ray casting (for object picking)
    glm::vec3 screen_to_world_ray(const glm::vec2& screen_pos) const;

private:
    // View parameters
    float azimuth_{45.0f};        // Rotation around Z axis (degrees)
    float elevation_{30.0f};      // Rotation around X axis (degrees)
    glm::vec2 pan_offset_{0, 0};  // Pan offset in world units
    float zoom_level_{1.0f};      // Zoom factor

    // Projection parameters
    ProjectionType projection_type_{ProjectionType::ORTHOGRAPHIC};
    int viewport_width_{800};
    int viewport_height_{480};
    float near_plane_{0.1f};
    float far_plane_{1000.0f};

    // Helpers
    void update_matrices();
    glm::mat4 view_matrix_;
    glm::mat4 projection_matrix_;
};

enum class ProjectionType {
    ORTHOGRAPHIC,  // Parallel projection (no perspective)
    PERSPECTIVE    // Realistic perspective (future)
};
```

**Default View:**
- **Azimuth**: 45° (looking from front-right)
- **Elevation**: 30° (looking down)
- **Projection**: Orthographic (isometric-like)
- **Zoom**: Fit entire model in view

**Touch Gestures:**
- **Single finger drag**: Rotate (azimuth + elevation)
- **Two finger drag**: Pan
- **Pinch**: Zoom

---

### 4. GCodeVisualizationManager

**Responsibility:** Coordinate parser, renderer, camera, and integrate with Moonraker API.

**API:**
```cpp
class GCodeVisualizationManager {
public:
    // Lifecycle
    void load_file(const std::string& file_path);
    void unload_file();
    bool is_loaded() const;

    // Rendering
    void render_to_canvas(lv_layer_t* layer);

    // Layer control
    void set_visible_layer_range(int start, int end);
    void set_current_layer(int layer);  // For progress tracking

    // Camera control
    GCodeCamera& get_camera();

    // Object exclusion
    std::vector<GCodeObject> get_objects() const;
    void set_object_excluded(const std::string& name, bool excluded);
    void send_exclusion_to_klipper(const std::string& name);

    // Progress tracking
    void set_print_progress(float progress);  // 0.0 - 1.0
    int get_current_layer_from_progress(float progress) const;

    // Data access
    const ParsedGCodeFile* get_parsed_file() const;

    // Subjects (reactive updates)
    Subject<ParsedGCodeFile> file_loaded_subject;
    Subject<int> current_layer_subject;
    Subject<std::string> object_selected_subject;

private:
    std::unique_ptr<ParsedGCodeFile> parsed_file_;
    GCodeParser parser_;
    GCodeRenderer renderer_;
    GCodeCamera camera_;

    int visible_layer_start_{0};
    int visible_layer_end_{-1};  // -1 = all layers
    int current_layer_{0};

    // Background parsing
    void parse_file_async(const std::string& file_path);
    std::future<ParsedGCodeFile> parse_future_;
};
```

**Integration Points:**
- **Print Select Panel**: Add "Preview" button, launch G-code viewer
- **Print Status Panel**: Overlay live progress on 3D view
- **Moonraker API**: Fetch G-code files, send EXCLUDE_OBJECT commands

---

## Implementation Phases

### Phase 1: Core Parser & Renderer (MVP)
**Goal:** Display static 3D preview of G-code file
**Duration:** ~1 week
**Status:** Not started

**Tasks:**
- [x] Research and document design (this document)
- [ ] Implement `GCodeParser`
  - [ ] Parse G0/G1 movement commands
  - [ ] Extract X/Y/Z/E coordinates
  - [ ] Detect layer boundaries (Z changes)
  - [ ] Build layer-indexed data structure
  - [ ] Write unit tests with sample G-code
- [ ] Implement `GCodeRenderer`
  - [ ] 3D-to-2D orthographic projection
  - [ ] Line drawing via LVGL canvas
  - [ ] Color coding (extrusion vs travel)
  - [ ] Basic frustum culling
- [ ] Implement `GCodeCamera`
  - [ ] Fixed isometric view (no interaction yet)
  - [ ] View and projection matrices
  - [ ] Fit-to-bounds auto-framing
- [ ] Create `ui_gcode_viewer.cpp` widget
  - [ ] Custom LVGL widget using canvas
  - [ ] Draw event callback
  - [ ] Integration with renderer
- [ ] Integration
  - [ ] Add "Preview" button to print select panel
  - [ ] Fetch G-code file from Moonraker
  - [ ] Parse and render in full-screen overlay
  - [ ] Test with small/medium/large files

**Success Criteria:**
- Display G-code files in 3D wireframe
- Blue lines for extrusion, gray for travel
- Entire model visible in viewport
- Performance: 30 FPS on desktop, 10+ FPS on embedded target

---

### Phase 2: Interactive View Controls
**Goal:** User can explore model from different angles
**Duration:** ~3-4 days
**Status:** Not started

**Tasks:**
- [ ] Implement camera rotation
  - [ ] Drag gesture handler
  - [ ] Update azimuth/elevation angles
  - [ ] Smooth rotation animation (optional)
- [ ] Implement pan and zoom
  - [ ] Two-finger drag for pan
  - [ ] Pinch gesture for zoom
  - [ ] Clamp zoom limits (0.1x - 10x)
- [ ] Add UI controls
  - [ ] Reset view button
  - [ ] Preset view buttons (top, front, side, iso)
  - [ ] Zoom slider
- [ ] Layer filtering
  - [ ] Layer range slider (e.g., "50-60 of 500")
  - [ ] "Current layer only" toggle
  - [ ] "Show travels" toggle

**Success Criteria:**
- Smooth rotation with touch drag
- Pan and zoom work intuitively
- Layer slider updates view in real-time
- UI controls integrated in preview panel

---

### Phase 3: Object Detection & Selection
**Goal:** Support Klipper exclude objects feature
**Duration:** ~4-5 days
**Status:** Not started

**Tasks:**
- [ ] Parse object metadata
  - [ ] `EXCLUDE_OBJECT_DEFINE` command parsing
  - [ ] Extract NAME, CENTER, POLYGON parameters
  - [ ] Store in `GCodeObject` structures
- [ ] Assign segments to objects
  - [ ] Track current object during parsing
  - [ ] `EXCLUDE_OBJECT_START/END` markers
  - [ ] Associate segments with object names
- [ ] Render object boundaries
  - [ ] Draw polygon outlines on build plate (Z=0)
  - [ ] Render labels at object centers
  - [ ] Use theme colors for consistency
- [ ] Object picking
  - [ ] Implement screen-to-world ray casting
  - [ ] Ray-polygon intersection test
  - [ ] Touch event handler
  - [ ] Highlight selected object
- [ ] Exclusion UI
  - [ ] Object list panel (checkbox list)
  - [ ] Integrate with `ui_panel_gcode_preview.xml`
  - [ ] Visual feedback (gray out excluded objects)
- [ ] Klipper integration
  - [ ] Send `EXCLUDE_OBJECT NAME=foo` via WebSocket
  - [ ] Update exclusion state in UI
  - [ ] Persist exclusions during session

**Success Criteria:**
- Parse object metadata from modern slicers (PrusaSlicer, SuperSlicer, OrcaSlicer)
- Touch object in 3D view to select
- Exclude/include objects via UI
- Excluded objects render in different color
- Exclusion commands sent to Klipper successfully

---

### Phase 4: Print Progress Visualization
**Goal:** Real-time progress overlay during active print
**Duration:** ~3-4 days
**Status:** Not started

**Tasks:**
- [ ] Integrate with print status
  - [ ] Subscribe to Moonraker `display_status` updates
  - [ ] Get current layer from `virtual_sdcard.file_position`
  - [ ] Calculate layer from file position (requires Z-index)
- [ ] Render progress indicator
  - [ ] Completed layers: green
  - [ ] Current layer: yellow/animated
  - [ ] Remaining layers: gray/faded
- [ ] Auto-follow mode
  - [ ] Camera tracks current layer automatically
  - [ ] Smooth animation between layers
  - [ ] Toggle for manual camera control
- [ ] Performance optimization
  - [ ] Only render visible layer range (current ±20)
  - [ ] Update only on layer change (not every progress update)
  - [ ] Cache rendered layers

**Success Criteria:**
- Live progress visualization updates during print
- Current layer clearly highlighted
- Auto-follow mode tracks print head
- No performance degradation during active print

---

### Phase 5: Advanced Features (Future)
**Goal:** Production-ready, optimized system
**Duration:** TBD
**Status:** Not started

**Ideas:**
- [ ] Performance optimization
  - [ ] Background thread for parsing (don't block UI)
  - [ ] Layer caching with LRU eviction
  - [ ] LOD rendering (level-of-detail)
  - [ ] Segment quantization (compress float coordinates)
- [ ] Enhanced rendering
  - [ ] Perspective projection (vs orthographic)
  - [ ] Simple lighting simulation (shading)
  - [ ] Wall thickness visualization
  - [ ] Support structure highlighting
- [ ] Thumbnail generation
  - [ ] Auto-generate preview image from G-code
  - [ ] Cache rendered thumbnails (avoid re-parsing)
  - [ ] Fallback when slicer thumbnail missing
- [ ] Analysis tools
  - [ ] Measure distances (tap two points)
  - [ ] Time-per-layer heatmap
  - [ ] Identify problem areas (thin walls, steep overhangs)
  - [ ] Material usage by object
- [ ] File format support
  - [ ] Compressed G-code (.gcode.gz)
  - [ ] Binary G-code formats (future)

---

## Performance Targets

### Parsing Performance

| File Size | Layers | Segments | Parse Time | Target |
|-----------|--------|----------|------------|--------|
| 1 MB | 100 | 10K | <1s | Desktop |
| 5 MB | 500 | 50K | <3s | Desktop |
| 20 MB | 2000 | 200K | <10s | Desktop |
| 50 MB | 5000 | 500K | <30s | Desktop |

**Embedded targets:** 2-3x slower acceptable (background parsing)

### Rendering Performance

| Segments Visible | FPS (Desktop) | FPS (Embedded) | Notes |
|-----------------|---------------|----------------|-------|
| <1K | 60 | 30 | Smooth interaction |
| 1K - 5K | 30-60 | 15-30 | Acceptable |
| 5K - 10K | 15-30 | 5-15 | Use LOD |
| >10K | <15 | <5 | Force LOD/caching |

**Strategy:** Cache rendered canvas, only redraw on view change

### Memory Budget

```
Component               | Size per Item | Max Items | Total
------------------------|---------------|-----------|--------
ToolpathSegment         | 32 bytes      | 100,000   | 3.2 MB
Layer                   | ~1 KB         | 500       | 0.5 MB
GCodeObject             | ~200 bytes    | 50        | 10 KB
Rendered Canvas Cache   | 800×480×2     | 1         | 0.75 MB
------------------------|---------------|-----------|--------
Total                   |               |           | ~5 MB
```

**Target:** 10 MB total memory footprint (acceptable for embedded hardware)

---

## Klipper Object Exclusion Integration

### G-Code Format Example

```gcode
; Define objects at start of file
EXCLUDE_OBJECT_DEFINE NAME=cube_1 CENTER=50,50 POLYGON=[[45,45],[55,45],[55,55],[45,55]]
EXCLUDE_OBJECT_DEFINE NAME=pyramid_2 CENTER=100,50 POLYGON=[[90,40],[110,40],[100,60]]
EXCLUDE_OBJECT_DEFINE NAME=support_3 CENTER=75,75 POLYGON=[[70,70],[80,70],[80,80],[70,80]]

; Toolpath with object boundaries
EXCLUDE_OBJECT_START NAME=cube_1
G1 X45 Y45 E1.0
G1 X55 Y45 E1.0
; ... more moves for cube_1
EXCLUDE_OBJECT_END NAME=cube_1

EXCLUDE_OBJECT_START NAME=pyramid_2
G1 X90 Y40 E1.0
; ... more moves for pyramid_2
EXCLUDE_OBJECT_END NAME=pyramid_2

; etc.
```

### Slicer Support

| Slicer | Support | Configuration |
|--------|---------|---------------|
| **PrusaSlicer** | ✅ Full | Enable "Label objects" in Print Settings > Output options |
| **SuperSlicer** | ✅ Full | Inherits PrusaSlicer support |
| **OrcaSlicer** | ✅ Full | Enable "Exclude object" in Printer Settings |
| **Cura** | ⚠️ Plugin | Requires "Klipper Preprocessor" plugin |
| **Simplify3D** | ❌ No | Manual post-processing required |

### Moonraker Configuration

User must enable object processing in `moonraker.conf`:

```ini
[file_manager]
enable_object_processing: True
```

### Runtime API

**Send exclusion command:**
```cpp
// Via MoonrakerAPI WebSocket
moonraker_api->send_gcode_command("EXCLUDE_OBJECT NAME=support_3");
```

**Query excluded objects:**
```cpp
// Via `printer.exclude_object.objects` status field
auto objects = moonraker_api->get_excluded_objects();
for (const auto& obj : objects) {
    spdlog::info("Object: {} (excluded={})", obj.name, obj.is_excluded);
}
```

---

## Testing Strategy

### Unit Tests

```cpp
// test_gcode_parser.cpp
TEST_CASE("Parse simple movement command") {
    GCodeParser parser;
    parser.parse_line("G1 X10 Y20 Z0.2 E1.5");
    auto file = parser.finalize();
    REQUIRE(file.layers.size() == 1);
    REQUIRE(file.layers[0].segments.size() == 1);
    REQUIRE(file.layers[0].segments[0].end.x == Approx(10.0f));
}

TEST_CASE("Detect layer boundaries") {
    GCodeParser parser;
    parser.parse_line("G1 X0 Y0 Z0.2");
    parser.parse_line("G1 X10 Y10");
    parser.parse_line("G1 X0 Y0 Z0.4");  // New layer
    auto file = parser.finalize();
    REQUIRE(file.layers.size() == 2);
}

TEST_CASE("Parse EXCLUDE_OBJECT_DEFINE") {
    GCodeParser parser;
    parser.parse_line("EXCLUDE_OBJECT_DEFINE NAME=test CENTER=50,50 POLYGON=[[0,0],[10,0],[10,10]]");
    auto file = parser.finalize();
    REQUIRE(file.objects.count("test") == 1);
    REQUIRE(file.objects["test"].center.x == Approx(50.0f));
}
```

### Integration Tests

```cpp
// Use mock G-code files from different slicers
TEST_CASE("Parse PrusaSlicer output") {
    auto file = load_and_parse("tests/fixtures/prusaslicer_benchy.gcode");
    REQUIRE(file.layers.size() > 0);
    REQUIRE(file.objects.size() > 0);
}

TEST_CASE("Render small file performance") {
    auto file = load_and_parse("tests/fixtures/small_cube.gcode");
    auto start = std::chrono::high_resolution_clock::now();
    renderer.render(layer, file, 0, -1, camera);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    REQUIRE(elapsed < std::chrono::milliseconds(100));
}
```

### Manual Testing

**Test Files:**
- Small (1 MB): Calibration cube, benchy
- Medium (10 MB): Complex model with supports
- Large (50 MB): Multi-part print bed

**Test Cases:**
1. Load file → verify correct layer count
2. Rotate view → verify smooth animation
3. Touch object → verify correct selection
4. Exclude object → verify Klipper command sent
5. Monitor live print → verify progress tracking

---

## Dependencies

### Required Libraries

| Library | Version | Usage | License |
|---------|---------|-------|---------|
| **GLM** | 0.9.9+ | Matrix math, vectors | MIT |
| **LVGL** | 9.4.0 | Canvas drawing, UI widgets | MIT |
| **libhv** | 1.3+ | HTTP client (Moonraker) | BSD |
| **spdlog** | 1.12+ | Logging | MIT |

**Note:** All dependencies already present in codebase.

### No Additional Dependencies

This design intentionally avoids:
- ❌ OpenGL / Vulkan (overkill for 2D line rendering)
- ❌ Three.js / WebGL (requires browser, wrong tech stack)
- ❌ Heavy 3D engines (Ogre, Irrlicht, etc.)

---

## Known Limitations

### Current Limitations

1. **No STL/3MF support**: Only G-code files
   - *Rationale:* G-code is the final format; STL requires slicing
2. **No color printing visualization**: Single-color toolpath
   - *Rationale:* Most printers are single-color; multi-material is rare
3. **No real-time parsing during print**: Pre-parse only
   - *Rationale:* Embedded CPU limited; parse on file selection
4. **Orthographic projection only (Phase 1)**: No perspective
   - *Rationale:* Simpler math, easier to implement; perspective is Phase 5
5. **Limited LOD**: Basic segment skipping only
   - *Rationale:* Good enough for most files; advanced LOD is Phase 5

### Future Enhancements

- Perspective projection for more realistic view
- Multi-material color visualization
- Arc (G2/G3) rendering (currently linearized by slicer)
- Compressed G-code support (.gcode.gz)
- Background parsing thread (non-blocking UI)

---

## References

### Technical Resources

**3D Math:**
- [TinyRenderer Tutorial](https://github.com/ssloy/tinyrenderer) - Lesson 4: Perspective Projection
- [Scratchapixel: 3D Projection](https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/)
- GLM documentation: [glm::lookAt](https://glm.g-truc.net/0.9.9/api/a00668.html), [glm::ortho](https://glm.g-truc.net/0.9.9/api/a00243.html)

**G-Code Parsing:**
- [gpr - C++ G-code Parser](https://github.com/dillonhuff/gpr)
- [RepRap G-code Reference](https://reprap.org/wiki/G-code)
- [Klipper G-code Commands](https://www.klipper3d.org/G-Codes.html)

**Klipper Exclude Objects:**
- [Official Documentation](https://www.klipper3d.org/Exclude_Object.html)
- [Moonraker Object Processing](https://moonraker.readthedocs.io/en/latest/configuration/#file_manager)

**Visualization Examples:**
- [OctoPrint PrettyGCode](https://github.com/Kragrathea/OctoPrint-PrettyGCode) - WebGL visualization
- [gcode.ws](https://gcode.ws/) - Browser-based viewer (Three.js)
- [Fluidd G-code Viewer](https://docs.fluidd.xyz/features/gcode-viewer) - 2D layer preview

### Internal References

- `ARCHITECTURE.md` - Subject-Observer pattern, reactive data binding
- `LVGL9_XML_GUIDE.md` - Custom widget creation
- `docs/BUILD_SYSTEM.md` - Adding new source files to Makefile
- `src/ui_jog_pad.cpp:220-350` - Reference for custom LVGL canvas widget
- `src/ui_temp_graph.cpp` - Reference for real-time data visualization
- `include/moonraker_api.h` - API for fetching G-code files

---

## Next Steps

1. **Create Phase 1 task list** in HANDOFF.md
2. **Implement GCodeParser** with unit tests
3. **Prototype projection math** in standalone test
4. **Create custom LVGL widget** based on jog pad pattern
5. **Integrate with print select panel** for MVP

---

**Questions? See:** [@prestonbrown](https://github.com/prestonbrown) or file issue in repo.
