// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file overlay_base.h
 * @brief Abstract base class for overlay panels with lifecycle hooks
 *
 * @pattern Two-phase init (init_subjects -> XML -> callbacks); lifecycle hooks via
 * NavigationManager
 * @threading Main thread only
 *
 * OverlayBase provides lifecycle management for overlay panels:
 * - on_activate() called when overlay becomes visible (slide-in complete)
 * - on_deactivate() called when overlay is being hidden (before slide-out)
 *
 * ## Lifecycle Flow
 *
 * ### Overlay pushed:
 * 1. If first overlay: main panel's on_deactivate() is called
 * 2. If nested: previous overlay's on_deactivate() is called
 * 3. Overlay shows with slide-in animation
 * 4. Overlay's on_activate() is called
 *
 * ### Overlay popped (go_back):
 * 1. Overlay's on_deactivate() is called
 * 2. Slide-out animation plays
 * 3. If returning to main panel: main panel's on_activate() is called
 * 4. If returning to previous overlay: previous overlay's on_activate() is called
 *
 * ## Usage Pattern:
 *
 * @code
 * class MyOverlay : public OverlayBase {
 * public:
 *     void init_subjects() override {
 *         // Register LVGL subjects for XML binding
 *         subjects_initialized_ = true;
 *     }
 *
 *     void register_callbacks() override {
 *         // Register event callbacks with lv_xml_register_event_cb()
 *     }
 *
 *     lv_obj_t* create(lv_obj_t* parent) override {
 *         overlay_root_ = lv_xml_create(parent, "my_overlay", nullptr);
 *         return overlay_root_;
 *     }
 *
 *     const char* get_name() const override { return "My Overlay"; }
 *
 *     void on_activate() override {
 *         // Start scanning, refresh data, etc.
 *         visible_ = true;
 *     }
 *
 *     void on_deactivate() override {
 *         // Stop scanning, cancel pending operations, etc.
 *         visible_ = false;
 *     }
 * };
 * @endcode
 *
 * @see NetworkSettingsOverlay for reference implementation
 */

#pragma once

#include "lvgl/lvgl.h"
#include "panel_lifecycle.h"

/**
 * @class OverlayBase
 * @brief Abstract base class for overlay panels with lifecycle management
 *
 * Provides shared infrastructure for overlay panels including:
 * - Lifecycle hooks (on_activate/on_deactivate) called by NavigationManager
 * - Two-phase initialization (init_subjects -> create -> register_callbacks)
 * - Async-safe cleanup pattern
 *
 * @implements IPanelLifecycle for NavigationManager dispatch
 */
class OverlayBase : public IPanelLifecycle {
  public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~OverlayBase();

    // Non-copyable (unique overlay instances)
    OverlayBase(const OverlayBase&) = delete;
    OverlayBase& operator=(const OverlayBase&) = delete;

    //
    // === Core Interface (must implement) ===
    //

    /**
     * @brief Initialize LVGL subjects for XML data binding
     *
     * MUST be called BEFORE create() to ensure bindings work.
     * Implementations should set subjects_initialized_ = true.
     */
    virtual void init_subjects() = 0;

    /**
     * @brief Create overlay UI from XML
     *
     * @param parent Parent widget to attach overlay to (usually screen)
     * @return Root object of overlay, or nullptr on failure
     *
     * Implementations should store result in overlay_root_.
     */
    virtual lv_obj_t* create(lv_obj_t* parent) = 0;

    /**
     * @brief Get human-readable overlay name
     *
     * Used in logging and debugging.
     *
     * @return Overlay name (e.g., "Network Settings")
     */
    const char* get_name() const override = 0;

    //
    // === Optional Hooks (override as needed) ===
    //

    /**
     * @brief Register event callbacks with lv_xml system
     *
     * Called after create() to register XML event callbacks.
     * Default implementation does nothing.
     */
    virtual void register_callbacks() {}

    /**
     * @brief Called when overlay becomes visible
     *
     * Override to start scanning, refresh data, begin animations, etc.
     * Called by NavigationManager after slide-in animation starts.
     * Default implementation sets visible_ = true.
     */
    void on_activate() override;

    /**
     * @brief Called when overlay is being hidden
     *
     * Override to stop scanning, cancel pending operations, pause timers.
     * Called by NavigationManager before slide-out animation starts.
     * Default implementation sets visible_ = false.
     */
    void on_deactivate() override;

    /**
     * @brief Clean up resources for async-safe destruction
     *
     * Call this before destroying the overlay to handle any pending
     * async callbacks safely. Sets cleanup_called_ flag.
     */
    virtual void cleanup();

    //
    // === State Queries ===
    //

    /**
     * @brief Check if overlay is currently visible
     * @return true if overlay is visible
     */
    bool is_visible() const {
        return visible_;
    }

    /**
     * @brief Check if cleanup has been called
     * @return true if cleanup() was called
     */
    bool cleanup_called() const {
        return cleanup_called_;
    }

    /**
     * @brief Get root overlay widget
     * @return Root widget, or nullptr if not created
     */
    lv_obj_t* get_root() const {
        return overlay_root_;
    }

    /**
     * @brief Check if subjects have been initialized
     * @return true if init_subjects() was called
     */
    bool are_subjects_initialized() const {
        return subjects_initialized_;
    }

  protected:
    /**
     * @brief Default constructor (protected - use derived classes)
     */
    OverlayBase() = default;

    //
    // === Protected State ===
    //

    lv_obj_t* overlay_root_ = nullptr;  ///< Root widget of overlay UI
    bool subjects_initialized_ = false; ///< True after init_subjects() called
    bool visible_ = false;              ///< True when overlay is visible
    bool cleanup_called_ = false;       ///< True after cleanup() called
};
