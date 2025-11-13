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

#include "ui_wizard_bed_select.h"
#include "ui_wizard.h"
#include "ui_wizard_helpers.h"
#include "wizard_config_paths.h"
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
static lv_subject_t bed_heater_selected;
static lv_subject_t bed_sensor_selected;

// Screen instance
static lv_obj_t* bed_select_screen_root = nullptr;

// Dynamic options storage (for event callback mapping)
static std::vector<std::string> bed_heater_items;
static std::vector<std::string> bed_sensor_items;

// ============================================================================
// Forward Declarations
// ============================================================================

static void on_bed_heater_changed(lv_event_t* e);
static void on_bed_sensor_changed(lv_event_t* e);

// ============================================================================
// Subject Initialization
// ============================================================================

void ui_wizard_bed_select_init_subjects() {
    spdlog::debug("[Wizard Bed] Initializing subjects");

    // Initialize subjects with default index 0
    // Actual selection will be restored from config during create() after hardware is discovered
    WizardHelpers::init_int_subject(&bed_heater_selected, 0, "bed_heater_selected");
    WizardHelpers::init_int_subject(&bed_sensor_selected, 0, "bed_sensor_selected");

    spdlog::info("[Wizard Bed] Subjects initialized");
}

// ============================================================================
// Event Callbacks
// ============================================================================

static void on_bed_heater_changed(lv_event_t* e) {
    lv_obj_t* dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint16_t selected_index = lv_dropdown_get_selected(dropdown);

    spdlog::debug("[Wizard Bed] Heater selection changed to index: {}", selected_index);

    // Update subject (config will be saved in cleanup when leaving screen)
    lv_subject_set_int(&bed_heater_selected, selected_index);
}

static void on_bed_sensor_changed(lv_event_t* e) {
    lv_obj_t* dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint16_t selected_index = lv_dropdown_get_selected(dropdown);

    spdlog::debug("[Wizard Bed] Sensor selection changed to index: {}", selected_index);

    // Update subject (config will be saved in cleanup when leaving screen)
    lv_subject_set_int(&bed_sensor_selected, selected_index);
}

// ============================================================================
// Callback Registration
// ============================================================================

void ui_wizard_bed_select_register_callbacks() {
    spdlog::debug("[Wizard Bed] Registering callbacks");

    lv_xml_register_event_cb(nullptr, "on_bed_heater_changed", on_bed_heater_changed);
    lv_xml_register_event_cb(nullptr, "on_bed_sensor_changed", on_bed_sensor_changed);
}

// ============================================================================
// Screen Creation
// ============================================================================

lv_obj_t* ui_wizard_bed_select_create(lv_obj_t* parent) {
    spdlog::debug("[Wizard Bed] Creating bed select screen");

    // Safety check: screen pointer should be nullptr (cleanup should have been called)
    if (bed_select_screen_root) {
        spdlog::warn("[Wizard Bed] Screen pointer not null - cleanup may not have been called properly");
        bed_select_screen_root = nullptr;  // Reset pointer, wizard framework handles actual deletion
    }

    // Create screen from XML
    bed_select_screen_root = (lv_obj_t*)lv_xml_create(parent, "wizard_bed_select", nullptr);
    if (!bed_select_screen_root) {
        spdlog::error("[Wizard Bed] Failed to create screen from XML");
        return nullptr;
    }

    // Get Moonraker client for hardware discovery
    MoonrakerClient* client = get_moonraker_client();

    // Build bed heater options from discovered hardware
    bed_heater_items.clear();
    if (client) {
        const auto& heaters = client->get_heaters();
        for (const auto& heater : heaters) {
            // Filter for bed-related heaters
            if (heater.find("bed") != std::string::npos) {
                bed_heater_items.push_back(heater);
            }
        }
    }

    // Build dropdown options string with "None" option
    std::string heater_options_str = WizardHelpers::build_dropdown_options(
        bed_heater_items,
        nullptr,  // No additional filter needed (already filtered above)
        true      // Include "None" option
    );

    // Add "None" to items vector to match dropdown
    bed_heater_items.push_back("None");

    // Build bed sensor options from discovered hardware
    bed_sensor_items.clear();
    if (client) {
        // For bed sensors, include all sensors (user can choose chamber, bed, etc.)
        bed_sensor_items = client->get_sensors();
    }

    // Build dropdown options string with "None" option
    std::string sensor_options_str = WizardHelpers::build_dropdown_options(
        bed_sensor_items,
        nullptr,  // No filter - include all sensors
        true      // Include "None" option
    );

    // Add "None" to items vector to match dropdown
    bed_sensor_items.push_back("None");

    // Find and configure heater dropdown
    lv_obj_t* heater_dropdown = lv_obj_find_by_name(bed_select_screen_root, "bed_heater_dropdown");
    if (heater_dropdown) {
        lv_dropdown_set_options(heater_dropdown, heater_options_str.c_str());

        // Restore saved selection with guessing fallback
        WizardHelpers::restore_dropdown_selection(
            heater_dropdown,
            &bed_heater_selected,
            bed_heater_items,
            WizardConfigPaths::BED_HEATER,
            client,
            [](MoonrakerClient* c) { return c->guess_bed_heater(); },
            "[Wizard Bed]"
        );
    }

    // Find and configure sensor dropdown
    lv_obj_t* sensor_dropdown = lv_obj_find_by_name(bed_select_screen_root, "bed_sensor_dropdown");
    if (sensor_dropdown) {
        lv_dropdown_set_options(sensor_dropdown, sensor_options_str.c_str());

        // Restore saved selection with guessing fallback
        WizardHelpers::restore_dropdown_selection(
            sensor_dropdown,
            &bed_sensor_selected,
            bed_sensor_items,
            WizardConfigPaths::BED_SENSOR,
            client,
            [](MoonrakerClient* c) { return c->guess_bed_sensor(); },
            "[Wizard Bed]"
        );
    }

    spdlog::info("[Wizard Bed] Screen created successfully");
    return bed_select_screen_root;
}

// ============================================================================
// Cleanup
// ============================================================================

void ui_wizard_bed_select_cleanup() {
    spdlog::debug("[Wizard Bed] Cleaning up resources");

    // Save current selections to config before cleanup (deferred save pattern)
    WizardHelpers::save_dropdown_selection(
        &bed_heater_selected,
        bed_heater_items,
        WizardConfigPaths::BED_HEATER,
        "[Wizard Bed]"
    );

    WizardHelpers::save_dropdown_selection(
        &bed_sensor_selected,
        bed_sensor_items,
        WizardConfigPaths::BED_SENSOR,
        "[Wizard Bed]"
    );

    // Persist to disk
    Config* config = Config::get_instance();
    if (config) {
        config->save();
    }

    // Reset UI references
    // Note: Do NOT call lv_obj_del() here - the wizard framework handles
    // object deletion when clearing wizard_content container
    bed_select_screen_root = nullptr;

    spdlog::info("[Wizard Bed] Cleanup complete");
}

// ============================================================================
// Validation
// ============================================================================

bool ui_wizard_bed_select_is_validated() {
    // Always return true for baseline implementation
    return true;
}