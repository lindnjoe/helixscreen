# MoonrakerClientMock - Remaining Work

**Last Updated:** 2025-11-30

## Overview

The mock client is functional for all current UI testing needs. These are optional enhancements for completeness.

---

## Low Priority (Nice-to-Have)

### 1. Extrusion Position Tracking
**Status:** Not implemented

**G-codes:**
- `G92 E0` - Set extruder position to 0
- `G0/G1` with E parameter - Extrude during moves

**Implementation needed:**
- Parse E value from G92 and G0/G1 commands
- Maintain `extruder_position_` atomic variable

---

### 2. Input Shaper Configuration
**Status:** STUB - logged but not tracked

**G-code:** `SET_INPUT_SHAPER AXIS=X FREQ=60 DAMPING=0.1`

**Implementation needed:**
- Parse AXIS, FREQ, DAMPING parameters
- Store per-axis configuration

---

### 3. Pressure Advance Simulation
**Status:** STUB - logged but not tracked

**G-code:** `SET_PRESSURE_ADVANCE ADVANCE=0.05 EXTRUDER=extruder`

**Implementation needed:**
- Parse ADVANCE and EXTRUDER parameters
- Store per-extruder values

---

### 4. printer.objects.query Callback
**Status:** Not implemented

Currently unimplemented - would return current printer state snapshot.

---

## Already Implemented (Reference)

These features are fully working in the mock client:

- ✅ Temperature simulation (extruder + bed with realistic heating/cooling)
- ✅ Motion & positioning (X/Y/Z, G90/G91, G28 homing)
- ✅ Print state machine (standby/printing/paused/complete/cancelled/error)
- ✅ Hardware discovery (7 printer presets)
- ✅ Bed mesh simulation (BED_MESH_CALIBRATE, profiles)
- ✅ Speed/flow factor oscillation
- ✅ `server.files.list` with callback
- ✅ `server.files.metadata` with callback and thumbnail extraction
- ✅ SET_LED control with RGB/HSV parsing
- ✅ Fan control (M106/M107, SET_FAN_SPEED) - multi-fan support
- ✅ Z offset tracking (SET_GCODE_OFFSET Z=, Z_ADJUST=)
- ✅ RESTART/FIRMWARE_RESTART simulation with klippy_state transitions

---

## Not Planned

These are unlikely to be needed:

- **PROBE/QGL/Z_TILT_ADJUST** - Complex state machines, rarely used in UI
