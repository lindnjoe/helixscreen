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

#include "ui_wizard_hotend_select.h"
#include "ui_wizard.h"
#include "app_globals.h"
#include "config.h"
#include "moonraker_client.h"
#include "lvgl/lvgl.h"
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <cstring>

// ============================================================================
// Static Data & Subjects
// ============================================================================

// Subject declarations (module scope)
static lv_subject_t hotend_heater_selected;
static lv_subject_t hotend_sensor_selected;

// Screen instance
static lv_obj_t* hotend_select_screen_root = nullptr;

// Dynamic options storage (for event callback mapping)
static std::vector<std::string> hotend_heater_items;
static std::vector<std::string> hotend_sensor_items;

// ============================================================================
// Forward Declarations
// ============================================================================

static void on_hotend_heater_changed(lv_event_t* e);
static void on_hotend_sensor_changed(lv_event_t* e);

// ============================================================================
// Subject Initialization
// ============================================================================

void ui_wizard_hotend_select_init_subjects() {
    spdlog::debug("[Wizard Hotend] Initializing subjects");

    // Initialize subjects with default index 0
    // Actual selection will be restored from config during create() after hardware is discovered
    lv_subject_init_int(&hotend_heater_selected, 0);
    lv_xml_register_subject(nullptr, "hotend_heater_selected", &hotend_heater_selected);

    lv_subject_init_int(&hotend_sensor_selected, 0);
    lv_xml_register_subject(nullptr, "hotend_sensor_selected", &hotend_sensor_selected);

    spdlog::info("[Wizard Hotend] Subjects initialized");
}

// ============================================================================
// Event Callbacks
// ============================================================================

static void on_hotend_heater_changed(lv_event_t* e) {
    lv_obj_t* dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint16_t selected_index = lv_dropdown_get_selected(dropdown);

    spdlog::debug("[Wizard Hotend] Heater selection changed to index: {}", selected_index);

    // Update subject (config will be saved in cleanup when leaving screen)
    lv_subject_set_int(&hotend_heater_selected, selected_index);
}

static void on_hotend_sensor_changed(lv_event_t* e) {
    lv_obj_t* dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint16_t selected_index = lv_dropdown_get_selected(dropdown);

    spdlog::debug("[Wizard Hotend] Sensor selection changed to index: {}", selected_index);

    // Update subject (config will be saved in cleanup when leaving screen)
    lv_subject_set_int(&hotend_sensor_selected, selected_index);
}

// ============================================================================
// Callback Registration
// ============================================================================

void ui_wizard_hotend_select_register_callbacks() {
    spdlog::debug("[Wizard Hotend] Registering callbacks");

    lv_xml_register_event_cb(nullptr, "on_hotend_heater_changed", on_hotend_heater_changed);
    lv_xml_register_event_cb(nullptr, "on_hotend_sensor_changed", on_hotend_sensor_changed);
}

// ============================================================================
// Screen Creation
// ============================================================================

lv_obj_t* ui_wizard_hotend_select_create(lv_obj_t* parent) {
    spdlog::info("[Wizard Hotend] Creating hotend select screen");

    // Safety check: cleanup should have been called by wizard navigation
    if (hotend_select_screen_root) {
        spdlog::warn("[Wizard Hotend] Screen pointer not null - cleanup may not have been called properly");
        hotend_select_screen_root = nullptr;  // Reset pointer, wizard framework handles deletion
    }

    // Create screen from XML
    hotend_select_screen_root = (lv_obj_t*)lv_xml_create(parent, "wizard_hotend_select", nullptr);
    if (!hotend_select_screen_root) {
        spdlog::error("[Wizard Hotend] Failed to create screen from XML");
        return nullptr;
    }

    // Get Moonraker client for hardware discovery
    MoonrakerClient* client = get_moonraker_client();

    // Build hotend heater options from discovered hardware
    hotend_heater_items.clear();
    std::string heater_options_str;

    if (client) {
        const auto& heaters = client->get_heaters();
        for (const auto& heater : heaters) {
            // Filter for extruder-related heaters
            if (heater.find("extruder") != std::string::npos) {
                hotend_heater_items.push_back(heater);
                if (!heater_options_str.empty()) heater_options_str += "\n";
                heater_options_str += heater;
            }
        }
    }

    // Always add "None" option
    hotend_heater_items.push_back("None");
    if (!heater_options_str.empty()) heater_options_str += "\n";
    heater_options_str += "None";

    // Build hotend sensor options from discovered hardware
    hotend_sensor_items.clear();
    std::string sensor_options_str;

    if (client) {
        const auto& sensors = client->get_sensors();
        // For hotend sensors, filter for extruder/hotend-related sensors
        for (const auto& sensor : sensors) {
            if (sensor.find("extruder") != std::string::npos ||
                sensor.find("hotend") != std::string::npos) {
                hotend_sensor_items.push_back(sensor);
                if (!sensor_options_str.empty()) sensor_options_str += "\n";
                sensor_options_str += sensor;
            }
        }
    }

    // Always add "None" option
    hotend_sensor_items.push_back("None");
    if (!sensor_options_str.empty()) sensor_options_str += "\n";
    sensor_options_str += "None";

    // Get config for restoring saved selections
    Config* config = Config::get_instance();

    // Find and configure heater dropdown
    lv_obj_t* heater_dropdown = lv_obj_find_by_name(hotend_select_screen_root, "hotend_heater_dropdown");
    if (heater_dropdown) {
        lv_dropdown_set_options(heater_dropdown, heater_options_str.c_str());

        // Restore saved selection OR use guessing as fallback
        int selected_index = 0;  // Default to first option
        if (config) {
            std::string saved_heater = config->get<std::string>("/printer/hotend_heater", "");
            if (!saved_heater.empty()) {
                // Search for saved name in discovered hardware
                for (size_t i = 0; i < hotend_heater_items.size(); i++) {
                    if (hotend_heater_items[i] == saved_heater) {
                        selected_index = i;
                        spdlog::debug("[Wizard Hotend] Restored heater selection: {}", saved_heater);
                        break;
                    }
                }
            } else if (client) {
                // No saved config, use guessing method
                std::string guessed = client->guess_hotend_heater();
                if (!guessed.empty()) {
                    // Search for guessed name in dropdown items
                    for (size_t i = 0; i < hotend_heater_items.size(); i++) {
                        if (hotend_heater_items[i] == guessed) {
                            selected_index = i;
                            spdlog::info("[Wizard Hotend] Auto-selected hotend heater: {}", guessed);
                            break;
                        }
                    }
                }
            }
        }

        lv_dropdown_set_selected(heater_dropdown, selected_index);
        lv_subject_set_int(&hotend_heater_selected, selected_index);
        spdlog::debug("[Wizard Hotend] Configured heater dropdown with {} options, selected: {}",
                     hotend_heater_items.size(), selected_index);
    }

    // Find and configure sensor dropdown
    lv_obj_t* sensor_dropdown = lv_obj_find_by_name(hotend_select_screen_root, "hotend_sensor_dropdown");
    if (sensor_dropdown) {
        lv_dropdown_set_options(sensor_dropdown, sensor_options_str.c_str());

        // Restore saved selection OR use guessing as fallback
        int selected_index = 0;  // Default to first option
        if (config) {
            std::string saved_sensor = config->get<std::string>("/printer/hotend_sensor", "");
            if (!saved_sensor.empty()) {
                // Search for saved name in discovered hardware
                for (size_t i = 0; i < hotend_sensor_items.size(); i++) {
                    if (hotend_sensor_items[i] == saved_sensor) {
                        selected_index = i;
                        spdlog::debug("[Wizard Hotend] Restored sensor selection: {}", saved_sensor);
                        break;
                    }
                }
            } else if (client) {
                // No saved config, use guessing method
                std::string guessed = client->guess_hotend_sensor();
                if (!guessed.empty()) {
                    // Search for guessed name in dropdown items
                    for (size_t i = 0; i < hotend_sensor_items.size(); i++) {
                        if (hotend_sensor_items[i] == guessed) {
                            selected_index = i;
                            spdlog::info("[Wizard Hotend] Auto-selected hotend sensor: {}", guessed);
                            break;
                        }
                    }
                }
            }
        }

        lv_dropdown_set_selected(sensor_dropdown, selected_index);
        lv_subject_set_int(&hotend_sensor_selected, selected_index);
        spdlog::debug("[Wizard Hotend] Configured sensor dropdown with {} options, selected: {}",
                     hotend_sensor_items.size(), selected_index);
    }

    spdlog::info("[Wizard Hotend] Screen created successfully");
    return hotend_select_screen_root;
}

// ============================================================================
// Cleanup
// ============================================================================

void ui_wizard_hotend_select_cleanup() {
    spdlog::debug("[Wizard Hotend] Cleaning up resources");

    // Save current selections to config before cleanup (deferred save pattern)
    Config* config = Config::get_instance();
    if (config) {
        // Get heater selection index and save NAME (not index)
        int heater_index = lv_subject_get_int(&hotend_heater_selected);
        if (heater_index >= 0 && heater_index < static_cast<int>(hotend_heater_items.size())) {
            const std::string& heater_name = hotend_heater_items[heater_index];
            config->set("/printer/hotend_heater", heater_name);
            spdlog::info("[Wizard Hotend] Saved hotend heater: {}", heater_name);
        }

        // Get sensor selection index and save NAME (not index)
        int sensor_index = lv_subject_get_int(&hotend_sensor_selected);
        if (sensor_index >= 0 && sensor_index < static_cast<int>(hotend_sensor_items.size())) {
            const std::string& sensor_name = hotend_sensor_items[sensor_index];
            config->set("/printer/hotend_sensor", sensor_name);
            spdlog::info("[Wizard Hotend] Saved hotend sensor: {}", sensor_name);
        }

        // Persist to disk
        config->save();
    }

    // Reset UI references
    // Note: Do NOT call lv_obj_del() here - the wizard framework handles
    // object deletion when clearing wizard_content container
    hotend_select_screen_root = nullptr;

    spdlog::info("[Wizard Hotend] Cleanup complete");
}

// ============================================================================
// Validation
// ============================================================================

bool ui_wizard_hotend_select_is_validated() {
    // Always return true for baseline implementation
    return true;
}