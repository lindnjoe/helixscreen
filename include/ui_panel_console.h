// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_panel_base.h"

#include "lvgl.h"
#include "moonraker_api.h"
#include "printer_state.h"

#include <deque>
#include <string>
#include <vector>

/**
 * @file ui_panel_console.h
 * @brief G-code console panel with command history display
 *
 * Displays a read-only scrollable history of G-code commands and responses
 * from Moonraker's gcode_store endpoint. Uses color-coded output to
 * distinguish commands from responses and errors.
 *
 * ## Features (Phase 1)
 * - Read-only command history display
 * - Color-coded output (errors red, responses green)
 * - Auto-scroll to newest messages (terminal-style)
 * - Empty state when no history available
 *
 * ## Moonraker API
 * - GET /server/gcode_store - Fetch command history
 *
 * ## Future Enhancements (Phase 2)
 * - Real-time updates via notify_gcode_response WebSocket
 * - G-code input field with on-screen keyboard
 * - Temperature message filtering
 *
 * @see docs/FEATURE_STATUS.md for implementation progress
 */
class ConsolePanel : public PanelBase {
  public:
    /**
     * @brief Construct ConsolePanel
     * @param printer_state Reference to global printer state
     * @param api Pointer to MoonrakerAPI (may be nullptr in test mode)
     */
    ConsolePanel(PrinterState& printer_state, MoonrakerAPI* api);

    // PanelBase interface
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;
    [[nodiscard]] const char* get_name() const override {
        return "Console";
    }
    [[nodiscard]] const char* get_xml_component_name() const override {
        return "console_panel";
    }
    void init_subjects() override;

    // Lifecycle hooks
    void on_activate() override;
    void on_deactivate() override;

  private:
    /**
     * @brief Entry in the console history
     */
    struct GcodeEntry {
        std::string message;    ///< The G-code command or response text
        double timestamp = 0.0; ///< Unix timestamp from Moonraker
        enum class Type {
            COMMAND, ///< User-entered G-code command
            RESPONSE ///< Klipper response (ok, error, info)
        } type = Type::COMMAND;
        bool is_error = false; ///< True if response contains error (!! prefix)
    };

    /**
     * @brief Fetch initial history from gcode_store
     *
     * Calls Moonraker's server.gcode_store JSON-RPC method to retrieve
     * the most recent command history.
     */
    void fetch_history();

    /**
     * @brief Populate the console with fetched entries
     *
     * Clears any existing entries and creates text widgets for each
     * entry in the history.
     *
     * @param entries Vector of gcode entries from API (oldest first)
     */
    void populate_entries(const std::vector<GcodeEntry>& entries);

    /**
     * @brief Create a single console line widget
     *
     * Creates a text_small label with appropriate color based on entry type:
     * - Commands: primary text color
     * - Success responses: success color (green)
     * - Error responses: error color (red)
     *
     * @param entry The gcode entry to display
     */
    void create_entry_widget(const GcodeEntry& entry);

    /**
     * @brief Clear all console entries
     *
     * Removes all child widgets from console_container_.
     */
    void clear_entries();

    /**
     * @brief Scroll console to bottom (newest entries)
     *
     * Called after populating entries to ensure most recent
     * content is visible (terminal-style scrolling).
     */
    void scroll_to_bottom();

    /**
     * @brief Check if a response message indicates an error
     *
     * Moonraker/Klipper errors typically start with "!!" or contain
     * "error" in the message.
     *
     * @param message Response message text
     * @return true if message is an error
     */
    static bool is_error_message(const std::string& message);

    /**
     * @brief Update UI visibility based on entry count
     *
     * Shows console_container_ if entries exist, otherwise shows
     * empty_state_. Updates status message accordingly.
     */
    void update_visibility();

    // Widget references
    lv_obj_t* console_container_ = nullptr; ///< Scrollable container for entries
    lv_obj_t* empty_state_ = nullptr;       ///< Shown when no entries
    lv_obj_t* status_label_ = nullptr;      ///< Status message label

    // Data
    std::deque<GcodeEntry> entries_;           ///< History buffer
    static constexpr size_t MAX_ENTRIES = 200; ///< Maximum entries to display
    static constexpr int FETCH_COUNT = 100;    ///< Number of entries to fetch

    // Subjects
    bool subjects_initialized_ = false;
    char status_buf_[128] = {};
    lv_subject_t status_subject_{};
};

/**
 * @brief Get the global ConsolePanel instance
 *
 * Returns reference to singleton. Must call init_global_console_panel() first.
 *
 * @return Reference to singleton ConsolePanel
 * @throws std::runtime_error if not initialized
 */
ConsolePanel& get_global_console_panel();

/**
 * @brief Initialize the global ConsolePanel instance
 *
 * Must be called by main.cpp before accessing get_global_console_panel().
 *
 * @param printer_state Reference to PrinterState
 * @param api Pointer to MoonrakerAPI
 */
void init_global_console_panel(PrinterState& printer_state, MoonrakerAPI* api);
