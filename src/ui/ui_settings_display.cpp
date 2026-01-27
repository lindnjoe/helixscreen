// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_settings_display.cpp
 * @brief Implementation of DisplaySettingsOverlay
 */

#include "ui_settings_display.h"

#include "ui_event_safety.h"
#include "ui_nav_manager.h"
#include "ui_theme_editor_overlay.h"
#include "ui_utils.h"

#include "settings_manager.h"
#include "static_panel_registry.h"
#include "theme_core.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <memory>

namespace helix::settings {

// ============================================================================
// SINGLETON ACCESSOR
// ============================================================================

static std::unique_ptr<DisplaySettingsOverlay> g_display_settings_overlay;

DisplaySettingsOverlay& get_display_settings_overlay() {
    if (!g_display_settings_overlay) {
        g_display_settings_overlay = std::make_unique<DisplaySettingsOverlay>();
        StaticPanelRegistry::instance().register_destroy(
            "DisplaySettingsOverlay", []() { g_display_settings_overlay.reset(); });
    }
    return *g_display_settings_overlay;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

DisplaySettingsOverlay::DisplaySettingsOverlay() {
    spdlog::debug("[{}] Created", get_name());
}

DisplaySettingsOverlay::~DisplaySettingsOverlay() {
    if (subjects_initialized_ && lv_is_initialized()) {
        lv_subject_deinit(&brightness_value_subject_);
    }
    spdlog::debug("[{}] Destroyed", get_name());
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void DisplaySettingsOverlay::init_subjects() {
    if (subjects_initialized_) {
        return;
    }

    // Initialize brightness value subject for label binding
    // 5-arg form: (subject, buf, prev_buf, size, initial_value)
    snprintf(brightness_value_buf_, sizeof(brightness_value_buf_), "100%%");
    lv_subject_init_string(&brightness_value_subject_, brightness_value_buf_, nullptr,
                           sizeof(brightness_value_buf_), brightness_value_buf_);
    lv_xml_register_subject(nullptr, "brightness_value", &brightness_value_subject_);

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized", get_name());
}

void DisplaySettingsOverlay::register_callbacks() {
    // Brightness slider callback
    lv_xml_register_event_cb(nullptr, "on_brightness_changed", on_brightness_changed);

    // Theme explorer callbacks (primary panel)
    lv_xml_register_event_cb(nullptr, "on_theme_preset_changed", on_theme_preset_changed);
    lv_xml_register_event_cb(nullptr, "on_theme_settings_clicked", on_theme_settings_clicked);
    lv_xml_register_event_cb(nullptr, "on_preview_dark_mode_toggled", on_preview_dark_mode_toggled);
    lv_xml_register_event_cb(nullptr, "on_edit_colors_clicked", on_edit_colors_clicked);

    // Apply button uses header_bar's action_button mechanism
    // The overlay_panel passes action_button_callback through, so we need to register it
    lv_xml_register_event_cb(nullptr, "on_apply_theme_clicked", on_apply_theme_clicked);

    spdlog::debug("[{}] Callbacks registered", get_name());
}

// ============================================================================
// UI CREATION
// ============================================================================

lv_obj_t* DisplaySettingsOverlay::create(lv_obj_t* parent) {
    if (overlay_root_) {
        spdlog::warn("[{}] create() called but overlay already exists", get_name());
        return overlay_root_;
    }

    spdlog::debug("[{}] Creating overlay...", get_name());

    // Create from XML component
    overlay_root_ =
        static_cast<lv_obj_t*>(lv_xml_create(parent, "display_settings_overlay", nullptr));
    if (!overlay_root_) {
        spdlog::error("[{}] Failed to create overlay from XML", get_name());
        return nullptr;
    }

    // Initially hidden until show() pushes it
    lv_obj_add_flag(overlay_root_, LV_OBJ_FLAG_HIDDEN);

    spdlog::info("[{}] Overlay created", get_name());
    return overlay_root_;
}

void DisplaySettingsOverlay::show(lv_obj_t* parent_screen) {
    spdlog::debug("[{}] show() called", get_name());

    parent_screen_ = parent_screen;

    // Ensure subjects and callbacks are initialized
    if (!subjects_initialized_) {
        init_subjects();
        register_callbacks();
    }

    // Lazy create overlay
    if (!overlay_root_ && parent_screen_) {
        create(parent_screen_);
    }

    if (!overlay_root_) {
        spdlog::error("[{}] Cannot show - overlay not created", get_name());
        return;
    }

    // Register for lifecycle callbacks
    NavigationManager::instance().register_overlay_instance(overlay_root_, this);

    // Push onto navigation stack (on_activate will initialize dropdowns)
    ui_nav_push_overlay(overlay_root_);
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void DisplaySettingsOverlay::on_activate() {
    OverlayBase::on_activate();

    // Initialize all widget values from SettingsManager
    init_brightness_controls();
    init_dim_dropdown();
    init_sleep_dropdown();
    init_bed_mesh_dropdown();
    init_gcode_dropdown();
    init_time_format_dropdown();
}

void DisplaySettingsOverlay::on_deactivate() {
    OverlayBase::on_deactivate();
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void DisplaySettingsOverlay::init_brightness_controls() {
    if (!overlay_root_)
        return;

    lv_obj_t* brightness_slider = lv_obj_find_by_name(overlay_root_, "brightness_slider");
    if (brightness_slider) {
        // Set initial value from settings
        int brightness = SettingsManager::instance().get_brightness();
        lv_slider_set_value(brightness_slider, brightness, LV_ANIM_OFF);

        // Update subject (label binding happens in XML)
        snprintf(brightness_value_buf_, sizeof(brightness_value_buf_), "%d%%", brightness);
        lv_subject_copy_string(&brightness_value_subject_, brightness_value_buf_);

        spdlog::debug("[{}] Brightness initialized to {}%", get_name(), brightness);
    }
}

void DisplaySettingsOverlay::init_dim_dropdown() {
    if (!overlay_root_)
        return;

    lv_obj_t* dim_row = lv_obj_find_by_name(overlay_root_, "row_display_dim");
    lv_obj_t* dim_dropdown = dim_row ? lv_obj_find_by_name(dim_row, "dropdown") : nullptr;
    if (dim_dropdown) {
        // Set initial selection based on current setting (options set in XML)
        int current_sec = SettingsManager::instance().get_display_dim_sec();
        int index = SettingsManager::dim_seconds_to_index(current_sec);
        lv_dropdown_set_selected(dim_dropdown, index);

        spdlog::debug("[{}] Dim dropdown initialized to index {} ({}s)", get_name(), index,
                      current_sec);
    }
}

void DisplaySettingsOverlay::init_sleep_dropdown() {
    if (!overlay_root_)
        return;

    lv_obj_t* sleep_row = lv_obj_find_by_name(overlay_root_, "row_display_sleep");
    lv_obj_t* sleep_dropdown = sleep_row ? lv_obj_find_by_name(sleep_row, "dropdown") : nullptr;
    if (sleep_dropdown) {
        // Set initial selection based on current setting (options set in XML)
        int current_sec = SettingsManager::instance().get_display_sleep_sec();
        int index = SettingsManager::sleep_seconds_to_index(current_sec);
        lv_dropdown_set_selected(sleep_dropdown, index);

        spdlog::debug("[{}] Sleep dropdown initialized to index {} ({}s)", get_name(), index,
                      current_sec);
    }
}

void DisplaySettingsOverlay::init_bed_mesh_dropdown() {
    if (!overlay_root_)
        return;

    lv_obj_t* bed_mesh_row = lv_obj_find_by_name(overlay_root_, "row_bed_mesh_mode");
    lv_obj_t* bed_mesh_dropdown =
        bed_mesh_row ? lv_obj_find_by_name(bed_mesh_row, "dropdown") : nullptr;
    if (bed_mesh_dropdown) {
        // Set initial selection based on current setting (options set in XML)
        int current_mode = SettingsManager::instance().get_bed_mesh_render_mode();
        lv_dropdown_set_selected(bed_mesh_dropdown, current_mode);

        spdlog::debug("[{}] Bed mesh mode dropdown initialized to {} ({})", get_name(),
                      current_mode, current_mode == 0 ? "Auto" : (current_mode == 1 ? "3D" : "2D"));
    }
}

void DisplaySettingsOverlay::init_gcode_dropdown() {
    if (!overlay_root_)
        return;

    // G-code mode row is hidden by default, but we still initialize it
    lv_obj_t* gcode_row = lv_obj_find_by_name(overlay_root_, "row_gcode_mode");
    lv_obj_t* gcode_dropdown = gcode_row ? lv_obj_find_by_name(gcode_row, "dropdown") : nullptr;
    if (gcode_dropdown) {
        // Set initial selection based on current setting (options set in XML)
        int current_mode = SettingsManager::instance().get_gcode_render_mode();
        lv_dropdown_set_selected(gcode_dropdown, current_mode);

        spdlog::debug("[{}] G-code mode dropdown initialized to {} ({})", get_name(), current_mode,
                      current_mode == 0 ? "Auto" : (current_mode == 1 ? "3D" : "2D Layers"));
    }
}

void DisplaySettingsOverlay::init_theme_preset_dropdown(lv_obj_t* root) {
    if (!root)
        return;

    // Try direct lookup first (Theme Explorer uses this name)
    lv_obj_t* theme_preset_dropdown = lv_obj_find_by_name(root, "theme_preset_dropdown");

    // Fall back to nested row lookup (Theme Editor used this pattern)
    if (!theme_preset_dropdown) {
        lv_obj_t* theme_preset_row = lv_obj_find_by_name(root, "row_theme_preset");
        theme_preset_dropdown =
            theme_preset_row ? lv_obj_find_by_name(theme_preset_row, "dropdown") : nullptr;
    }

    if (theme_preset_dropdown) {
        // Set dropdown options from discovered theme files
        std::string options = SettingsManager::instance().get_theme_options();
        lv_dropdown_set_options(theme_preset_dropdown, options.c_str());

        // Set initial selection based on current theme
        int current_index = SettingsManager::instance().get_theme_index();
        lv_dropdown_set_selected(theme_preset_dropdown, static_cast<uint32_t>(current_index));

        spdlog::debug("[{}] Theme dropdown initialized to index {} ({})", get_name(), current_index,
                      SettingsManager::instance().get_theme_name());
    }
}

void DisplaySettingsOverlay::init_time_format_dropdown() {
    if (!overlay_root_)
        return;

    lv_obj_t* time_format_row = lv_obj_find_by_name(overlay_root_, "row_time_format");
    lv_obj_t* time_format_dropdown =
        time_format_row ? lv_obj_find_by_name(time_format_row, "dropdown") : nullptr;
    if (time_format_dropdown) {
        // Set initial selection based on current setting (options set in XML)
        auto current_format = SettingsManager::instance().get_time_format();
        lv_dropdown_set_selected(time_format_dropdown, static_cast<uint32_t>(current_format));

        spdlog::debug("[{}] Time format dropdown initialized to {} ({})", get_name(),
                      static_cast<int>(current_format),
                      current_format == TimeFormat::HOUR_12 ? "12H" : "24H");
    }
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void DisplaySettingsOverlay::handle_brightness_changed(int value) {
    SettingsManager::instance().set_brightness(value);

    // Update subject (label binding happens in XML)
    snprintf(brightness_value_buf_, sizeof(brightness_value_buf_), "%d%%", value);
    lv_subject_copy_string(&brightness_value_subject_, brightness_value_buf_);
}

void DisplaySettingsOverlay::handle_theme_preset_changed(int index) {
    // If called from Theme Explorer, preview the theme locally
    if (theme_explorer_overlay_ && lv_obj_is_visible(theme_explorer_overlay_)) {
        handle_explorer_theme_changed(index);
        return;
    }

    // Otherwise fall back to global theme change (legacy behavior)
    SettingsManager::instance().set_theme_by_index(index);

    spdlog::info("[{}] Theme changed to index {} ({})", get_name(), index,
                 SettingsManager::instance().get_theme_name());
}

void DisplaySettingsOverlay::handle_explorer_theme_changed(int index) {
    // Preview selected theme without saving globally
    // Use cached theme list (populated when explorer opens)
    if (index < 0 || index >= static_cast<int>(cached_themes_.size())) {
        spdlog::error("[{}] Invalid theme index {}", get_name(), index);
        return;
    }

    std::string theme_name = cached_themes_[index].filename;
    helix::ThemeData theme = helix::load_theme_from_file(theme_name);

    if (!theme.is_valid()) {
        spdlog::error("[{}] Failed to load theme '{}' for preview", get_name(), theme_name);
        return;
    }

    // Store for passing to editor
    preview_theme_name_ = theme_name;

    // Preview the theme with current dark mode setting
    theme_manager_preview(theme);

    // Update Apply button state - enable if different from original
    if (theme_explorer_overlay_) {
        lv_obj_t* header = lv_obj_find_by_name(theme_explorer_overlay_, "overlay_header");
        lv_obj_t* action_btn = header ? lv_obj_find_by_name(header, "action_button") : nullptr;
        if (action_btn) {
            if (index != original_theme_index_) {
                lv_obj_remove_state(action_btn, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(action_btn, LV_STATE_DISABLED);
            }
        }
    }

    spdlog::debug("[{}] Explorer preview: theme '{}' (index {})", get_name(), theme_name, index);
}

void DisplaySettingsOverlay::handle_theme_settings_clicked() {
    // Primary entry point: Opens Theme Explorer first (not editor)
    if (!parent_screen_) {
        spdlog::warn("[{}] Theme settings clicked without parent screen", get_name());
        return;
    }

    if (!theme_explorer_overlay_) {
        spdlog::debug("[{}] Creating theme explorer overlay...", get_name());
        theme_explorer_overlay_ =
            static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "theme_preview_overlay", nullptr));
        if (!theme_explorer_overlay_) {
            spdlog::error("[{}] Failed to create theme explorer overlay", get_name());
            return;
        }

        lv_obj_add_flag(theme_explorer_overlay_, LV_OBJ_FLAG_HIDDEN);

        // Register with nullptr - this overlay has no lifecycle object but we
        // register to suppress the "pushed without lifecycle registration" warning
        NavigationManager::instance().register_overlay_instance(theme_explorer_overlay_, nullptr);
        NavigationManager::instance().register_overlay_close_callback(
            theme_explorer_overlay_, [this]() {
                // Revert preview to current theme on close
                theme_manager_revert_preview();
                lv_obj_safe_delete(theme_explorer_overlay_);
                // Clear cache so next open picks up filesystem changes
                cached_themes_.clear();
            });
    }

    // Initialize theme preset dropdown
    init_theme_preset_dropdown(theme_explorer_overlay_);

    // Cache the theme list to avoid re-parsing on every toggle/selection
    cached_themes_ = helix::discover_themes(helix::get_themes_directory());

    // Remember original theme for Apply button state and preview
    original_theme_index_ = SettingsManager::instance().get_theme_index();
    preview_theme_name_ = SettingsManager::instance().get_theme_name();

    // Initialize dark mode toggle to current global state
    preview_is_dark_ = theme_manager_is_dark_mode();
    lv_obj_t* dark_toggle =
        lv_obj_find_by_name(theme_explorer_overlay_, "preview_dark_mode_toggle");
    if (dark_toggle) {
        if (preview_is_dark_) {
            lv_obj_add_state(dark_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(dark_toggle, LV_STATE_CHECKED);
        }
    }

    // Initially disable Apply button (no changes yet)
    lv_obj_t* header = lv_obj_find_by_name(theme_explorer_overlay_, "overlay_header");
    lv_obj_t* action_btn = header ? lv_obj_find_by_name(header, "action_button") : nullptr;
    if (action_btn) {
        lv_obj_add_state(action_btn, LV_STATE_DISABLED);
    }

    ui_nav_push_overlay(theme_explorer_overlay_);
}

void DisplaySettingsOverlay::handle_apply_theme_clicked() {
    // Apply the currently selected (previewed) theme globally
    lv_obj_t* dropdown = theme_explorer_overlay_
                             ? lv_obj_find_by_name(theme_explorer_overlay_, "theme_preset_dropdown")
                             : nullptr;
    if (!dropdown) {
        spdlog::warn("[{}] Apply clicked but dropdown not found", get_name());
        return;
    }

    int selected_index = lv_dropdown_get_selected(dropdown);
    SettingsManager::instance().set_theme_by_index(selected_index);

    // Update original index since theme is now applied
    original_theme_index_ = selected_index;

    // Disable Apply button since changes are now saved
    lv_obj_t* header = lv_obj_find_by_name(theme_explorer_overlay_, "overlay_header");
    lv_obj_t* action_btn = header ? lv_obj_find_by_name(header, "action_button") : nullptr;
    if (action_btn) {
        lv_obj_add_state(action_btn, LV_STATE_DISABLED);
    }

    // Show restart notice using info note toast (non-blocking)
    spdlog::info("[{}] Theme applied - index {}. Restart required for full effect.", get_name(),
                 selected_index);
}

void DisplaySettingsOverlay::handle_edit_colors_clicked() {
    // Open Theme Colors Editor (secondary panel)
    if (!parent_screen_) {
        spdlog::warn("[{}] Theme settings clicked without parent screen", get_name());
        return;
    }

    // Create theme editor overlay on first access (lazy initialization)
    if (!theme_settings_overlay_) {
        spdlog::debug("[{}] Creating theme editor overlay...", get_name());
        auto& overlay = get_theme_editor_overlay();

        // Initialize subjects and callbacks if not already done
        if (!overlay.are_subjects_initialized()) {
            overlay.init_subjects();
        }
        overlay.register_callbacks();

        // Create overlay UI
        theme_settings_overlay_ = overlay.create(parent_screen_);
        if (!theme_settings_overlay_) {
            spdlog::error("[{}] Failed to create theme editor overlay", get_name());
            return;
        }

        // Register with NavigationManager for lifecycle callbacks
        NavigationManager::instance().register_overlay_instance(theme_settings_overlay_, &overlay);
    }

    if (theme_settings_overlay_) {
        // Load currently previewed theme for editing (or fallback to saved theme)
        std::string theme_name = !preview_theme_name_.empty()
                                     ? preview_theme_name_
                                     : SettingsManager::instance().get_theme_name();
        get_theme_editor_overlay().load_theme(theme_name);
        ui_nav_push_overlay(theme_settings_overlay_);
    }
}

// ============================================================================
// STATIC CALLBACKS
// ============================================================================

void DisplaySettingsOverlay::on_brightness_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[DisplaySettingsOverlay] on_brightness_changed");
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int value = lv_slider_get_value(slider);
    get_display_settings_overlay().handle_brightness_changed(value);
    LVGL_SAFE_EVENT_CB_END();
}

void DisplaySettingsOverlay::on_theme_preset_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[DisplaySettingsOverlay] on_theme_preset_changed");
    auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int index = lv_dropdown_get_selected(dropdown);
    get_display_settings_overlay().handle_theme_preset_changed(index);
    LVGL_SAFE_EVENT_CB_END();
}

void DisplaySettingsOverlay::on_theme_settings_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[DisplaySettingsOverlay] on_theme_settings_clicked");
    static_cast<void>(lv_event_get_current_target(e));
    get_display_settings_overlay().handle_theme_settings_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void DisplaySettingsOverlay::on_apply_theme_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[DisplaySettingsOverlay] on_apply_theme_clicked");
    static_cast<void>(lv_event_get_current_target(e));
    get_display_settings_overlay().handle_apply_theme_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void DisplaySettingsOverlay::on_edit_colors_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[DisplaySettingsOverlay] on_edit_colors_clicked");
    static_cast<void>(lv_event_get_current_target(e));
    get_display_settings_overlay().handle_edit_colors_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void DisplaySettingsOverlay::on_preview_dark_mode_toggled(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[DisplaySettingsOverlay] on_preview_dark_mode_toggled");
    auto* target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    bool is_dark = lv_obj_has_state(target, LV_STATE_CHECKED);

    get_display_settings_overlay().handle_preview_dark_mode_toggled(is_dark);
    LVGL_SAFE_EVENT_CB_END();
}

// Helper to recursively update text colors within an object tree
static void update_text_colors_recursive(lv_obj_t* obj, lv_color_t text_primary) {
    if (!obj)
        return;

    // Check if this object has text (labels, buttons with text, etc.)
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_obj_set_style_text_color(obj, text_primary, LV_PART_MAIN);
    }

    // Recurse into children
    uint32_t child_count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(obj, i);
        update_text_colors_recursive(child, text_primary);
    }
}

void DisplaySettingsOverlay::handle_preview_dark_mode_toggled(bool is_dark) {
    // Update local state
    preview_is_dark_ = is_dark;

    // Get currently previewed theme (from dropdown selection)
    if (!theme_explorer_overlay_) {
        return;
    }

    lv_obj_t* dropdown = lv_obj_find_by_name(theme_explorer_overlay_, "theme_preset_dropdown");
    if (!dropdown) {
        return;
    }

    int selected_index = lv_dropdown_get_selected(dropdown);

    // Use cached theme list (populated when explorer opens)
    if (selected_index < 0 || selected_index >= static_cast<int>(cached_themes_.size())) {
        return;
    }

    // Pass just the theme name - load_theme_from_file() handles path resolution
    helix::ThemeData theme = helix::load_theme_from_file(cached_themes_[selected_index].filename);

    if (!theme.is_valid()) {
        return;
    }

    // Apply colors to preview cards using dual-palette system
    // Select palette based on mode toggle (fall back if mode not supported)
    const helix::ModePalette* palette = nullptr;
    if (is_dark && theme.supports_dark()) {
        palette = &theme.dark;
    } else if (!is_dark && theme.supports_light()) {
        palette = &theme.light;
    } else {
        // Fall back to whatever is available
        palette = theme.supports_dark() ? &theme.dark : &theme.light;
    }

    lv_color_t card_bg = theme_manager_parse_hex_color(palette->card_bg.c_str());
    lv_color_t app_bg = theme_manager_parse_hex_color(palette->app_bg.c_str());
    lv_color_t text_primary = theme_manager_parse_hex_color(palette->text.c_str());
    // Update overlay background color
    lv_obj_set_style_bg_color(theme_explorer_overlay_, app_bg, LV_PART_MAIN);

    // Update preview card backgrounds
    lv_obj_t* typography_card =
        lv_obj_find_by_name(theme_explorer_overlay_, "preview_typography_card");
    lv_obj_t* actions_card = lv_obj_find_by_name(theme_explorer_overlay_, "preview_actions_card");
    lv_obj_t* background_card = lv_obj_find_by_name(theme_explorer_overlay_, "preview_background");

    if (typography_card) {
        lv_obj_set_style_bg_color(typography_card, card_bg, LV_PART_MAIN);
        update_text_colors_recursive(typography_card, text_primary);
    }
    if (actions_card) {
        lv_obj_set_style_bg_color(actions_card, card_bg, LV_PART_MAIN);
        update_text_colors_recursive(actions_card, text_primary);
    }
    if (background_card) {
        lv_obj_set_style_bg_color(background_card, app_bg, LV_PART_MAIN);
        update_text_colors_recursive(background_card, text_primary);
    }

    // Update header action buttons text
    lv_obj_t* action_btn_2 = lv_obj_find_by_name(theme_explorer_overlay_, "action_button_2");
    if (action_btn_2) {
        update_text_colors_recursive(action_btn_2, text_primary);
    }
    lv_obj_t* action_btn = lv_obj_find_by_name(theme_explorer_overlay_, "action_button");
    if (action_btn) {
        update_text_colors_recursive(action_btn, text_primary);
    }

    // Also update the content area text
    lv_obj_t* overlay_content = lv_obj_find_by_name(theme_explorer_overlay_, "overlay_content");
    if (overlay_content) {
        update_text_colors_recursive(overlay_content, text_primary);
    }

    // Update header bar for complete preview
    lv_obj_t* header = lv_obj_find_by_name(theme_explorer_overlay_, "overlay_header");
    if (header) {
        // Header background should use app_bg, not card_bg
        lv_obj_set_style_bg_color(header, app_bg, LV_PART_MAIN);

        // Back button icon - ensure transparent background
        lv_obj_t* back_btn = lv_obj_find_by_name(header, "back_button");
        if (back_btn) {
            lv_obj_set_style_text_color(back_btn, text_primary, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
        }

        // Header title
        lv_obj_t* title = lv_obj_find_by_name(header, "header_title");
        if (title) {
            lv_obj_set_style_text_color(title, text_primary, LV_PART_MAIN);
        }

        // Action button (Apply) - update text color
        lv_obj_t* action_btn = lv_obj_find_by_name(header, "action_button");
        if (action_btn && !lv_obj_has_flag(action_btn, LV_OBJ_FLAG_HIDDEN)) {
            update_text_colors_recursive(action_btn, text_primary);
        }
    }

    // Update input widgets (dropdowns, textarea) - use card_alt for input backgrounds
    lv_color_t card_alt = theme_manager_parse_hex_color(palette->card_alt.c_str());
    lv_color_t border_color = theme_manager_parse_hex_color(palette->border.c_str());

    lv_obj_t* preset_dropdown =
        lv_obj_find_by_name(theme_explorer_overlay_, "theme_preset_dropdown");
    if (preset_dropdown) {
        lv_obj_set_style_bg_color(preset_dropdown, card_alt, LV_PART_MAIN);
        lv_obj_set_style_border_color(preset_dropdown, border_color, LV_PART_MAIN);
        lv_obj_set_style_text_color(preset_dropdown, text_primary, LV_PART_MAIN);
    }

    lv_obj_t* preview_dropdown = lv_obj_find_by_name(theme_explorer_overlay_, "preview_dropdown");
    if (preview_dropdown) {
        lv_obj_set_style_bg_color(preview_dropdown, card_alt, LV_PART_MAIN);
        lv_obj_set_style_text_color(preview_dropdown, text_primary, LV_PART_MAIN);
    }

    lv_obj_t* textarea = lv_obj_find_by_name(theme_explorer_overlay_, "preview_text_input");
    if (textarea) {
        lv_obj_set_style_bg_color(textarea, card_alt, LV_PART_MAIN);
        lv_obj_set_style_text_color(textarea, text_primary, LV_PART_MAIN);
    }

    // Update slider and switch colors
    lv_color_t primary = theme_manager_parse_hex_color(palette->primary.c_str());
    lv_color_t secondary = theme_manager_parse_hex_color(palette->secondary.c_str());

    lv_obj_t* slider = lv_obj_find_by_name(theme_explorer_overlay_, "preview_intensity_slider");
    if (slider) {
        lv_obj_set_style_bg_color(slider, border_color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, secondary, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, primary, LV_PART_KNOB);
        lv_obj_set_style_shadow_color(slider, app_bg, LV_PART_KNOB);
    }

    lv_obj_t* preview_switch = lv_obj_find_by_name(theme_explorer_overlay_, "preview_switch");
    if (preview_switch) {
        lv_obj_set_style_bg_color(preview_switch, border_color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(preview_switch, secondary, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(preview_switch, primary, LV_PART_KNOB);
    }

    // Update the dark mode toggle switch itself
    lv_obj_t* dark_mode_toggle =
        lv_obj_find_by_name(theme_explorer_overlay_, "preview_dark_mode_toggle");
    if (dark_mode_toggle) {
        lv_obj_t* inner_switch = lv_obj_find_by_name(dark_mode_toggle, "switch");
        if (inner_switch) {
            lv_obj_set_style_bg_color(inner_switch, border_color, LV_PART_MAIN);
            lv_obj_set_style_bg_color(inner_switch, secondary,
                                      LV_PART_INDICATOR | LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(inner_switch, primary, LV_PART_KNOB);
        }
    }

    spdlog::debug("[DisplaySettingsOverlay] Preview dark mode toggled to {} (local only)",
                  is_dark ? "dark" : "light");
}

void DisplaySettingsOverlay::show_theme_preview(lv_obj_t* parent_screen) {
    // Store parent screen for overlay creation
    parent_screen_ = parent_screen;

    // Register callbacks (idempotent - safe to call multiple times)
    register_callbacks();

    // Use the same flow as handle_theme_settings_clicked()
    handle_theme_settings_clicked();

    // Show and push the overlay (handle_theme_settings_clicked creates it hidden)
    if (theme_explorer_overlay_) {
        lv_obj_remove_flag(theme_explorer_overlay_, LV_OBJ_FLAG_HIDDEN);
        ui_nav_push_overlay(theme_explorer_overlay_);
    }
}

} // namespace helix::settings
