// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/**
 * @file wizard_config_paths.h
 * @brief Centralized configuration paths for wizard screens
 *
 * Defines all JSON configuration paths used by wizard screens to eliminate
 * hardcoded string literals and reduce typo risk.
 *
 * All printer-specific paths are under /printer/...
 */

namespace helix {
namespace wizard {
// Printer identification
constexpr const char* PRINTER_NAME = "/printer/name";
constexpr const char* PRINTER_TYPE = "/printer/type";

// Bed hardware
constexpr const char* BED_HEATER = "/printer/heater/bed";
constexpr const char* BED_SENSOR = "/printer/sensor/bed";

// Hotend hardware
constexpr const char* HOTEND_HEATER = "/printer/heater/hotend";
constexpr const char* HOTEND_SENSOR = "/printer/sensor/hotend";

// Fan hardware
constexpr const char* HOTEND_FAN = "/printer/fan/hotend";
constexpr const char* PART_FAN = "/printer/fan/part";

// LED hardware
constexpr const char* LED_STRIP = "/printer/led/strip";

// Network configuration
constexpr const char* MOONRAKER_HOST = "/printer/moonraker_host";
constexpr const char* MOONRAKER_PORT = "/printer/moonraker_port";
constexpr const char* WIFI_SSID = "/wifi/ssid";
constexpr const char* WIFI_PASSWORD = "/wifi/password";
} // namespace wizard
} // namespace helix
