// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_ams_tool_mapping_overlay.h
 * @brief AMS Tool Mapping sub-panel overlay
 *
 * This overlay allows users to configure tool-to-slot mappings for
 * the AMS system. Each tool (T0, T1, etc.) can be mapped to any
 * available slot.
 *
 * @pattern Overlay (lazy init, singleton)
 * @threading Main thread only
 */

#pragma once

#include "overlay_base.h"

#include <lvgl/lvgl.h>

#include <memory>
#include <vector>

// Forward declarations
class AmsBackend;

namespace helix::ui {

/**
 * @class AmsToolMappingOverlay
 * @brief Overlay for configuring tool-to-slot mappings
 *
 * This overlay displays a list of tools (T0, T1, etc.) and allows
 * the user to select which slot each tool maps to via dropdowns.
 *
 * ## Usage:
 *
 * @code
 * auto& overlay = helix::ui::get_ams_tool_mapping_overlay();
 * if (!overlay.are_subjects_initialized()) {
 *     overlay.init_subjects();
 *     overlay.register_callbacks();
 * }
 * overlay.show(parent_screen);
 * @endcode
 */
class AmsToolMappingOverlay : public OverlayBase {
  public:
    /**
     * @brief Default constructor
     */
    AmsToolMappingOverlay();

    /**
     * @brief Destructor
     */
    ~AmsToolMappingOverlay() override;

    // Non-copyable
    AmsToolMappingOverlay(const AmsToolMappingOverlay&) = delete;
    AmsToolMappingOverlay& operator=(const AmsToolMappingOverlay&) = delete;

    //
    // === OverlayBase Interface ===
    //

    /**
     * @brief Initialize subjects for reactive binding
     *
     * Currently no subjects needed for this overlay.
     */
    void init_subjects() override;

    /**
     * @brief Register event callbacks with lv_xml system
     *
     * No XML callbacks needed - dropdowns use lv_obj_add_event_cb.
     */
    void register_callbacks() override;

    /**
     * @brief Create the overlay UI (called lazily)
     *
     * @param parent Parent widget to attach overlay to (usually screen)
     * @return Root object of overlay, or nullptr on failure
     */
    lv_obj_t* create(lv_obj_t* parent) override;

    /**
     * @brief Get human-readable overlay name
     * @return "Tool Mapping"
     */
    const char* get_name() const override {
        return "Tool Mapping";
    }

    //
    // === Public API ===
    //

    /**
     * @brief Show the overlay
     *
     * This method:
     * 1. Ensures overlay is created (lazy init)
     * 2. Queries backend for tool mapping capabilities
     * 3. Populates tool rows based on backend data
     * 4. Pushes overlay onto navigation stack
     *
     * @param parent_screen The parent screen for overlay creation
     */
    void show(lv_obj_t* parent_screen);

    /**
     * @brief Refresh the tool mapping rows from backend
     *
     * Re-queries the backend and updates all rows.
     */
    void refresh();

  private:
    //
    // === Internal Methods ===
    //

    /**
     * @brief Populate tool mapping rows from backend data
     *
     * Creates a row for each tool with:
     * - Label showing tool number (T0, T1, etc.)
     * - Color swatch showing mapped slot's color
     * - Dropdown for selecting the slot
     */
    void populate_rows();

    /**
     * @brief Clear all existing tool rows
     */
    void clear_rows();

    /**
     * @brief Create a single tool mapping row
     *
     * @param tool_index Tool number (0-based)
     * @param current_slot Currently mapped slot index
     * @param slot_count Total number of available slots
     * @param backend Backend for slot info queries
     * @return The created row widget
     */
    lv_obj_t* create_tool_row(int tool_index, int current_slot, int slot_count,
                              AmsBackend* backend);

    /**
     * @brief Update color swatch for a tool row
     *
     * @param row The row widget
     * @param slot_index The slot to get color from
     * @param backend Backend for slot info queries
     */
    void update_row_color_swatch(lv_obj_t* row, int slot_index, AmsBackend* backend);

    /**
     * @brief Show the "not supported" message
     */
    void show_not_supported();

    //
    // === Static Callbacks ===
    //

    /**
     * @brief Callback for dropdown value change
     *
     * Gets tool index from row's user_data, reads selected slot
     * from dropdown, and calls backend->set_tool_mapping().
     */
    static void on_slot_dropdown_changed(lv_event_t* e);

    //
    // === State ===
    //

    /// Alias for overlay_root_ to match existing pattern
    lv_obj_t*& overlay_ = overlay_root_;

    /// Container for tool rows
    lv_obj_t* rows_container_ = nullptr;

    /// Not supported card
    lv_obj_t* not_supported_card_ = nullptr;

    /// Track created rows for cleanup
    std::vector<lv_obj_t*> tool_rows_;
};

/**
 * @brief Global instance accessor
 *
 * Creates the overlay on first access and registers it for cleanup
 * with StaticPanelRegistry.
 *
 * @return Reference to singleton AmsToolMappingOverlay
 */
AmsToolMappingOverlay& get_ams_tool_mapping_overlay();

} // namespace helix::ui
