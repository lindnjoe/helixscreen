# Motion Panel Moonraker API Wiring Plan

**Status:** Planned (not yet implemented)
**Created:** 2025-11-26
**Priority:** Phase 2.1/2.2 in Moonraker wiring roadmap

## Overview

Wire the motion panel controls (homing, jog, Z-axis buttons) to real Moonraker API calls and add position feedback from PrinterState.

## Current State

- **Homing buttons**: Wired to `ui_panel_motion_home()` which is a **placeholder** (only updates local display)
- **Jog pad**: Wired to `ui_panel_motion_jog()` which is a **placeholder**
- **Z buttons**: Wired to `z_button_cb` which only updates local position
- **Position display**: Uses local subjects (`motion_pos_x/y/z`) NOT connected to PrinterState
- **API methods**: `home_axes()`, `move_axis()` exist and are fully implemented in `moonraker_api.cpp`

### Key Gap

PrinterState receives position updates from Moonraker (`position_x_`, `position_y_`, `position_z_` subjects) BUT the motion panel uses separate local subjects that are NOT wired to PrinterState. This needs to be bridged.

## Recommended Approach: Hybrid

Wire API calls now with minimal changes, add TODO markers for future RAII refactoring to match `TempControlPanel` pattern.

**Estimated effort:** 2-3 hours
**Files changed:** 2 (`ui_panel_motion.cpp`, `ui_panel_motion.h`)
**Lines changed:** ~145

## Implementation Steps

### Phase 1: Wire API Calls (~30 min)

#### 1.1 Add Required Includes

In `src/ui_panel_motion.cpp`:
```cpp
#include "app_globals.h"        // get_moonraker_api(), get_printer_state()
#include "moonraker_api.h"      // MoonrakerAPI
#include "moonraker_error.h"    // MoonrakerError
#include "ui_error_reporting.h" // NOTIFY_ERROR, NOTIFY_SUCCESS, NOTIFY_WARNING
#include "printer_state.h"      // PrinterState
```

#### 1.2 Wire `ui_panel_motion_jog()`

Replace mock implementation (lines ~301-345) with API call:

```cpp
void ui_panel_motion_jog(jog_direction_t direction, float distance_mm) {
    // Calculate dx/dy from direction
    float dx = 0.0f, dy = 0.0f;
    switch (direction) {
        case JOG_DIR_N:  dy = distance_mm; break;
        case JOG_DIR_S:  dy = -distance_mm; break;
        case JOG_DIR_E:  dx = distance_mm; break;
        case JOG_DIR_W:  dx = -distance_mm; break;
        case JOG_DIR_NE: dx = distance_mm; dy = distance_mm; break;
        case JOG_DIR_NW: dx = -distance_mm; dy = distance_mm; break;
        case JOG_DIR_SE: dx = distance_mm; dy = -distance_mm; break;
        case JOG_DIR_SW: dx = -distance_mm; dy = -distance_mm; break;
    }

    // TODO(refactor): Move API access to class member for RAII pattern
    MoonrakerAPI* api = get_moonraker_api();
    if (!api) {
        NOTIFY_WARNING("Not connected to printer");
        ui_panel_motion_set_position(current_x + dx, current_y + dy, current_z);
        return;
    }

    if (dx != 0.0f) {
        api->move_axis('X', static_cast<double>(dx), 0,
            []() { spdlog::debug("[Motion] X jog complete"); },
            [](const MoonrakerError& err) {
                NOTIFY_ERROR("X jog failed: {}", err.user_message());
            });
    }
    if (dy != 0.0f) {
        api->move_axis('Y', static_cast<double>(dy), 0,
            []() { spdlog::debug("[Motion] Y jog complete"); },
            [](const MoonrakerError& err) {
                NOTIFY_ERROR("Y jog failed: {}", err.user_message());
            });
    }

    // Optimistic local update for responsiveness
    ui_panel_motion_set_position(current_x + dx, current_y + dy, current_z);
}
```

#### 1.3 Wire `ui_panel_motion_home()`

Replace mock implementation (lines ~347-368):

```cpp
void ui_panel_motion_home(char axis) {
    MoonrakerAPI* api = get_moonraker_api();
    if (!api) {
        NOTIFY_WARNING("Not connected to printer");
        // Mock fallback
        switch (axis) {
            case 'X': ui_panel_motion_set_position(0.0f, current_y, current_z); break;
            case 'Y': ui_panel_motion_set_position(current_x, 0.0f, current_z); break;
            case 'Z': ui_panel_motion_set_position(current_x, current_y, 0.0f); break;
            case 'A': ui_panel_motion_set_position(0.0f, 0.0f, 0.0f); break;
        }
        return;
    }

    std::string axes = (axis == 'A') ? "" : std::string(1, axis);
    api->home_axes(axes,
        [axis]() {
            if (axis == 'A') NOTIFY_SUCCESS("All axes homed");
            else NOTIFY_SUCCESS("{} axis homed", axis);
        },
        [](const MoonrakerError& err) {
            NOTIFY_ERROR("Homing failed: {}", err.user_message());
        });
}
```

#### 1.4 Wire Z-axis Buttons

Update `z_button_cb` to use API:

```cpp
float distance = 0.0f;
if (strcmp(name, "z_up_10") == 0) distance = 10.0f;
else if (strcmp(name, "z_up_1") == 0) distance = 1.0f;
else if (strcmp(name, "z_down_1") == 0) distance = -1.0f;
else if (strcmp(name, "z_down_10") == 0) distance = -10.0f;

MoonrakerAPI* api = get_moonraker_api();
if (api) {
    api->move_axis('Z', static_cast<double>(distance), 0,
        []() {},
        [](const MoonrakerError& err) {
            NOTIFY_ERROR("Z jog failed: {}", err.user_message());
        });
}
ui_panel_motion_set_position(current_x, current_y, current_z + distance);
```

### Phase 2: Add Position Observer (~15 min)

#### 2.1 Add Observer Storage

```cpp
// After existing static variables (line ~57)
static lv_observer_t* position_x_observer = nullptr;
static lv_observer_t* position_y_observer = nullptr;
static lv_observer_t* position_z_observer = nullptr;
```

#### 2.2 Add Observer Callbacks

```cpp
static void position_x_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    (void)observer;
    current_x = static_cast<float>(lv_subject_get_int(subject));
    snprintf(pos_x_buf, sizeof(pos_x_buf), "X: %6.1f mm", current_x);
    lv_subject_copy_string(&pos_x_subject, pos_x_buf);
}
// Similar for Y and Z
```

#### 2.3 Register Observers in `ui_panel_motion_setup()`

```cpp
PrinterState& state = get_printer_state();
position_x_observer = lv_subject_add_observer(
    state.get_position_x_subject(), position_x_observer_cb, nullptr);
position_y_observer = lv_subject_add_observer(
    state.get_position_y_subject(), position_y_observer_cb, nullptr);
position_z_observer = lv_subject_add_observer(
    state.get_position_z_subject(), position_z_observer_cb, nullptr);
```

### Phase 3: Add Cleanup Function (~5 min)

```cpp
void ui_panel_motion_cleanup() {
    if (position_x_observer) {
        lv_observer_remove(position_x_observer);
        position_x_observer = nullptr;
    }
    // Similar for Y and Z
}
```

Add declaration to `include/ui_panel_motion.h`.

## Critical Files

| File | Purpose |
|------|---------|
| `src/ui_panel_motion.cpp` | Main implementation - add API calls + observers |
| `include/ui_panel_motion.h` | Add cleanup function declaration |
| `include/moonraker_api.h` | Reference: `home_axes()`, `move_axis()` signatures |
| `src/printer_state.cpp` | Reference: position subject accessors |
| `src/ui_temp_control_panel.cpp` | Reference: RAII class pattern for future refactor |

## Testing Checklist

- [ ] Build succeeds: `make -j`
- [ ] Mock mode: buttons show warning toast, local display still updates
- [ ] Live mode: jog X/Y in all 8 directions
- [ ] Live mode: jog Z up/down (1mm, 10mm)
- [ ] Live mode: home X, Y, Z, All
- [ ] Position display updates from Moonraker feedback
- [ ] Error toasts appear on command failure

## Future Refactoring (TODO)

Convert to `MotionControlPanel` class matching `TempControlPanel` pattern:
- Constructor dependency injection (`PrinterState&`, `MoonrakerAPI*`)
- RAII observer cleanup in destructor
- Remove static globals
- Proper move semantics

This provides testability and consistent architecture across all panels.

## Related

- Phase 1.1/1.2 (Temperature controls): ✅ Complete
- Phase 3.1 (Print start): Pending
- Phase 3.2 (Pause/resume/cancel): Pending
