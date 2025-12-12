// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/**
 * @file moonraker_api_internal.h
 * @brief Internal helpers shared across MoonrakerAPI implementation files
 *
 * This header is NOT part of the public API. It provides validation and
 * utility functions used by the split moonraker_api_*.cpp implementation files.
 */

#include "moonraker_api.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace moonraker_internal {

/**
 * @brief Validate that a string contains only safe identifier characters
 *
 * Allows alphanumeric, underscore, and space (for names like "heater_generic chamber").
 * Rejects newlines, semicolons, and other G-code control characters.
 *
 * @param str String to validate
 * @return true if safe, false otherwise
 */
inline bool is_safe_identifier(const std::string& str) {
    if (str.empty()) {
        return false;
    }

    return std::all_of(str.begin(), str.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ' ';
    });
}

/**
 * @brief Validate that a file path is safe from directory traversal attacks
 *
 * Rejects paths containing:
 * - Parent directory references (..)
 * - Absolute paths (starting with /)
 * - Null bytes (path truncation attack)
 * - Windows-style absolute paths (C:, D:, etc)
 * - Suspicious characters (<>|*?)
 *
 * @param path File path to validate
 * @return true if safe relative path, false otherwise
 */
inline bool is_safe_path(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    if (path.find("..") != std::string::npos) {
        return false;
    }

    if (path[0] == '/') {
        return false;
    }

    if (path.find('\0') != std::string::npos) {
        return false;
    }

    if (path.size() >= 2 && path[1] == ':') {
        return false;
    }

    const std::string dangerous_chars = "<>|*?";
    if (path.find_first_of(dangerous_chars) != std::string::npos) {
        return false;
    }

    for (char c : path) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate that an axis character is valid
 *
 * @param axis Axis character to validate
 * @return true if valid axis (X, Y, Z, E), false otherwise
 */
inline bool is_valid_axis(char axis) {
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(axis)));
    return upper == 'X' || upper == 'Y' || upper == 'Z' || upper == 'E';
}

/**
 * @brief Validate temperature is in safe range
 *
 * @param temp Temperature in Celsius
 * @param limits Safety limits configuration
 * @return true if within configured range, false otherwise
 */
inline bool is_safe_temperature(double temp, const SafetyLimits& limits) {
    return temp >= limits.min_temperature_celsius && temp <= limits.max_temperature_celsius;
}

/**
 * @brief Validate fan speed is in valid percentage range
 *
 * @param speed Speed percentage
 * @param limits Safety limits configuration
 * @return true if within configured range, false otherwise
 */
inline bool is_safe_fan_speed(double speed, const SafetyLimits& limits) {
    return speed >= limits.min_fan_speed_percent && speed <= limits.max_fan_speed_percent;
}

/**
 * @brief Validate feedrate is within safe limits
 *
 * @param feedrate Feedrate in mm/min
 * @param limits Safety limits configuration
 * @return true if within configured range, false otherwise
 */
inline bool is_safe_feedrate(double feedrate, const SafetyLimits& limits) {
    return feedrate >= limits.min_feedrate_mm_min && feedrate <= limits.max_feedrate_mm_min;
}

/**
 * @brief Validate distance is reasonable for axis movement
 *
 * @param distance Distance in mm
 * @param limits Safety limits configuration
 * @return true if within configured range, false otherwise
 */
inline bool is_safe_distance(double distance, const SafetyLimits& limits) {
    return distance >= limits.min_relative_distance_mm &&
           distance <= limits.max_relative_distance_mm;
}

/**
 * @brief Validate position is reasonable for axis positioning
 *
 * @param position Position in mm
 * @param limits Safety limits configuration
 * @return true if within configured range, false otherwise
 */
inline bool is_safe_position(double position, const SafetyLimits& limits) {
    return position >= limits.min_absolute_position_mm &&
           position <= limits.max_absolute_position_mm;
}

} // namespace moonraker_internal
