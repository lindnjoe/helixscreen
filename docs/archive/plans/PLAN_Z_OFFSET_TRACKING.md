# Plan: Z-Offset Adjustment Tracking & Save

## Problem
When users adjust Z-offset during a print (via Print Tuning panel), those adjustments are temporary baby-stepping changes. After the print completes, there's no indication that an adjustment was made, and no easy way to save it permanently.

## Solution
Track Z-offset adjustments made during printing and show a notification in the Controls panel with an option to save when the printer is idle.

---

## Implementation Steps

### Phase 1: Track Pending Z-Offset Delta ✅ COMPLETE

**File: `include/printer_state.h`**
- Add `pending_z_offset_delta_` subject (int, microns)
- Add `z_offset_adjusted_this_session_` flag
- Add public methods:
  - `add_pending_z_offset_delta(int microns)`
  - `get_pending_z_offset_delta() -> int`
  - `clear_pending_z_offset_delta()`
  - `has_pending_z_offset_adjustment() -> bool`

**File: `src/printer_state.cpp`**
- Initialize subject in `init_subjects()`
- Register as `pending_z_offset_delta` for XML binding
- Implement the public methods

### Phase 2: Update Print Tuning to Track Adjustments ✅ COMPLETE

**File: `src/ui_panel_print_status.cpp`**
- In `handle_tune_z_offset_changed()`:
  - Call `PrinterState::instance().add_pending_z_offset_delta(delta_microns)`
  - This accumulates the total adjustment made during the session

### Phase 3: Add Notification Banner to Controls Panel ✅ COMPLETE

**File: `ui_xml/controls_panel.xml`**
- Add a conditional banner above the cards:
```xml
<!-- Z-Offset Adjustment Banner (shown when pending delta != 0) -->
<lv_obj name="z_offset_banner" width="100%" height="content"
        style_bg_color="#info_color" style_bg_opa="40"
        style_radius="#border_radius" style_pad_all="#space_sm"
        flex_flow="row" style_flex_main_place="space_between"
        style_flex_cross_place="center" hidden="true">
  <lv_obj width="content" height="content" style_bg_opa="0"
          style_border_width="0" flex_flow="column" style_pad_gap="2">
    <text_small text="Z-Offset Adjusted" style_text_color="#text_primary"/>
    <text_small name="z_offset_delta_label" bind_text="z_offset_delta_display"
                style_text_color="#text_secondary"/>
  </lv_obj>
  <lv_button name="btn_save_z_offset" height="content"
             style_radius="#border_radius" style_bg_color="#success_color">
    <event_cb trigger="clicked" callback="on_controls_save_z_offset"/>
    <text_small text="Save"/>
  </lv_button>
</lv_obj>
```

**File: `src/ui_panel_controls.cpp`**
- Add observer for `pending_z_offset_delta` subject
- Show/hide banner based on delta != 0
- Format display: "+0.05mm adjusted"
- Disable save button if printer is not idle

### Phase 4: Implement Save Functionality ✅ COMPLETE

**File: `src/ui_panel_controls.cpp`**
- Add `on_controls_save_z_offset` callback:
  - Check printer is idle (not printing)
  - Execute `Z_OFFSET_APPLY_ENDSTOP` G-code (preferred - doesn't restart Klipper)
  - Or fall back to `SAVE_CONFIG` with warning modal
  - On success: clear pending delta, hide banner, show toast

---

## Data Flow

```
[Print Tuning Panel]
    |
    | on_tune_z_offset() -> delta
    v
[PrinterState]
    |
    | add_pending_z_offset_delta(delta)
    | pending_z_offset_delta_ += delta
    v
[Controls Panel]
    |
    | observes pending_z_offset_delta_
    | shows banner if != 0
    v
[Save Button]
    |
    | Z_OFFSET_APPLY_ENDSTOP
    | clear_pending_z_offset_delta()
    v
[Done]
```

---

## G-Code Commands

| Command | Effect | Restart? |
|---------|--------|----------|
| `SET_GCODE_OFFSET Z_ADJUST=0.05` | Temporary offset (current session) | No |
| `Z_OFFSET_APPLY_ENDSTOP` | Apply current offset to endstop position | No |
| `Z_OFFSET_APPLY_PROBE` | Apply current offset to probe z_offset | No |
| `SAVE_CONFIG` | Save all config changes to printer.cfg | **Yes** |

**Preferred:** `Z_OFFSET_APPLY_ENDSTOP` - saves without restart.

---

## Files to Modify

| File | Changes |
|------|---------|
| `include/printer_state.h` | Add pending delta subject and methods |
| `src/printer_state.cpp` | Implement delta tracking |
| `src/ui_panel_print_status.cpp` | Call `add_pending_z_offset_delta()` on adjustment |
| `ui_xml/controls_panel.xml` | Add notification banner |
| `src/ui_panel_controls.cpp` | Add observer, show/hide banner, save callback |

---

## Testing

1. Start a mock print
2. Open Print Tuning, adjust Z-offset several times
3. Close tuning, verify delta accumulates correctly
4. End/cancel print
5. Go to Controls panel, verify banner shows with correct delta
6. Click Save, verify G-code sent and banner hides
7. Verify banner doesn't show when delta is 0

---

## Future Enhancements

- Show toast notification when print ends if delta != 0
- Add "Dismiss" option to ignore adjustment without saving
- Remember adjustment across app restarts (persist to config)
- Global "dirty config" indicator for all unsaved changes (Z-offset, PID, input shaper, etc.)
