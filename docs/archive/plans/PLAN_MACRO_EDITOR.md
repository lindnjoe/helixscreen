# Plan: Macro Editor UI

> **Status:** Deferred - saved for future implementation

## Summary

Add a **MACROS** section to the settings panel allowing users to edit the 3 built-in macros (load_filament, unload_filament, cooldown) via a multi-line G-code editor overlay.

## Prerequisites

- Filament panel must read macros from config (currently hardcoded)
- Context-aware keyboard Enter key for multi-line editing

---

## Implementation Plan

### Phase 1: Context-Aware Keyboard Enter Key

**Files to modify:**
- `include/ui_keyboard.h` - Add multi-line mode flag API
- `src/ui_keyboard.cpp` - Modify `keyboard_event_cb()` to check mode

**Changes:**
```cpp
// New API in ui_keyboard.h
void ui_keyboard_set_multiline_mode(lv_obj_t* textarea, bool multiline);

// In keyboard_event_cb() around line 643
// Check if textarea is in multiline mode before closing on Enter
```

**Behavior:**
- Default: Enter closes keyboard (existing behavior)
- Multiline mode: Enter inserts `\n`, textarea stays focused
- Macro editor registers textarea with multiline mode enabled

---

### Phase 2: Macro Editor Overlay

**New files:**
- `ui_xml/macro_editor_overlay.xml` - Modal layout
- `include/ui_macro_editor.h` - Editor class header
- `src/ui_macro_editor.cpp` - Editor implementation

**XML Structure:**
```xml
<view name="macro_editor_overlay">
  <!-- Header with macro name -->
  <lv_label name="macro_title" text="Edit Macro"/>

  <!-- Multi-line G-code textarea -->
  <lv_textarea name="macro_gcode"
               one_line="false"
               max_length="1000"
               placeholder="Enter G-code commands..."/>

  <!-- Help text -->
  <text_small text="One command per line"/>

  <!-- Save/Cancel buttons -->
  <lv_obj flex_flow="row">
    <lv_button name="btn_cancel">Cancel</lv_button>
    <lv_button name="btn_save">Save</lv_button>
  </lv_obj>
</view>
```

**MacroEditor class:**
```cpp
class MacroEditor {
public:
    // Open editor for specific macro
    void open(const std::string& macro_key, const std::string& display_name);

private:
    void on_save();   // Write to config, close overlay
    void on_cancel(); // Discard changes, close overlay

    std::string current_macro_key_;  // e.g., "load_filament"
    lv_obj_t* overlay_ = nullptr;
    lv_obj_t* textarea_ = nullptr;
};
```

---

### Phase 3: Settings Panel MACROS Section

**Files to modify:**
- `ui_xml/settings_panel.xml` - Add MACROS section
- `src/ui_panel_settings.cpp` - Wire up action rows
- `include/ui_panel_settings.h` - Add members

**XML additions (after NOTIFICATIONS section):**
```xml
<!-- MACROS section -->
<settings_section name="macros_section">
  <settings_section_header label="MACROS"/>

  <settings_action_row name="macro_load_filament"
                       label="Load Filament"
                       sublabel="LOAD_FILAMENT"/>

  <settings_action_row name="macro_unload_filament"
                       label="Unload Filament"
                       sublabel="UNLOAD_FILAMENT"/>

  <settings_action_row name="macro_cooldown"
                       label="Cooldown"
                       sublabel="2 commands"/>
</settings_section>
```

**Sublabel logic:**
- If macro is a single command: show the command (truncated if long)
- If multi-line: show "N commands"

---

### Phase 4: Fix Filament Panel to Use Config Macros

**Files to modify:**
- `src/ui_panel_filament.cpp` - Read G-code from config instead of hardcoding

**Current (hardcoded):**
```cpp
api_->execute_gcode("LOAD_FILAMENT", success_cb, error_cb);
```

**New (config-driven):**
```cpp
Config* cfg = Config::get_instance();
std::string gcode = cfg->get<std::string>(
    cfg->df() + "default_macros/load_filament",
    "LOAD_FILAMENT"  // fallback
);
api_->execute_gcode(gcode, success_cb, error_cb);
```

**Apply to:**
- `handle_load_filament_click()` (line ~301)
- `handle_unload_filament_click()` (line ~322)

---

## File Summary

| File | Action | Purpose |
|------|--------|---------|
| `ui_xml/macro_editor_overlay.xml` | Create | Modal layout for G-code editing |
| `include/ui_macro_editor.h` | Create | MacroEditor class header |
| `src/ui_macro_editor.cpp` | Create | MacroEditor implementation |
| `ui_xml/settings_panel.xml` | Modify | Add MACROS section |
| `src/ui_panel_settings.cpp` | Modify | Wire up macro action rows |
| `include/ui_panel_settings.h` | Modify | Add MacroEditor member |
| `src/ui_panel_filament.cpp` | Modify | Read macros from config |
| `include/ui_keyboard.h` | Modify | Add multiline mode API |
| `src/ui_keyboard.cpp` | Modify | Context-aware Enter key |

---

## Testing Plan

1. **Keyboard multiline mode**: Test Enter inserts newline in macro editor, closes keyboard in WiFi password modal
2. **Macro editor UI**: Open each macro, verify G-code displays correctly, edit and save
3. **Config persistence**: Edit macro, restart app, verify change persisted
4. **Filament panel integration**: Edit load_filament macro, go to filament panel, verify new G-code executes
5. **Edge cases**: Empty macro, very long macro (>500 chars), special characters
