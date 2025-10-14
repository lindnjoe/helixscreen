# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **LVGL 9 UI Prototype** for GuppyScreen - a declarative XML-based touch UI system using LVGL 9.3 with reactive Subject-Observer data binding. The prototype runs on SDL2 for rapid development and will eventually target framebuffer displays on embedded hardware.

**Key Innovation:** Complete separation of UI layout (XML) from business logic (C++), similar to modern web frameworks. No manual widget management - all updates happen through reactive subjects.

## Quick Commands

```bash
make                          # Incremental build (auto-parallel)
make clean && make            # Clean rebuild
./build/bin/guppy-ui-proto    # Run simulator
./scripts/screenshot.sh       # Build + screenshot (2s capture)
python3 scripts/generate-icon-consts.py  # Regenerate icon constants
```

**Binary:** `build/bin/guppy-ui-proto`
**Panels:** home, controls, motion, filament, settings, advanced, print-select

## Architecture

```
XML Components (ui_xml/*.xml)
    ↓ bind_text/bind_value/bind_flag
Subjects (reactive data)
    ↓ lv_subject_set_*/copy_*
C++ Wrappers (src/ui_*.cpp)
```

**Component Hierarchy:**
```
app_layout.xml
├── navigation_bar.xml (5 buttons)
└── content_area
    ├── home_panel.xml
    ├── controls_panel.xml (launcher → motion/temps/extrusion sub-screens)
    ├── print_select_panel.xml
    └── [filament/settings/advanced]_panel.xml
```

All components reference `globals.xml` for shared constants (`#primary_color`, `#nav_width`, etc).

## Critical Patterns (Project-Specific)

### 1. Subject Initialization Order ⚠️

**MUST initialize subjects BEFORE creating XML:**

```cpp
// CORRECT ORDER:
lv_xml_component_register_from_file("A:/ui_xml/globals.xml");
lv_xml_component_register_from_file("A:/ui_xml/home_panel.xml");

ui_nav_init();                      // Initialize subjects
ui_panel_home_init_subjects();

lv_xml_create(screen, "app_layout", NULL);  // NOW create UI
```

If subjects are created in XML before C++ initialization, they'll have empty/default values.

### 2. Component Instantiation Names ⚠️

**CRITICAL:** Always add explicit `name` attributes to component tags:

```xml
<!-- app_layout.xml -->
<lv_obj name="content_area">
  <controls_panel name="controls_panel"/>  <!-- Explicit name required -->
  <home_panel name="home_panel"/>
</lv_obj>
```

**Why:** Component names in `<view name="...">` definitions do NOT propagate to `<component_tag/>` instantiations. Without explicit names, `lv_obj_find_by_name()` returns NULL.

See **docs/LVGL9_XML_GUIDE.md** section "Component Instantiation: Always Add Explicit Names" for details.

### 3. Widget Lookup by Name

Always use `lv_obj_find_by_name(parent, "widget_name")` instead of index-based `lv_obj_get_child(parent, 3)`.

```cpp
// In XML: <lv_label name="temp_display" bind_text="temp_text"/>
// In C++:
lv_obj_t* label = lv_obj_find_by_name(panel, "temp_display");
```

See **docs/QUICK_REFERENCE.md** for common patterns.

### 4. FontAwesome Icon Generation

Icons are auto-generated to avoid UTF-8 encoding issues:

```bash
# After adding icons to include/ui_fonts.h:
python3 scripts/generate-icon-consts.py  # Updates globals.xml
make                                      # Rebuild
```

**Adding new icons:**
1. Edit `package.json` to add Unicode codepoint to font range
2. Run `npm run convert-font-XX` (XX = 16/32/48/64)
3. Run `python3 scripts/generate-icon-consts.py`
4. Rebuild with `make`

See **docs/LVGL9_XML_GUIDE.md** for font generation details.

## Common Gotchas

1. **Subject registration conflict** - If `globals.xml` declares subjects, they're registered with empty values before C++ initialization. Solution: Remove `<subjects>` from globals.xml.

2. **Icon constants not rendering** - Run `python3 scripts/generate-icon-consts.py` to regenerate UTF-8 byte sequences.

3. **BMP screenshots too large** - Always convert to PNG before reading: `magick screenshot.bmp screenshot.png`

4. **Labels not clickable** - Use `lv_button` instead of `lv_label` with `flag_clickable`. XML parser doesn't apply clickable flag to labels.

## Screenshot Workflow

```bash
./scripts/screenshot.sh                    # Auto-capture after 2s
LATEST=$(ls -t /tmp/ui-screenshot-*.bmp | head -1)
magick "$LATEST" "${LATEST%.bmp}.png"      # Convert BMP → PNG
```

Press 'S' in running UI for manual screenshot.

## UI Review System (Optional)

For automated UI verification against requirements:

```bash
./scripts/review-ui-screenshot.sh \
  --screenshot /tmp/screenshot.png \
  --requirements docs/requirements/panel-v1.md \
  --xml-source ui_xml/panel.xml \
  --changelog docs/changelogs/panel-2025-01-13.md
```

Templates available in `docs/templates/`. See section in original CLAUDE.md for full details if needed.

## Documentation

- **[LVGL9_XML_GUIDE.md](docs/LVGL9_XML_GUIDE.md)** - Complete LVGL 9 XML reference, patterns, troubleshooting
- **[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Common patterns, quick lookup
- **[STATUS.md](STATUS.md)** - Development journal, recent updates
- **[HANDOFF.md](docs/HANDOFF.md)** - Architecture patterns, established conventions
- **[ROADMAP.md](docs/ROADMAP.md)** - Planned features, milestones

## File Organization

```
prototype-ui9/
├── src/              # C++ business logic
├── include/          # Headers
├── ui_xml/           # XML component definitions
├── assets/           # Fonts, images
├── scripts/          # Build/screenshot automation
├── docs/             # Documentation
└── Makefile          # Build system
```

## Development Workflow

1. Edit XML for layout changes (no recompilation needed)
2. Edit C++ for logic/subjects changes → `make`
3. Test with `./build/bin/guppy-ui-proto [panel_name]`
4. Screenshot with `./scripts/screenshot.sh` or press 'S' in UI

## Future Integration

This prototype will integrate with main GuppyScreen:
- Replace SDL2 with framebuffer for embedded hardware
- Connect to Moonraker WebSocket for printer data
- Complete remaining panel content
- Add animations and multi-language support
