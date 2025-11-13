// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WIZARD_CONFIG_PATHS_H
#define WIZARD_CONFIG_PATHS_H

/**
 * @file wizard_config_paths.h
 * @brief Centralized configuration paths for wizard screens
 *
 * Defines all JSON configuration paths used by wizard screens to eliminate
 * hardcoded string literals and reduce typo risk.
 */

namespace WizardConfigPaths {
    // Printer identification
    constexpr const char* DEFAULT_PRINTER = "/default_printer";
    constexpr const char* PRINTER_NAME = "/printer/name";
    constexpr const char* PRINTER_TYPE = "/printer/type";

    // Bed hardware
    constexpr const char* BED_HEATER = "/printer/bed_heater";
    constexpr const char* BED_SENSOR = "/printer/bed_sensor";

    // Hotend hardware
    constexpr const char* HOTEND_HEATER = "/printer/hotend_heater";
    constexpr const char* HOTEND_SENSOR = "/printer/hotend_sensor";

    // Fan hardware
    constexpr const char* HOTEND_FAN = "/printer/hotend_fan";
    constexpr const char* PART_FAN = "/printer/part_fan";

    // LED hardware
    constexpr const char* LED_STRIP = "/printer/led_strip";

    // Network configuration
    constexpr const char* MOONRAKER_HOST = "/moonraker_host";
    constexpr const char* MOONRAKER_PORT = "/moonraker_port";
    constexpr const char* WIFI_SSID = "/wifi/ssid";
    constexpr const char* WIFI_PASSWORD = "/wifi/password";
}

#endif // WIZARD_CONFIG_PATHS_H
