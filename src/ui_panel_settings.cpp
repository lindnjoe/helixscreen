// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_settings.h"

#include "app_globals.h"
#include "config.h"
#include "moonraker_client.h"
#include "printer_state.h"
#include "settings_manager.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_panel_bed_mesh.h"
#include "ui_panel_calibration_zoffset.h"

#include <spdlog/spdlog.h>

#include <memory>

// Version string (could come from build system)
#ifndef HELIX_VERSION
#define HELIX_VERSION "1.0.0-dev"
#endif

// Forward declarations for class-based API
class BedMeshPanel;
BedMeshPanel& get_global_bed_mesh_panel();
ZOffsetCalibrationPanel& get_global_zoffset_cal_panel();
MoonrakerClient* get_moonraker_client();

// ============================================================================
// CONSTRUCTOR
// ============================================================================

SettingsPanel::SettingsPanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    spdlog::trace("[{}] Constructor", get_name());
}

// ============================================================================
// PANELBASE IMPLEMENTATION
// ============================================================================

void SettingsPanel::init_subjects() {
    if (subjects_initialized_) {
        spdlog::warn("[{}] init_subjects() called twice - ignoring", get_name());
        return;
    }

    // Initialize SettingsManager subjects (for reactive binding)
    SettingsManager::instance().init_subjects();

    // Initialize bed mesh panel subjects (may be lazily created later)
    get_global_bed_mesh_panel().init_subjects();

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized", get_name());
}

void SettingsPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    // Setup all handlers and bindings
    setup_toggle_handlers();
    setup_dropdown();
    setup_action_handlers();
    populate_info_rows();

    spdlog::info("[{}] Setup complete", get_name());
}

// ============================================================================
// SETUP HELPERS
// ============================================================================

void SettingsPanel::setup_toggle_handlers() {
    auto& settings = SettingsManager::instance();

    // === Dark Mode Toggle ===
    lv_obj_t* dark_mode_row = lv_obj_find_by_name(panel_, "row_dark_mode");
    if (dark_mode_row) {
        dark_mode_switch_ = lv_obj_find_by_name(dark_mode_row, "toggle");
        if (dark_mode_switch_) {
            // Set initial state from SettingsManager
            if (settings.get_dark_mode()) {
                lv_obj_add_state(dark_mode_switch_, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(dark_mode_switch_, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(dark_mode_switch_, on_dark_mode_changed, LV_EVENT_VALUE_CHANGED,
                                this);
            spdlog::debug("[{}]   ✓ Dark mode toggle", get_name());
        }
    }

    // === LED Light Toggle ===
    lv_obj_t* led_light_row = lv_obj_find_by_name(panel_, "row_led_light");
    if (led_light_row) {
        led_light_switch_ = lv_obj_find_by_name(led_light_row, "toggle");
        if (led_light_switch_) {
            // LED state from SettingsManager (ephemeral, starts off)
            if (settings.get_led_enabled()) {
                lv_obj_add_state(led_light_switch_, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(led_light_switch_, on_led_light_changed, LV_EVENT_VALUE_CHANGED,
                                this);
            spdlog::debug("[{}]   ✓ LED light toggle", get_name());
        }
    }

    // === Sounds Toggle (placeholder) ===
    lv_obj_t* sounds_row = lv_obj_find_by_name(panel_, "row_sounds");
    if (sounds_row) {
        sounds_switch_ = lv_obj_find_by_name(sounds_row, "toggle");
        if (sounds_switch_) {
            if (settings.get_sounds_enabled()) {
                lv_obj_add_state(sounds_switch_, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(sounds_switch_, on_sounds_changed, LV_EVENT_VALUE_CHANGED, this);
            spdlog::debug("[{}]   ✓ Sounds toggle", get_name());
        }
    }

    // === Completion Alert Toggle ===
    lv_obj_t* completion_row = lv_obj_find_by_name(panel_, "row_completion_alert");
    if (completion_row) {
        completion_alert_switch_ = lv_obj_find_by_name(completion_row, "toggle");
        if (completion_alert_switch_) {
            if (settings.get_completion_alert()) {
                lv_obj_add_state(completion_alert_switch_, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(completion_alert_switch_, on_completion_alert_changed,
                                LV_EVENT_VALUE_CHANGED, this);
            spdlog::debug("[{}]   ✓ Completion alert toggle", get_name());
        }
    }
}

void SettingsPanel::setup_dropdown() {
    auto& settings = SettingsManager::instance();

    lv_obj_t* sleep_row = lv_obj_find_by_name(panel_, "row_display_sleep");
    if (sleep_row) {
        display_sleep_dropdown_ = lv_obj_find_by_name(sleep_row, "dropdown");
        if (display_sleep_dropdown_) {
            // Set dropdown options
            lv_dropdown_set_options(display_sleep_dropdown_,
                                    SettingsManager::get_display_sleep_options());

            // Set current selection
            int current_sleep = settings.get_display_sleep_sec();
            int index = SettingsManager::sleep_seconds_to_index(current_sleep);
            lv_dropdown_set_selected(display_sleep_dropdown_, index);

            // Wire up change handler
            lv_obj_add_event_cb(display_sleep_dropdown_, on_display_sleep_changed,
                                LV_EVENT_VALUE_CHANGED, this);

            spdlog::debug("[{}]   ✓ Display sleep dropdown ({}s = index {})", get_name(),
                          current_sleep, index);
        }
    }
}

void SettingsPanel::setup_action_handlers() {
    // === Bed Mesh Row ===
    bed_mesh_row_ = lv_obj_find_by_name(panel_, "row_bed_mesh");
    if (bed_mesh_row_) {
        lv_obj_add_event_cb(bed_mesh_row_, on_bed_mesh_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Bed mesh action row", get_name());
    }

    // === Z-Offset Row ===
    z_offset_row_ = lv_obj_find_by_name(panel_, "row_z_offset");
    if (z_offset_row_) {
        lv_obj_add_event_cb(z_offset_row_, on_z_offset_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Z-offset action row", get_name());
    }

    // === PID Tuning Row ===
    pid_tuning_row_ = lv_obj_find_by_name(panel_, "row_pid_tuning");
    if (pid_tuning_row_) {
        lv_obj_add_event_cb(pid_tuning_row_, on_pid_tuning_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ PID tuning action row", get_name());
    }

    // === Network Row ===
    network_row_ = lv_obj_find_by_name(panel_, "row_network");
    if (network_row_) {
        lv_obj_add_event_cb(network_row_, on_network_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Network action row", get_name());
    }

    // === Factory Reset Row ===
    factory_reset_row_ = lv_obj_find_by_name(panel_, "row_factory_reset");
    if (factory_reset_row_) {
        lv_obj_add_event_cb(factory_reset_row_, on_factory_reset_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Factory reset action row", get_name());
    }
}

void SettingsPanel::populate_info_rows() {
    // === Version ===
    lv_obj_t* version_row = lv_obj_find_by_name(panel_, "row_version");
    if (version_row) {
        version_value_ = lv_obj_find_by_name(version_row, "value");
        if (version_value_) {
            lv_label_set_text(version_value_, HELIX_VERSION);
            spdlog::debug("[{}]   ✓ Version: {}", get_name(), HELIX_VERSION);
        }
    }

    // === Printer Name (from PrinterState or Config) ===
    lv_obj_t* printer_row = lv_obj_find_by_name(panel_, "row_printer");
    if (printer_row) {
        printer_value_ = lv_obj_find_by_name(printer_row, "value");
        if (printer_value_) {
            // Try to get printer name from config
            Config* config = Config::get_instance();
            std::string printer_name =
                config->get<std::string>(config->df() + "printer_name", "Unknown");
            lv_label_set_text(printer_value_, printer_name.c_str());
            spdlog::debug("[{}]   ✓ Printer: {}", get_name(), printer_name);
        }
    }

    // === Klipper Version (would come from Moonraker query) ===
    lv_obj_t* klipper_row = lv_obj_find_by_name(panel_, "row_klipper");
    if (klipper_row) {
        klipper_value_ = lv_obj_find_by_name(klipper_row, "value");
        if (klipper_value_) {
            // Placeholder - would be updated after Moonraker connection
            lv_label_set_text(klipper_value_, "—");
            spdlog::debug("[{}]   ✓ Klipper version placeholder", get_name());
        }
    }
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void SettingsPanel::handle_dark_mode_changed(bool enabled) {
    spdlog::info("[{}] Dark mode toggled: {}", get_name(), enabled ? "ON" : "OFF");
    SettingsManager::instance().set_dark_mode(enabled);
}

void SettingsPanel::handle_display_sleep_changed(int index) {
    int seconds = SettingsManager::index_to_sleep_seconds(index);
    spdlog::info("[{}] Display sleep changed: index {} = {}s", get_name(), index, seconds);
    SettingsManager::instance().set_display_sleep_sec(seconds);
}

void SettingsPanel::handle_led_light_changed(bool enabled) {
    spdlog::info("[{}] LED light toggled: {}", get_name(), enabled ? "ON" : "OFF");
    SettingsManager::instance().set_led_enabled(enabled);
}

void SettingsPanel::handle_sounds_changed(bool enabled) {
    spdlog::info("[{}] Sounds toggled: {} (placeholder)", get_name(), enabled ? "ON" : "OFF");
    SettingsManager::instance().set_sounds_enabled(enabled);
}

void SettingsPanel::handle_completion_alert_changed(bool enabled) {
    spdlog::info("[{}] Completion alert toggled: {}", get_name(), enabled ? "ON" : "OFF");
    SettingsManager::instance().set_completion_alert(enabled);
}

void SettingsPanel::handle_bed_mesh_clicked() {
    spdlog::debug("[{}] Bed Mesh clicked - opening visualization", get_name());

    // Create bed mesh panel on first access (lazy initialization)
    if (!bed_mesh_panel_ && parent_screen_) {
        spdlog::debug("[{}] Creating bed mesh visualization panel...", get_name());

        // Create from XML
        bed_mesh_panel_ =
            static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "bed_mesh_panel", nullptr));
        if (bed_mesh_panel_) {
            // Setup event handlers and renderer (class-based API)
            get_global_bed_mesh_panel().setup(bed_mesh_panel_, parent_screen_);

            // Initially hidden
            lv_obj_add_flag(bed_mesh_panel_, LV_OBJ_FLAG_HIDDEN);
            spdlog::info("[{}] Bed mesh visualization panel created", get_name());
        } else {
            spdlog::error("[{}] Failed to create bed mesh panel from XML", get_name());
            return;
        }
    }

    // Push bed mesh panel onto navigation history and show it
    if (bed_mesh_panel_) {
        ui_nav_push_overlay(bed_mesh_panel_);
    }
}

void SettingsPanel::handle_z_offset_clicked() {
    spdlog::debug("[{}] Z-Offset clicked - opening calibration panel", get_name());

    // Create Z-Offset calibration panel on first access (lazy initialization)
    if (!zoffset_cal_panel_ && parent_screen_) {
        spdlog::debug("[{}] Creating Z-Offset calibration panel...", get_name());

        // Create from XML
        zoffset_cal_panel_ = static_cast<lv_obj_t*>(
            lv_xml_create(parent_screen_, "calibration_zoffset_panel", nullptr));
        if (zoffset_cal_panel_) {
            // Setup event handlers (class-based API)
            MoonrakerClient* client = get_moonraker_client();
            get_global_zoffset_cal_panel().setup(zoffset_cal_panel_, parent_screen_, client);

            // Initially hidden
            lv_obj_add_flag(zoffset_cal_panel_, LV_OBJ_FLAG_HIDDEN);
            spdlog::info("[{}] Z-Offset calibration panel created", get_name());
        } else {
            spdlog::error("[{}] Failed to create Z-Offset panel from XML", get_name());
            return;
        }
    }

    // Push Z-Offset panel onto navigation history and show it
    if (zoffset_cal_panel_) {
        ui_nav_push_overlay(zoffset_cal_panel_);
    }
}

void SettingsPanel::handle_pid_tuning_clicked() {
    spdlog::debug("[{}] PID Tuning clicked (not yet implemented)", get_name());
    // TODO: Open PID tuning panel
}

void SettingsPanel::handle_network_clicked() {
    spdlog::debug("[{}] Network clicked (not yet implemented)", get_name());
    // TODO: Open network settings panel
}

void SettingsPanel::handle_factory_reset_clicked() {
    spdlog::debug("[{}] Factory Reset clicked (not yet implemented)", get_name());
    // TODO: Show confirmation dialog, then reset config
}

// ============================================================================
// STATIC TRAMPOLINES
// ============================================================================

void SettingsPanel::on_dark_mode_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_dark_mode_changed");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self && self->dark_mode_switch_) {
        bool enabled = lv_obj_has_state(self->dark_mode_switch_, LV_STATE_CHECKED);
        self->handle_dark_mode_changed(enabled);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_display_sleep_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_display_sleep_changed");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self && self->display_sleep_dropdown_) {
        int index = lv_dropdown_get_selected(self->display_sleep_dropdown_);
        self->handle_display_sleep_changed(index);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_led_light_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_led_light_changed");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self && self->led_light_switch_) {
        bool enabled = lv_obj_has_state(self->led_light_switch_, LV_STATE_CHECKED);
        self->handle_led_light_changed(enabled);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_sounds_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_sounds_changed");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self && self->sounds_switch_) {
        bool enabled = lv_obj_has_state(self->sounds_switch_, LV_STATE_CHECKED);
        self->handle_sounds_changed(enabled);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_completion_alert_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_completion_alert_changed");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self && self->completion_alert_switch_) {
        bool enabled = lv_obj_has_state(self->completion_alert_switch_, LV_STATE_CHECKED);
        self->handle_completion_alert_changed(enabled);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_bed_mesh_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_bed_mesh_clicked");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_bed_mesh_clicked();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_z_offset_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_z_offset_clicked");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_z_offset_clicked();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_pid_tuning_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_pid_tuning_clicked");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_pid_tuning_clicked();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_network_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_network_clicked");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_network_clicked();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void SettingsPanel::on_factory_reset_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_factory_reset_clicked");
    auto* self = static_cast<SettingsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_factory_reset_clicked();
    }
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

static std::unique_ptr<SettingsPanel> g_settings_panel;

SettingsPanel& get_global_settings_panel() {
    if (!g_settings_panel) {
        g_settings_panel = std::make_unique<SettingsPanel>(get_printer_state(), nullptr);
    }
    return *g_settings_panel;
}
