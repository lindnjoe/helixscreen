// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_console.h"

#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_panel_common.h"
#include "ui_subject_registry.h"
#include "ui_theme.h"

#include "app_globals.h"
#include "moonraker_client.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

// Forward declaration for row click callback (registered in init_subjects)
static void on_console_row_clicked(lv_event_t* e);

ConsolePanel::ConsolePanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    std::snprintf(status_buf_, sizeof(status_buf_), "Loading history...");
}

void ConsolePanel::init_subjects() {
    if (subjects_initialized_) {
        spdlog::warn("[{}] init_subjects() called twice - ignoring", get_name());
        return;
    }

    // Initialize status subject for reactive binding
    UI_SUBJECT_INIT_AND_REGISTER_STRING(status_subject_, status_buf_, status_buf_,
                                        "console_status");

    // Register row click callback for opening from Advanced panel
    lv_xml_register_event_cb(nullptr, "on_console_row_clicked", on_console_row_clicked);

    subjects_initialized_ = true;
    spdlog::debug("[{}] init_subjects() - registered row click callback", get_name());
}

void ConsolePanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    spdlog::info("[{}] Setting up...", get_name());

    // Use standard overlay panel setup (wires header, back button, handles responsive padding)
    ui_overlay_panel_setup_standard(panel_, parent_screen_, "overlay_header", "overlay_content");

    // Find widget references
    lv_obj_t* overlay_content = lv_obj_find_by_name(panel_, "overlay_content");
    if (overlay_content) {
        console_container_ = lv_obj_find_by_name(overlay_content, "console_container");
        empty_state_ = lv_obj_find_by_name(overlay_content, "empty_state");
        status_label_ = lv_obj_find_by_name(overlay_content, "status_message");
    }

    if (!console_container_) {
        spdlog::error("[{}] console_container not found!", get_name());
        return;
    }

    // Fetch initial history
    fetch_history();

    spdlog::info("[{}] Setup complete!", get_name());
}

void ConsolePanel::on_activate() {
    spdlog::debug("[{}] Panel activated", get_name());
    // Refresh history when panel becomes visible
    fetch_history();
}

void ConsolePanel::on_deactivate() {
    spdlog::debug("[{}] Panel deactivated", get_name());
}

void ConsolePanel::fetch_history() {
    MoonrakerClient* client = get_moonraker_client();
    if (!client) {
        spdlog::warn("[{}] No MoonrakerClient available", get_name());
        std::snprintf(status_buf_, sizeof(status_buf_), "Not connected to printer");
        lv_subject_copy_string(&status_subject_, status_buf_);
        update_visibility();
        return;
    }

    // Update status while loading
    std::snprintf(status_buf_, sizeof(status_buf_), "Loading...");
    lv_subject_copy_string(&status_subject_, status_buf_);

    // Request gcode history from Moonraker
    client->get_gcode_store(
        FETCH_COUNT,
        [this](const std::vector<MoonrakerClient::GcodeStoreEntry>& entries) {
            spdlog::info("[{}] Received {} gcode entries", get_name(), entries.size());

            // Convert to our entry format
            std::vector<GcodeEntry> converted;
            converted.reserve(entries.size());

            for (const auto& entry : entries) {
                GcodeEntry e;
                e.message = entry.message;
                e.timestamp = entry.time;
                e.type = (entry.type == "command") ? GcodeEntry::Type::COMMAND
                                                   : GcodeEntry::Type::RESPONSE;
                e.is_error = is_error_message(entry.message);
                converted.push_back(e);
            }

            populate_entries(converted);
        },
        [this](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to fetch gcode store: {}", get_name(), err.message);
            std::snprintf(status_buf_, sizeof(status_buf_), "Failed to load history");
            lv_subject_copy_string(&status_subject_, status_buf_);
            update_visibility();
        });
}

void ConsolePanel::populate_entries(const std::vector<GcodeEntry>& entries) {
    clear_entries();

    // Store entries (already oldest-first from API)
    for (const auto& entry : entries) {
        entries_.push_back(entry);

        // Enforce max size (remove oldest)
        if (entries_.size() > MAX_ENTRIES) {
            entries_.pop_front();
        }
    }

    // Create widgets for each entry
    for (const auto& entry : entries_) {
        create_entry_widget(entry);
    }

    // Update visibility and scroll to bottom
    update_visibility();
    scroll_to_bottom();
}

void ConsolePanel::create_entry_widget(const GcodeEntry& entry) {
    if (!console_container_) {
        return;
    }

    // Create a simple label (not XML component for better performance with many entries)
    lv_obj_t* label = lv_label_create(console_container_);
    lv_label_set_text(label, entry.message.c_str());
    lv_obj_set_width(label, LV_PCT(100));

    // Apply color based on entry type (same pattern as ui_icon.cpp)
    lv_color_t color;
    if (entry.is_error) {
        color = ui_theme_get_color("error_color");
    } else if (entry.type == GcodeEntry::Type::RESPONSE) {
        color = ui_theme_get_color("success_color");
    } else {
        // Commands use primary text color
        color = UI_COLOR_TEXT_PRIMARY;
    }
    lv_obj_set_style_text_color(label, color, 0);

    // Use small font from theme (responsive)
    lv_obj_set_style_text_font(label, UI_FONT_SMALL, 0);
}

void ConsolePanel::clear_entries() {
    entries_.clear();

    if (console_container_) {
        lv_obj_clean(console_container_);
    }
}

void ConsolePanel::scroll_to_bottom() {
    if (console_container_) {
        lv_obj_scroll_to_y(console_container_, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

bool ConsolePanel::is_error_message(const std::string& message) {
    if (message.empty()) {
        return false;
    }

    // Klipper errors typically start with "!!" or "Error:"
    if (message.size() >= 2 && message[0] == '!' && message[1] == '!') {
        return true;
    }

    // Case-insensitive check for "error" at start
    if (message.size() >= 5) {
        std::string lower = message.substr(0, 5);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == "error") {
            return true;
        }
    }

    return false;
}

void ConsolePanel::update_visibility() {
    bool has_entries = !entries_.empty();

    // Toggle visibility: show console OR empty state
    if (console_container_) {
        if (has_entries) {
            lv_obj_remove_flag(console_container_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(console_container_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (empty_state_) {
        if (has_entries) {
            lv_obj_add_flag(empty_state_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(empty_state_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update status message
    if (has_entries) {
        std::snprintf(status_buf_, sizeof(status_buf_), "%zu entries", entries_.size());
    } else {
        std::snprintf(status_buf_, sizeof(status_buf_), "");
    }
    lv_subject_copy_string(&status_subject_, status_buf_);
}

// ============================================================================
// Global Instance and Row Click Handler
// ============================================================================

static std::unique_ptr<ConsolePanel> g_console_panel;
static lv_obj_t* g_console_panel_obj = nullptr;

ConsolePanel& get_global_console_panel() {
    if (!g_console_panel) {
        spdlog::error("get_global_console_panel() called before initialization!");
        throw std::runtime_error("ConsolePanel not initialized");
    }
    return *g_console_panel;
}

void init_global_console_panel(PrinterState& printer_state, MoonrakerAPI* api) {
    if (g_console_panel) {
        spdlog::warn("ConsolePanel already initialized, skipping");
        return;
    }
    g_console_panel = std::make_unique<ConsolePanel>(printer_state, api);
    spdlog::debug("ConsolePanel initialized");
}

/**
 * @brief Row click handler for opening console from Advanced panel
 *
 * Registered via lv_xml_register_event_cb() in init_subjects().
 * Lazy-creates the console panel on first click.
 */
static void on_console_row_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("[Console] Console row clicked");

    if (!g_console_panel) {
        spdlog::error("[Console] Global instance not initialized!");
        return;
    }

    // Lazy-create the console panel
    if (!g_console_panel_obj) {
        spdlog::debug("[Console] Creating console panel...");
        g_console_panel_obj = static_cast<lv_obj_t*>(
            lv_xml_create(lv_display_get_screen_active(NULL), "console_panel", nullptr));

        if (g_console_panel_obj) {
            g_console_panel->setup(g_console_panel_obj, lv_display_get_screen_active(NULL));
            lv_obj_add_flag(g_console_panel_obj, LV_OBJ_FLAG_HIDDEN);
            spdlog::info("[Console] Panel created and setup complete");
        } else {
            spdlog::error("[Console] Failed to create console_panel");
            return;
        }
    }

    // Show the overlay
    ui_nav_push_overlay(g_console_panel_obj);
}
