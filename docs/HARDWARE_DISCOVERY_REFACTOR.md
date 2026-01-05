# Hardware Discovery Architecture Refactor

## Status: PLANNED

This document outlines a proposed refactor to consolidate hardware discovery data into a single, consistent architecture.

## Current State (Problem)

Hardware discovery data is scattered across multiple classes with inconsistent patterns:

| Data | Current Location | Notes |
|------|------------------|-------|
| Heaters | `MoonrakerClient` | `get_heaters()` |
| Fans | `MoonrakerClient` | `get_fans()` |
| Temperature sensors | `MoonrakerClient` | `get_sensors()` |
| LEDs | `MoonrakerClient` | `get_leds()` |
| Hostname | `MoonrakerClient` | `get_hostname()` |
| Printer objects | `MoonrakerClient` | `get_printer_objects()` |
| Capability flags | `PrinterCapabilities` | `has_qgl()`, `has_probe()`, etc. |
| Filament sensors | `PrinterCapabilities` | `get_filament_sensor_names()` |
| AMS type/lanes | `PrinterCapabilities` | `get_mmu_type()`, `get_afc_lane_names()` |
| Macros | `PrinterCapabilities` | `macros()` |

### Problems

1. **MoonrakerClient does too much** - It's both a WebSocket/RPC client AND a hardware data store
2. **Inconsistent querying** - Some code uses `client->get_X()`, other code uses `caps.has_X()`
3. **Duplicate parsing** - `PrinterCapabilities::parse_objects()` parses printer_objects that MoonrakerClient already has
4. **Initialization timing** - Wizard had to work around async race conditions because different data was in different places
5. **No single source of truth** - Wizard steps query MoonrakerClient directly, other code uses PrinterCapabilities

## Proposed Architecture

### Layer 1: MoonrakerClient (Connection Only)
```
MoonrakerClient
├── WebSocket connection management
├── RPC method calls
├── Status subscriptions
└── Raw JSON responses
```

**Responsibility:** Network communication only. No hardware state storage.

### Layer 2: PrinterHardware (Single Source of Truth)
```
PrinterHardware
├── Discovery Data
│   ├── heaters: vector<HeaterInfo>
│   ├── fans: vector<FanInfo>
│   ├── sensors: vector<SensorInfo>
│   ├── leds: vector<LedInfo>
│   ├── filament_sensors: vector<FilamentSensorInfo>
│   ├── ams: AmsInfo (type, lanes, hubs)
│   └── tools: vector<ToolInfo>
│
├── Capability Flags
│   ├── has_qgl, has_z_tilt, has_bed_mesh
│   ├── has_probe, has_accelerometer
│   ├── has_chamber_heater, has_chamber_sensor
│   └── etc.
│
├── Macros
│   ├── all_macros: set<string>
│   ├── helix_macros: set<string>
│   └── Detected common macros (nozzle_clean, purge_line, etc.)
│
└── Metadata
    ├── hostname
    ├── klipper_version
    ├── moonraker_version
    └── mcu_info
```

**Responsibility:** Store ALL discovered hardware data. Single place to query printer capabilities.

### Layer 3: State Managers (Runtime State)
```
FilamentSensorManager  - Runtime state, role assignments, runout detection
AmsState              - Backend selection, lane states, filament info
PrinterState          - Temperatures, positions, print status
```

**Responsibility:** Runtime state and business logic. Initialized FROM PrinterHardware data.

## Migration Plan

### Phase 1: Create PrinterHardware class
- [ ] Create `include/printer_hardware.h` with new unified structure
- [ ] Create `src/printer/printer_hardware.cpp` with discovery parsing
- [ ] Add `PrinterHardware` member to MoonrakerClient
- [ ] Populate PrinterHardware in `discover_printer()` callback

### Phase 2: Migrate MoonrakerClient callers
- [ ] Update wizard steps to use `client->hardware().heaters()` etc.
- [ ] Update PrinterState to use PrinterHardware
- [ ] Update HardwareValidator to use PrinterHardware
- [ ] Deprecate `MoonrakerClient::get_heaters()` etc. with warnings

### Phase 3: Merge PrinterCapabilities into PrinterHardware
- [ ] Move capability flag logic from PrinterCapabilities to PrinterHardware
- [ ] Move macro detection to PrinterHardware
- [ ] Update all PrinterCapabilities users to use PrinterHardware
- [ ] Delete PrinterCapabilities class

### Phase 4: Simplify init_subsystems_from_capabilities
- [ ] Rename to `init_state_managers_from_hardware()`
- [ ] Take `PrinterHardware&` instead of multiple params
- [ ] Remove need for separate MoonrakerAPI/Client params

### Phase 5: Cleanup
- [ ] Remove deprecated methods from MoonrakerClient
- [ ] Remove old includes and forward declarations
- [ ] Update documentation

## Data Structures

```cpp
// New unified structures
struct HeaterInfo {
    std::string name;           // e.g., "extruder", "heater_bed"
    std::string type;           // "extruder", "heater_bed", "heater_generic"
    std::string sensor_type;    // Temperature sensor type
};

struct FanInfo {
    std::string name;           // e.g., "fan", "heater_fan hotend"
    std::string type;           // "fan", "heater_fan", "controller_fan"
    bool is_pwm;
};

struct AmsInfo {
    AmsType type;               // NONE, AFC, HAPPY_HARE, VALGACE, TOOL_CHANGER
    std::vector<std::string> lane_names;
    std::vector<std::string> hub_names;
    std::vector<std::string> tool_names;  // For tool changers
    int total_slots;
};

// Main class
class PrinterHardware {
public:
    // Parse from Moonraker discovery
    void parse_discovery(const json& printer_objects, const json& printer_info);

    // Hardware queries
    const std::vector<HeaterInfo>& heaters() const;
    const std::vector<FanInfo>& fans() const;
    const std::vector<SensorInfo>& sensors() const;
    const std::vector<LedInfo>& leds() const;
    const std::vector<FilamentSensorInfo>& filament_sensors() const;
    const AmsInfo& ams() const;

    // Capability queries (convenience)
    bool has_qgl() const;
    bool has_probe() const;
    bool has_led() const;
    bool has_mmu() const;
    // ... etc.

    // Macro queries
    bool has_macro(const std::string& name) const;
    const std::unordered_set<std::string>& macros() const;
};
```

## Benefits

1. **Single source of truth** - All hardware data in one place
2. **Clear separation** - Network client vs. data storage vs. state management
3. **Simpler initialization** - State managers init from one object
4. **Consistent API** - `hardware.heaters()` instead of mix of `client->get_X()` and `caps.has_X()`
5. **Easier testing** - Mock PrinterHardware instead of multiple classes

## Files Affected

### New Files
- `include/printer_hardware.h`
- `src/printer/printer_hardware.cpp`

### Major Changes
- `include/moonraker_client.h` - Add PrinterHardware member, deprecate old methods
- `src/api/moonraker_client.cpp` - Populate PrinterHardware in discovery
- `src/printer/printer_capabilities.cpp` - Eventually delete

### Updates (use new API)
- `src/ui/ui_wizard_*.cpp` - All wizard steps
- `src/printer/printer_state.cpp`
- `src/printer/hardware_validator.cpp`
- `src/application/application.cpp`
- Many panel files that query heaters/fans/etc.

## Timeline Estimate

- Phase 1: 2-3 hours (create new class, parallel implementation)
- Phase 2: 2-3 hours (migrate callers incrementally)
- Phase 3: 1-2 hours (merge PrinterCapabilities)
- Phase 4: 30 mins (simplify init function)
- Phase 5: 30 mins (cleanup)

**Total: ~6-9 hours**

## Related Issues

- Wizard filament sensor step race condition (fixed with quick workaround)
- Wizard AMS step race condition (fixed with quick workaround)
- `init_subsystems_from_capabilities()` needs api+client params (architectural smell)
