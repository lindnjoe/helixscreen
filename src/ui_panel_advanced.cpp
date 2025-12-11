// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_advanced.h"

#include "ui_nav.h"
#include "ui_timelapse_settings.h"
#include "ui_toast.h"

#include "moonraker_api.h"
#include "moonraker_client.h"
#include "printer_capabilities.h"
#include "printer_state.h"

#include <spdlog/spdlog.h>

// Forward declarations
MoonrakerClient* get_moonraker_client();

// Global instance (singleton pattern matching SettingsPanel)
static std::unique_ptr<AdvancedPanel> g_advanced_panel;

AdvancedPanel& get_global_advanced_panel() {
    // Should be initialized by main.cpp before use
    if (!g_advanced_panel) {
        spdlog::error("get_global_advanced_panel() called before initialization!");
        throw std::runtime_error("AdvancedPanel not initialized");
    }
    return *g_advanced_panel;
}

// Called by main.cpp to initialize the global instance
void init_global_advanced_panel(PrinterState& printer_state, MoonrakerAPI* api) {
    g_advanced_panel = std::make_unique<AdvancedPanel>(printer_state, api);
}

// ============================================================================
// CONSTRUCTOR
// ============================================================================

AdvancedPanel::AdvancedPanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    spdlog::trace("[{}] Constructor", get_name());
}

// ============================================================================
// PANELBASE IMPLEMENTATION
// ============================================================================

void AdvancedPanel::init_subjects() {
    // No local subjects - capability subjects are owned by PrinterState
    subjects_initialized_ = true;
}

void AdvancedPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    setup_action_handlers();
    spdlog::info("[{}] Setup complete", get_name());
}

void AdvancedPanel::on_activate() {
    spdlog::debug("[{}] Activated", get_name());
}

// ============================================================================
// SETUP HELPERS
// ============================================================================

void AdvancedPanel::setup_action_handlers() {
    // === Input Shaping Row ===
    input_shaping_row_ = lv_obj_find_by_name(panel_, "row_input_shaping");
    if (input_shaping_row_) {
        lv_obj_add_event_cb(input_shaping_row_, on_input_shaping_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Input shaping action row", get_name());
    }

    // === Machine Limits Row ===
    machine_limits_row_ = lv_obj_find_by_name(panel_, "row_machine_limits");
    if (machine_limits_row_) {
        lv_obj_add_event_cb(machine_limits_row_, on_machine_limits_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Machine limits action row", get_name());
    }

    // === Spoolman Row ===
    spoolman_row_ = lv_obj_find_by_name(panel_, "row_spoolman");
    if (spoolman_row_) {
        lv_obj_add_event_cb(spoolman_row_, on_spoolman_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Spoolman action row", get_name());
    }

    // === Macros Row ===
    macros_row_ = lv_obj_find_by_name(panel_, "row_macros");
    if (macros_row_) {
        lv_obj_add_event_cb(macros_row_, on_macros_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Macros action row", get_name());
    }

    // === Restart Row ===
    restart_row_ = lv_obj_find_by_name(panel_, "row_restart");
    if (restart_row_) {
        lv_obj_add_event_cb(restart_row_, on_restart_clicked, LV_EVENT_CLICKED, this);
        spdlog::debug("[{}]   ✓ Restart action row", get_name());
    }
}

// ============================================================================
// NAVIGATION HANDLERS
// ============================================================================

void AdvancedPanel::handle_input_shaping_clicked() {
    spdlog::debug("[{}] Input Shaping clicked", get_name());

    MoonrakerClient* client = get_moonraker_client();
    if (client && client->capabilities().has_klippain_shaketune()) {
        ui_toast_show(ToastSeverity::INFO, "Input Shaping: Klippain Shake&Tune detected", 2000);
    } else {
        ui_toast_show(ToastSeverity::INFO, "Input Shaping: Coming soon", 2000);
    }
}

void AdvancedPanel::handle_machine_limits_clicked() {
    spdlog::debug("[{}] Machine Limits clicked", get_name());
    ui_toast_show(ToastSeverity::INFO, "Machine Limits: Coming soon", 2000);
}

void AdvancedPanel::handle_spoolman_clicked() {
    spdlog::debug("[{}] Spoolman clicked", get_name());
    ui_toast_show(ToastSeverity::INFO, "Spoolman: Coming soon", 2000);
}

void AdvancedPanel::handle_macros_clicked() {
    spdlog::debug("[{}] Macros clicked", get_name());

    MoonrakerClient* client = get_moonraker_client();
    if (client) {
        size_t macro_count = client->capabilities().macro_count();
        std::string msg = "Macros: " + std::to_string(macro_count) + " available";
        ui_toast_show(ToastSeverity::INFO, msg.c_str(), 2000);
    }
}

void AdvancedPanel::handle_restart_clicked() {
    spdlog::debug("[{}] Restart clicked", get_name());
    ui_toast_show(ToastSeverity::INFO, "Restart: Coming soon", 2000);
}

// ============================================================================
// STATIC EVENT TRAMPOLINES
// ============================================================================

void AdvancedPanel::on_input_shaping_clicked(lv_event_t* e) {
    auto* self = static_cast<AdvancedPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_input_shaping_clicked();
}

void AdvancedPanel::on_machine_limits_clicked(lv_event_t* e) {
    auto* self = static_cast<AdvancedPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_machine_limits_clicked();
}

void AdvancedPanel::on_spoolman_clicked(lv_event_t* e) {
    auto* self = static_cast<AdvancedPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_spoolman_clicked();
}

void AdvancedPanel::on_macros_clicked(lv_event_t* e) {
    auto* self = static_cast<AdvancedPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_macros_clicked();
}

void AdvancedPanel::on_restart_clicked(lv_event_t* e) {
    auto* self = static_cast<AdvancedPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_restart_clicked();
}
