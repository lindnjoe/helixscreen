# Session Handoff Document

**Last Updated:** 2025-10-27
**Current Focus:** Wizard screens implementation

---

## 🎯 Active Work & Next Priorities

### Wizard Step 0 (WiFi Setup) - COMPLETE ✅

**Status:** WiFi setup screen layout fixed and functional

**Completed:**
- ✅ Fixed critical flex layout height requirements (both cards visible)
- ✅ Created custom `<ui_switch>` widget (wraps lv_switch C API)
- ✅ Platform abstraction (macOS mock, Linux WiFi/Ethernet detection)
- ✅ Reactive subjects for WiFi/Ethernet status
- ✅ Comprehensive documentation of flex layout patterns

### Next Priorities

1. Review wizard Step 3 (Printer Identify) - roller widget visibility
2. Continue wizard screen UI/UX review and fixes
3. Implement WiFi toggle event handler (currently non-functional)
4. Hardware detection/mapping screens
5. Integration tests for wizard flow

---

## 📋 Critical Architecture Patterns (Essential How-To Reference)

### Pattern #0: Flex Layout Height Requirements 🚨 CRITICAL

**When using `flex_grow` on children, parent MUST have explicit height:**

```xml
<!-- BROKEN: Parent has no height -->
<lv_obj flex_flow="row">
    <lv_obj flex_grow="3">Left (30%)</lv_obj>
    <lv_obj flex_grow="7">Right (70%)</lv_obj>
</lv_obj>
<!-- Result: Columns collapse to 0 height -->

<!-- CORRECT: Two-column pattern (30/70 split) -->
<view height="100%" flex_flow="column">
    <lv_obj width="100%" flex_grow="1" flex_flow="column">
        <lv_obj width="100%" flex_grow="1" flex_flow="row">
            <!-- BOTH columns MUST have height="100%" -->
            <lv_obj flex_grow="3" height="100%"
                    flex_flow="column" scrollable="true" scroll_dir="VER">
                <lv_obj height="100">Card 1</lv_obj>
                <lv_obj height="100">Card 2</lv_obj>
            </lv_obj>
            <lv_obj flex_grow="7" height="100%"
                    scrollable="true" scroll_dir="VER">
                <!-- Content -->
            </lv_obj>
        </lv_obj>
    </lv_obj>
</view>
```

**Critical Checks:**
1. Parent has explicit height (`height="300"`, `height="100%"`, or `flex_grow="1"`)
2. ALL columns have `height="100%"` (row height = tallest child)
3. Every level has sizing (wrapper → row → columns)
4. Cards use fixed heights (`height="100"`), NOT `LV_SIZE_CONTENT` in nested flex

**Diagnostic:** Add `style_bg_color="#ff0000"` to visualize bounds

**Reference:** `docs/LVGL9_XML_GUIDE.md:634-716`, `.claude/agents/widget-maker.md:107-149`, `.claude/agents/ui-reviewer.md:101-152`

### Pattern #1: Custom Switch Widget

**Available:** `<ui_switch>` registered for XML use

```xml
<ui_switch name="my_toggle" checked="true"/>
<ui_switch orientation="horizontal"/>  <!-- auto|horizontal|vertical -->
```

**Supports:** All standard `lv_obj` properties (width, height, style_*)

**Files:** `include/ui_switch.h`, `src/ui_switch.cpp`, `src/main.cpp:643`

### Pattern #2: Navigation History Stack

**When to use:** Overlay panels (motion, temps, extrusion, keypad)

```cpp
ui_nav_push_overlay(motion_panel);  // Shows overlay, saves history
if (!ui_nav_go_back()) { /* fallback */ }
```

**Files:** `ui_nav.h:54-62`, `ui_nav.cpp:250-327`

### Pattern #3: Global Keyboard for Textareas

```cpp
// One-time init in main.cpp (already done)
ui_keyboard_init(lv_screen_active());

// For each textarea
ui_keyboard_register_textarea(my_textarea);  // Auto show/hide on focus
```

**Files:** `include/ui_keyboard.h`, `src/ui_keyboard.cpp`

### Pattern #4: Subject Initialization Order

**MUST initialize subjects BEFORE creating XML:**

```cpp
lv_xml_component_register_from_file("A:/ui_xml/my_panel.xml");
ui_my_panel_init_subjects();  // FIRST
lv_xml_create(screen, "my_panel", NULL);  // AFTER
```

### Pattern #5: Component Instantiation Names

**Always add explicit `name` attributes:**

```xml
<!-- WRONG --><my_panel/>
<!-- CORRECT --><my_panel name="my_panel"/>
```

**Why:** Component `<view name="...">` doesn't propagate to instantiation

### Pattern #6: Image Scaling in Flex Layouts

```cpp
lv_obj_update_layout(container);  // Force layout calculation FIRST
ui_image_scale_to_cover(img, container);
```

**Why:** LVGL uses deferred layout - containers report 0x0 until forced

**Files:** `ui_utils.cpp:213-276`, `ui_panel_print_status.cpp:249-314`

### Pattern #7: Logging Policy

**ALWAYS use spdlog, NEVER printf/cout/LV_LOG:**

```cpp
#include <spdlog/spdlog.h>
spdlog::info("Operation complete: {}", value);  // fmt-style formatting
spdlog::error("Failed: {}", (int)enum_val);     // Cast enums
```

**Reference:** `CLAUDE.md:77-134`

### Pattern #8: Copyright Headers

**ALL new files MUST include GPL v3 header**

**Reference:** `docs/COPYRIGHT_HEADERS.md`

---

## 🔧 Known Issues & Gotchas

### LVGL 9 XML Flag Syntax ✅ FIXED

**NEVER use `flag_` prefix:**
- ❌ `flag_hidden="true"` → ✅ `hidden="true"`
- ❌ `flag_clickable="true"` → ✅ `clickable="true"`

**Status:** All XML files fixed (2025-10-24)

### LV_SIZE_CONTENT in Nested Flex

**Problem:** Evaluates to 0 before `lv_obj_update_layout()` is called

**Solutions:**
1. Call `lv_obj_update_layout()` after creation (timing sensitive)
2. Use explicit pixel dimensions (recommended)
3. Use `style_min_height`/`style_min_width` for cards

**Reference:** `docs/LVGL9_XML_GUIDE.md:705-708`

---

**Rule:** When work is complete, REMOVE it from HANDOFF immediately. Keep this document lean and current.
