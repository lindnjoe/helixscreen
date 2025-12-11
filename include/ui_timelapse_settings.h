// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_panel_base.h"

#include "lvgl.h"
#include "moonraker_api.h"
#include "printer_state.h"

/**
 * @file ui_timelapse_settings.h
 * @brief Timelapse settings overlay panel
 *
 * Configures Moonraker-Timelapse plugin settings for recording prints.
 * Provides UI for enabling timelapse, selecting recording mode, and
 * configuring output settings.
 *
 * ## Features
 * - Enable/disable timelapse recording
 * - Recording mode: Layer Macro (per-layer) or Hyperlapse (time-based)
 * - Output framerate selection (15/24/30/60 fps)
 * - Auto-render toggle (create video when print completes)
 *
 * ## Moonraker API
 * - machine.timelapse.get_settings - Fetch current settings
 * - machine.timelapse.post_settings - Update settings
 *
 * @see docs/FEATURE_STATUS.md for implementation progress
 */
class TimelapseSettingsOverlay : public PanelBase {
  public:
    /**
     * @brief Construct TimelapseSettingsOverlay
     * @param printer_state Reference to global printer state
     * @param api Pointer to MoonrakerAPI (may be nullptr in test mode)
     */
    TimelapseSettingsOverlay(PrinterState& printer_state, MoonrakerAPI* api);

    // PanelBase interface
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;
    [[nodiscard]] const char* get_name() const override {
        return "TimelapseSettings";
    }
    [[nodiscard]] const char* get_xml_component_name() const override {
        return "timelapse_settings_overlay";
    }
    void init_subjects() override;

    // Lifecycle hooks
    void on_activate() override;
    void on_deactivate() override;

  private:
    /**
     * @brief Fetch current settings from Moonraker
     */
    void fetch_settings();

    /**
     * @brief Update settings to Moonraker
     */
    void save_settings();

    /**
     * @brief Update mode info text based on current selection
     * @param mode_index 0=Layer Macro, 1=Hyperlapse
     */
    void update_mode_info(int mode_index);

    // Event handlers
    static void on_enabled_changed(lv_event_t* e);
    static void on_mode_changed(lv_event_t* e);
    static void on_framerate_changed(lv_event_t* e);
    static void on_autorender_changed(lv_event_t* e);

    // Current settings (loaded from API)
    TimelapseSettings current_settings_;
    bool settings_loaded_ = false;

    // Widget references
    lv_obj_t* enable_switch_ = nullptr;
    lv_obj_t* mode_dropdown_ = nullptr;
    lv_obj_t* mode_info_text_ = nullptr;
    lv_obj_t* framerate_dropdown_ = nullptr;
    lv_obj_t* autorender_switch_ = nullptr;

    // Framerate values for dropdown index mapping
    static constexpr int FRAMERATE_VALUES[] = {15, 24, 30, 60};
    static constexpr int FRAMERATE_COUNT = 4;

    /**
     * @brief Convert framerate value to dropdown index
     */
    static int framerate_to_index(int framerate);

    /**
     * @brief Convert dropdown index to framerate value
     */
    static int index_to_framerate(int index);
};

// Global accessor
TimelapseSettingsOverlay& get_global_timelapse_settings();
void init_global_timelapse_settings(PrinterState& printer_state, MoonrakerAPI* api);
