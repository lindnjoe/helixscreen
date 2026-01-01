// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_observer_guard.h"

#include "overlay_base.h"

/**
 * @file ui_panel_fan.h
 * @brief Fan control panel - part cooling fan slider + presets, hotend fan status
 *
 * Provides slider for part cooling fan (0-100%), preset buttons (Off/50%/75%/100%),
 * and read-only display for firmware-controlled hotend fan.
 * Uses OverlayBase pattern with lifecycle hooks.
 */
class FanPanel : public OverlayBase {
  public:
    FanPanel();
    ~FanPanel() override;

    // === OverlayBase interface ===
    void init_subjects() override;
    void deinit_subjects();
    void register_callbacks() override;
    lv_obj_t* create(lv_obj_t* parent) override;
    const char* get_name() const override {
        return "Fan Control Panel";
    }

    // === Lifecycle hooks ===
    void on_activate() override;
    void on_deactivate() override;

    // === Public API ===
    lv_obj_t* get_panel() const {
        return overlay_root_;
    }

    /**
     * @brief Set fan speeds (called from observer or externally)
     * @param part_speed Part cooling fan speed (0-100%)
     * @param hotend_speed Hotend fan speed (0-100%)
     */
    void set_fan_speeds(int part_speed, int hotend_speed);

    /**
     * @brief Get current slider value
     * @return Current slider value (0-100)
     */
    int get_slider_value() const {
        return slider_value_;
    }

  private:
    // Subjects for reactive text binding
    lv_subject_t part_speed_display_subject_;
    lv_subject_t hotend_speed_display_subject_;

    // Persistent string buffers for LVGL binding
    char part_speed_buf_[16];
    char hotend_speed_buf_[16];

    // Current state
    int slider_value_ = 0;
    int part_fan_speed_ = 0;
    int hotend_fan_speed_ = 0;

    // Widget references
    lv_obj_t* fan_speed_slider_ = nullptr;
    lv_obj_t* fan_speed_value_label_ = nullptr;
    lv_obj_t* status_message_ = nullptr;
    lv_obj_t* preset_buttons_[4] = {nullptr};

    // Parent screen reference
    lv_obj_t* parent_screen_ = nullptr;
    bool callbacks_registered_ = false;

    // Observers with RAII cleanup
    ObserverGuard fan_speed_observer_;

    // Preset values
    static constexpr int PRESET_VALUES[4] = {0, 50, 75, 100};

    // Setup helpers
    void setup_slider();
    void setup_preset_buttons();
    void register_fan_observer();

    // State update helpers
    void update_speed_display();
    void update_slider_value_label();
    void send_fan_speed(int speed);

    // Event handlers
    void handle_preset_button(int preset_index);
    void handle_slider_changed(int value);

    // Static callbacks for LVGL
    static void on_preset_button_clicked(lv_event_t* e);
    static void on_slider_value_changed(lv_event_t* e);
    static void on_fan_speed_changed(lv_observer_t* observer, lv_subject_t* subject);
};

/**
 * @brief Get global fan panel instance
 * @return Reference to the singleton fan panel
 */
FanPanel& get_global_fan_panel();
