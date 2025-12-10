// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 HelixScreen Contributors

/**
 * @file wifi_settings_overlay.h
 * @brief WiFi Settings overlay panel - network configuration and testing
 *
 * Manages reactive WiFi settings overlay with:
 * - WiFi enable/disable toggle
 * - Network scanning and selection
 * - Connection status display (SSID, IP, MAC)
 * - Network connectivity testing (gateway + internet)
 * - Password entry modal for secured networks
 * - Hidden network configuration
 *
 * ## Architecture
 *
 * Fully reactive design - C++ updates subjects, XML handles all UI bindings.
 * Minimal direct widget manipulation (only network list population).
 *
 * ## Subject Bindings (10 total):
 *
 * - wifi_enabled (int) - 0=off, 1=on
 * - wifi_connected (int) - 0=disconnected, 1=connected
 * - connected_ssid (string) - Current network name
 * - ip_address (string) - e.g., "192.168.1.100"
 * - mac_address (string) - e.g., "50:41:1C:xx:xx:xx"
 * - network_count (string) - e.g., "(4)"
 * - wifi_scanning (int) - 0=idle, 1=scanning
 * - test_running (int) - 0=idle, 1=running
 * - test_gateway_status (int) - 0=pending, 1=active, 2=success, 3=failed
 * - test_internet_status (int) - 0=pending, 1=active, 2=success, 3=failed
 *
 * ## Initialization Order (CRITICAL):
 *
 *   1. Register XML components (wifi_settings_overlay.xml, wifi_network_item.xml)
 *   2. init_subjects()
 *   3. register_callbacks()
 *   4. create(parent_screen)
 *   5. show() when ready to display
 */

#pragma once

#include "lvgl/lvgl.h"
#include "network_tester.h"

#include <memory>
#include <string>
#include <vector>

// Forward declarations
class WiFiManager;
struct WiFiNetwork;

/**
 * @class WiFiSettingsOverlay
 * @brief Reactive WiFi settings overlay panel
 *
 * Manages WiFi configuration UI with reactive subject-based architecture.
 * Integrates with WiFiManager for scanning/connection and NetworkTester
 * for connectivity validation.
 */
class WiFiSettingsOverlay {
  public:
    WiFiSettingsOverlay();
    ~WiFiSettingsOverlay();

    // Non-copyable
    WiFiSettingsOverlay(const WiFiSettingsOverlay&) = delete;
    WiFiSettingsOverlay& operator=(const WiFiSettingsOverlay&) = delete;

    /**
     * @brief Initialize reactive subjects
     *
     * Creates and registers 10 subjects with defaults.
     * MUST be called BEFORE create() to ensure bindings work.
     */
    void init_subjects();

    /**
     * @brief Register event callbacks with lv_xml system
     *
     * Registers callbacks:
     * - on_wlan_toggle_changed
     * - on_refresh_clicked
     * - on_test_network_clicked
     * - on_add_other_clicked
     * - on_network_item_clicked
     */
    void register_callbacks();

    /**
     * @brief Create overlay UI from XML
     *
     * @param parent_screen Parent screen widget to attach overlay to
     * @return Root object of overlay, or nullptr on failure
     */
    lv_obj_t* create(lv_obj_t* parent_screen);

    /**
     * @brief Show overlay panel
     *
     * Pushes overlay onto navigation stack, triggers network scan if enabled.
     */
    void show();

    /**
     * @brief Hide overlay panel
     *
     * Pops overlay from navigation stack, stops scanning.
     */
    void hide();

    /**
     * @brief Clean up resources
     *
     * Stops scanning, destroys managers, resets UI references.
     */
    void cleanup();

    /**
     * @brief Check if overlay is created
     * @return true if overlay widget exists
     */
    bool is_created() const {
        return overlay_root_ != nullptr;
    }

    /**
     * @brief Check if overlay is visible
     * @return true if overlay is currently shown
     */
    bool is_visible() const {
        return visible_;
    }

  private:
    // Widget references (minimal - prefer subjects)
    lv_obj_t* overlay_root_ = nullptr;
    lv_obj_t* parent_screen_ = nullptr;
    lv_obj_t* networks_list_ = nullptr;

    // Subjects (11 total)
    lv_subject_t wifi_enabled_;
    lv_subject_t wifi_connected_;
    lv_subject_t wifi_only_24ghz_; // 1 if hardware only supports 2.4GHz
    lv_subject_t connected_ssid_;
    lv_subject_t ip_address_;
    lv_subject_t mac_address_;
    lv_subject_t network_count_;
    lv_subject_t wifi_scanning_;
    lv_subject_t test_running_;
    lv_subject_t test_gateway_status_;
    lv_subject_t test_internet_status_;

    // String buffers (subjects need stable char* pointers)
    char ssid_buffer_[64];
    char ip_buffer_[32];
    char mac_buffer_[32];
    char count_buffer_[16];

    // Integration
    std::shared_ptr<WiFiManager> wifi_manager_;
    std::shared_ptr<NetworkTester> network_tester_;

    // State tracking
    bool subjects_initialized_ = false;
    bool callbacks_registered_ = false;
    bool visible_ = false;
    bool cleanup_called_ = false;

    // Current network selection for password modal
    char current_ssid_[64];
    bool current_network_is_secured_ = false;

    // Cached networks for async UI update
    std::vector<WiFiNetwork> cached_networks_;

    // Event handler implementations
    void handle_wlan_toggle_changed(lv_event_t* e);
    void handle_refresh_clicked();
    void handle_test_network_clicked();
    void handle_add_other_clicked();
    void handle_network_item_clicked(lv_event_t* e);

    // Helper functions
    void update_connection_status();
    void update_test_state(NetworkTester::TestState state, const NetworkTester::TestResult& result);
    void populate_network_list(const std::vector<WiFiNetwork>& networks);
    void clear_network_list();
    void show_placeholder(bool show);
    void update_signal_icons(lv_obj_t* item, int icon_state);

    // Static trampolines for LVGL callbacks
    static void on_wlan_toggle_changed(lv_event_t* e);
    static void on_refresh_clicked(lv_event_t* e);
    static void on_test_network_clicked(lv_event_t* e);
    static void on_add_other_clicked(lv_event_t* e);
    static void on_network_item_clicked(lv_event_t* e);
};

// ============================================================================
// Global Instance Access
// ============================================================================

/**
 * @brief Get the global WiFiSettingsOverlay instance
 *
 * Creates the instance on first call. Singleton pattern.
 */
WiFiSettingsOverlay& get_wifi_settings_overlay();

/**
 * @brief Destroy the global WiFiSettingsOverlay instance
 *
 * Call during application shutdown.
 */
void destroy_wifi_settings_overlay();
