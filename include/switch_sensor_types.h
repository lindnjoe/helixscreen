// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace helix::sensors {

/// @brief Role assigned to a switch sensor
///
/// Roles are grouped by functional area with numeric spacing for future expansion.
/// Filament roles (1-9), Probe roles (10-19), Other (20+)
enum class SwitchSensorRole {
    NONE = 0,              ///< Discovered but not assigned to a role
    FILAMENT_RUNOUT = 1,   ///< Primary filament runout detection
    FILAMENT_TOOLHEAD = 2, ///< Toolhead filament detection
    FILAMENT_ENTRY = 3,    ///< Entry point filament detection
    Z_PROBE = 10,          ///< Z probing sensor (maps to "probe")
    DOCK_DETECT = 20,      ///< Dock presence detection
};

/// @brief Type of switch sensor hardware
enum class SwitchSensorType {
    SWITCH = 1, ///< filament_switch_sensor in Klipper
    MOTION = 2, ///< filament_motion_sensor in Klipper (encoder-based)
};

/// @brief Configuration for a switch sensor
struct SwitchSensorConfig {
    std::string klipper_name; ///< Full Klipper name (e.g., "filament_switch_sensor e1")
    std::string sensor_name;  ///< Short name (e.g., "e1")
    SwitchSensorType type = SwitchSensorType::SWITCH;
    SwitchSensorRole role = SwitchSensorRole::NONE;
    bool enabled = true;

    SwitchSensorConfig() = default;

    SwitchSensorConfig(std::string klipper_name_, std::string sensor_name_, SwitchSensorType type_)
        : klipper_name(std::move(klipper_name_)), sensor_name(std::move(sensor_name_)), type(type_) {
    }
};

/// @brief Runtime state for a switch sensor
struct SwitchSensorState {
    bool triggered = false;  ///< filament_detected or probe triggered
    bool enabled = true;     ///< Sensor enabled flag from Klipper
    int detection_count = 0; ///< For motion sensors
    bool available = false;  ///< Sensor available in current config
};

/// @brief Convert role enum to config string
/// @param role The role to convert
/// @return Config-safe string for JSON storage
[[nodiscard]] inline std::string switch_role_to_string(SwitchSensorRole role) {
    switch (role) {
    case SwitchSensorRole::NONE:
        return "none";
    case SwitchSensorRole::FILAMENT_RUNOUT:
        return "filament_runout";
    case SwitchSensorRole::FILAMENT_TOOLHEAD:
        return "filament_toolhead";
    case SwitchSensorRole::FILAMENT_ENTRY:
        return "filament_entry";
    case SwitchSensorRole::Z_PROBE:
        return "z_probe";
    case SwitchSensorRole::DOCK_DETECT:
        return "dock_detect";
    default:
        return "none";
    }
}

/// @brief Parse role string to enum
/// @param str The config string to parse
/// @return Parsed role, or NONE if unrecognized
[[nodiscard]] inline SwitchSensorRole switch_role_from_string(const std::string& str) {
    if (str == "filament_runout")
        return SwitchSensorRole::FILAMENT_RUNOUT;
    if (str == "filament_toolhead")
        return SwitchSensorRole::FILAMENT_TOOLHEAD;
    if (str == "filament_entry")
        return SwitchSensorRole::FILAMENT_ENTRY;
    if (str == "z_probe")
        return SwitchSensorRole::Z_PROBE;
    if (str == "dock_detect")
        return SwitchSensorRole::DOCK_DETECT;
    // Backwards compatibility with old config strings
    if (str == "runout")
        return SwitchSensorRole::FILAMENT_RUNOUT;
    if (str == "toolhead")
        return SwitchSensorRole::FILAMENT_TOOLHEAD;
    if (str == "entry")
        return SwitchSensorRole::FILAMENT_ENTRY;
    return SwitchSensorRole::NONE;
}

/// @brief Convert role to display string
/// @param role The role to convert
/// @return Human-readable role name for UI display
[[nodiscard]] inline std::string switch_role_to_display_string(SwitchSensorRole role) {
    switch (role) {
    case SwitchSensorRole::NONE:
        return "Unassigned";
    case SwitchSensorRole::FILAMENT_RUNOUT:
        return "Runout";
    case SwitchSensorRole::FILAMENT_TOOLHEAD:
        return "Toolhead";
    case SwitchSensorRole::FILAMENT_ENTRY:
        return "Entry";
    case SwitchSensorRole::Z_PROBE:
        return "Z Probe";
    case SwitchSensorRole::DOCK_DETECT:
        return "Dock Detect";
    default:
        return "Unassigned";
    }
}

/// @brief Check if role is a filament-related role
/// @param role The role to check
/// @return true if role is for filament sensing
[[nodiscard]] inline bool is_filament_role(SwitchSensorRole role) {
    return role == SwitchSensorRole::FILAMENT_RUNOUT ||
           role == SwitchSensorRole::FILAMENT_TOOLHEAD || role == SwitchSensorRole::FILAMENT_ENTRY;
}

/// @brief Check if role is a probe-related role
/// @param role The role to check
/// @return true if role is for probing
[[nodiscard]] inline bool is_probe_role(SwitchSensorRole role) {
    return role == SwitchSensorRole::Z_PROBE;
}

/// @brief Convert type enum to config string
/// @param type The type to convert
/// @return Config-safe string
[[nodiscard]] inline std::string switch_type_to_string(SwitchSensorType type) {
    switch (type) {
    case SwitchSensorType::SWITCH:
        return "switch";
    case SwitchSensorType::MOTION:
        return "motion";
    default:
        return "switch";
    }
}

/// @brief Parse type string to enum
/// @param str The config string to parse
/// @return Parsed type, defaults to SWITCH if unrecognized
[[nodiscard]] inline SwitchSensorType switch_type_from_string(const std::string& str) {
    if (str == "motion")
        return SwitchSensorType::MOTION;
    return SwitchSensorType::SWITCH;
}

} // namespace helix::sensors
