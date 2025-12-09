// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_ams.h"

#include "ui_ams_slot.h"
#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_panel_common.h"
#include "ui_theme.h"

#include "ams_backend.h"
#include "ams_state.h"
#include "app_globals.h"
#include "moonraker_api.h"
#include "printer_state.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <memory>

// Global instance pointer for XML callback access
static AmsPanel* g_ams_panel_instance = nullptr;

// ============================================================================
// XML Event Callback Wrappers (for <event_cb> elements in XML)
// ============================================================================

static void on_unload_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    if (g_ams_panel_instance) {
        g_ams_panel_instance->handle_unload();
    }
}

static void on_home_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    if (g_ams_panel_instance) {
        g_ams_panel_instance->handle_home();
    }
}

// ============================================================================
// Construction
// ============================================================================

AmsPanel::AmsPanel(PrinterState& printer_state, MoonrakerAPI* api) : PanelBase(printer_state, api) {
    spdlog::debug("[AmsPanel] Constructed");
}

// ============================================================================
// PanelBase Interface
// ============================================================================

void AmsPanel::init_subjects() {
    if (subjects_initialized_) {
        spdlog::warn("[{}] init_subjects() called twice - ignoring", get_name());
        return;
    }

    // AmsState handles all subject registration centrally
    // We just ensure it's initialized before panel creation
    AmsState::instance().init_subjects(true);

    // Create and connect backend if not already present
    if (!AmsState::instance().get_backend()) {
        // Factory method checks should_mock_ams() and creates appropriate backend
        auto backend = AmsBackend::create(AmsType::NONE);
        if (backend) {
            // Start backend BEFORE set_backend to avoid deadlock:
            // start() emits events while holding its internal mutex, and the callback
            // would call sync_from_backend()->get_system_info() which needs the same mutex.
            // By starting first, the event fires with no callback registered.
            backend->start();
            // Now register the backend (sets up callback for future events)
            AmsState::instance().set_backend(std::move(backend));
            // Manually sync state since we started before callback was registered
            AmsState::instance().sync_from_backend();
            spdlog::info("[{}] Created and connected AMS backend", get_name());
        }
    }

    // Register observers for state changes
    gates_version_observer_ = ObserverGuard(AmsState::instance().get_gates_version_subject(),
                                            on_gates_version_changed, this);

    action_observer_ =
        ObserverGuard(AmsState::instance().get_ams_action_subject(), on_action_changed, this);

    current_gate_observer_ = ObserverGuard(AmsState::instance().get_current_gate_subject(),
                                           on_current_gate_changed, this);

    // Gate count observer for dynamic slot creation
    gate_count_observer_ =
        ObserverGuard(AmsState::instance().get_gate_count_subject(), on_gate_count_changed, this);

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized via AmsState + observers registered", get_name());
}

void AmsPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    spdlog::debug("[{}] Setting up...", get_name());

    // Use standard overlay panel setup (header bar, responsive padding)
    ui_overlay_panel_setup_standard(panel_, parent_screen_, "overlay_header", "overlay_content");

    // Setup UI components
    setup_slots();
    setup_action_buttons();
    setup_status_display();

    // Initial UI sync from backend state
    refresh_slots();

    spdlog::debug("[{}] Setup complete!", get_name());
}

void AmsPanel::on_activate() {
    spdlog::debug("[{}] Activated - syncing from backend", get_name());

    // Sync state when panel becomes visible
    AmsState::instance().sync_from_backend();
    refresh_slots();
}

void AmsPanel::on_deactivate() {
    spdlog::debug("[{}] Deactivated", get_name());
    // Nothing to pause for now
}

// ============================================================================
// Setup Helpers
// ============================================================================

void AmsPanel::setup_slots() {
    slot_grid_ = lv_obj_find_by_name(panel_, "slot_grid");
    if (!slot_grid_) {
        spdlog::warn("[{}] slot_grid not found in XML", get_name());
        return;
    }

    // Get initial gate count and create slots
    int gate_count = lv_subject_get_int(AmsState::instance().get_gate_count_subject());
    create_slots(gate_count);
}

void AmsPanel::create_slots(int count) {
    if (!slot_grid_) {
        return;
    }

    // Clamp to reasonable range
    if (count < 0) {
        count = 0;
    }
    if (count > MAX_VISIBLE_SLOTS) {
        spdlog::warn("[{}] Clamping gate_count {} to max {}", get_name(), count, MAX_VISIBLE_SLOTS);
        count = MAX_VISIBLE_SLOTS;
    }

    // Skip if unchanged
    if (count == current_slot_count_) {
        return;
    }

    spdlog::debug("[{}] Creating {} slots (was {})", get_name(), count, current_slot_count_);

    // Delete existing slots
    for (int i = 0; i < current_slot_count_; ++i) {
        if (slot_widgets_[i]) {
            lv_obj_delete(slot_widgets_[i]);
            slot_widgets_[i] = nullptr;
        }
    }

    // Create new slots via XML system (widget handles its own sizing/appearance)
    for (int i = 0; i < count; ++i) {
        lv_obj_t* slot = static_cast<lv_obj_t*>(lv_xml_create(slot_grid_, "ams_slot", nullptr));
        if (!slot) {
            spdlog::error("[{}] Failed to create ams_slot for index {}", get_name(), i);
            continue;
        }

        // Configure slot index (triggers reactive binding setup)
        ui_ams_slot_set_index(slot, i);

        // Store reference and setup click handler
        slot_widgets_[i] = slot;
        lv_obj_set_user_data(slot, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(slot, on_slot_clicked, LV_EVENT_CLICKED, this);
    }

    current_slot_count_ = count;
    spdlog::info("[{}] Created {} slot widgets", get_name(), count);
}

void AmsPanel::on_gate_count_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self || !self->panel_) {
        return;
    }

    int new_count = lv_subject_get_int(subject);
    spdlog::debug("[AmsPanel] Gate count changed to {}", new_count);
    self->create_slots(new_count);
}

void AmsPanel::setup_action_buttons() {
    // Register XML event callbacks for buttons
    // These callbacks are referenced in ams_panel.xml via <event_cb> elements
    lv_xml_register_event_cb(nullptr, "ams_unload_clicked_cb", on_unload_clicked_xml);
    lv_xml_register_event_cb(nullptr, "ams_home_clicked_cb", on_home_clicked_xml);

    // Store panel pointer for static callbacks to access
    g_ams_panel_instance = this;

    spdlog::debug("[{}] Action button callbacks registered", get_name());
}

void AmsPanel::setup_status_display() {
    // Status display is handled reactively via bind_text in XML
    // Just verify the elements exist
    lv_obj_t* status_label = lv_obj_find_by_name(panel_, "status_label");
    if (status_label) {
        spdlog::debug("[{}] Status label found - bound to ams_action_detail", get_name());
    }
}

// ============================================================================
// Public API
// ============================================================================

void AmsPanel::refresh_slots() {
    if (!panel_ || !subjects_initialized_) {
        return;
    }

    update_slot_colors();

    // Update current gate highlight
    int current_gate = lv_subject_get_int(AmsState::instance().get_current_gate_subject());
    update_current_gate_highlight(current_gate);
}

// ============================================================================
// UI Update Handlers
// ============================================================================

void AmsPanel::update_slot_colors() {
    int gate_count = lv_subject_get_int(AmsState::instance().get_gate_count_subject());
    AmsBackend* backend = AmsState::instance().get_backend();

    for (int i = 0; i < MAX_VISIBLE_SLOTS; ++i) {
        if (!slot_widgets_[i]) {
            continue;
        }

        if (i >= gate_count) {
            // Hide slots beyond configured count
            lv_obj_add_flag(slot_widgets_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_remove_flag(slot_widgets_[i], LV_OBJ_FLAG_HIDDEN);

        // Get gate color from AmsState subject
        lv_subject_t* color_subject = AmsState::instance().get_gate_color_subject(i);
        if (color_subject) {
            uint32_t rgb = static_cast<uint32_t>(lv_subject_get_int(color_subject));
            lv_color_t color = lv_color_hex(rgb);

            // Find color swatch within slot
            lv_obj_t* swatch = lv_obj_find_by_name(slot_widgets_[i], "color_swatch");
            if (swatch) {
                lv_obj_set_style_bg_color(swatch, color, 0);
            }
        }

        // Update material label and fill level from backend gate info
        if (backend) {
            GateInfo gate_info = backend->get_gate_info(i);
            lv_obj_t* material_label = lv_obj_find_by_name(slot_widgets_[i], "material_label");
            if (material_label) {
                if (!gate_info.material.empty()) {
                    lv_label_set_text(material_label, gate_info.material.c_str());
                } else {
                    lv_label_set_text(material_label, "---");
                }
            }

            // Set fill level from Spoolman weight data
            if (gate_info.total_weight_g > 0.0f) {
                float fill_level = gate_info.remaining_weight_g / gate_info.total_weight_g;
                ui_ams_slot_set_fill_level(slot_widgets_[i], fill_level);
            }
        }

        // Update status indicator
        update_slot_status(i);
    }
}

void AmsPanel::update_slot_status(int gate_index) {
    if (gate_index < 0 || gate_index >= MAX_VISIBLE_SLOTS || !slot_widgets_[gate_index]) {
        return;
    }

    lv_subject_t* status_subject = AmsState::instance().get_gate_status_subject(gate_index);
    if (!status_subject) {
        return;
    }

    auto status = static_cast<GateStatus>(lv_subject_get_int(status_subject));

    // Find status indicator icon within slot
    lv_obj_t* status_icon = lv_obj_find_by_name(slot_widgets_[gate_index], "status_icon");
    if (!status_icon) {
        return;
    }

    // Update icon based on status
    switch (status) {
    case GateStatus::EMPTY:
        // Show empty indicator
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_30, 0);
        break;

    case GateStatus::AVAILABLE:
    case GateStatus::FROM_BUFFER:
        // Show filament available
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_100, 0);
        break;

    case GateStatus::LOADED:
        // Show loaded (highlighted)
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_100, 0);
        break;

    case GateStatus::BLOCKED:
        // Show error state
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_100, 0);
        break;

    case GateStatus::UNKNOWN:
    default:
        lv_obj_add_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

void AmsPanel::update_action_display(AmsAction action) {
    // Action display is handled via bind_text to ams_action_detail
    // This method can add visual feedback (progress indicators, etc.)

    lv_obj_t* progress = lv_obj_find_by_name(panel_, "action_progress");
    if (!progress) {
        return;
    }

    bool show_progress = (action == AmsAction::LOADING || action == AmsAction::UNLOADING ||
                          action == AmsAction::SELECTING || action == AmsAction::HOMING);

    if (show_progress) {
        lv_obj_remove_flag(progress, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(progress, LV_OBJ_FLAG_HIDDEN);
    }
}

void AmsPanel::update_current_gate_highlight(int gate_index) {
    // Remove highlight from all slots (set border opacity to 0)
    for (int i = 0; i < MAX_VISIBLE_SLOTS; ++i) {
        if (slot_widgets_[i]) {
            lv_obj_remove_state(slot_widgets_[i], LV_STATE_CHECKED);
            lv_obj_set_style_border_opa(slot_widgets_[i], LV_OPA_0, 0);
        }
    }

    // Add highlight to current gate (show border)
    if (gate_index >= 0 && gate_index < MAX_VISIBLE_SLOTS && slot_widgets_[gate_index]) {
        lv_obj_add_state(slot_widgets_[gate_index], LV_STATE_CHECKED);
        lv_obj_set_style_border_opa(slot_widgets_[gate_index], LV_OPA_100, 0);
    }

    // Update the "Currently Loaded" card in the right column
    update_current_loaded_display(gate_index);
}

void AmsPanel::update_current_loaded_display(int gate_index) {
    if (!panel_) {
        return;
    }

    // Find the "Currently Loaded" card elements
    lv_obj_t* current_swatch = lv_obj_find_by_name(panel_, "current_swatch");
    lv_obj_t* current_material = lv_obj_find_by_name(panel_, "current_material");
    lv_obj_t* current_slot_label = lv_obj_find_by_name(panel_, "current_slot_label");

    AmsBackend* backend = AmsState::instance().get_backend();
    bool filament_loaded =
        lv_subject_get_int(AmsState::instance().get_filament_loaded_subject()) != 0;

    if (gate_index >= 0 && filament_loaded && backend) {
        // Filament is loaded - show the loaded gate info
        GateInfo gate_info = backend->get_gate_info(gate_index);

        // Set swatch color
        if (current_swatch) {
            lv_color_t color = lv_color_hex(gate_info.color_rgb);
            lv_obj_set_style_bg_color(current_swatch, color, 0);
            lv_obj_set_style_border_color(current_swatch, color, 0);
        }

        // Set material name
        if (current_material) {
            if (!gate_info.material.empty()) {
                lv_label_set_text(current_material, gate_info.material.c_str());
            } else {
                lv_label_set_text(current_material, "Filament");
            }
        }

        // Set slot label (1-based for user display)
        if (current_slot_label) {
            char buf[16];
            snprintf(buf, sizeof(buf), "Slot %d", gate_index + 1);
            lv_label_set_text(current_slot_label, buf);
        }
    } else {
        // No filament loaded - show empty state
        if (current_swatch) {
            lv_obj_set_style_bg_color(current_swatch, lv_color_hex(0x505050), 0);
            lv_obj_set_style_border_color(current_swatch, lv_color_hex(0x505050), 0);
        }

        if (current_material) {
            lv_label_set_text(current_material, "---");
        }

        if (current_slot_label) {
            lv_label_set_text(current_slot_label, "None");
        }
    }
}

// ============================================================================
// Event Callbacks
// ============================================================================

void AmsPanel::on_slot_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_slot_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        lv_obj_t* slot = static_cast<lv_obj_t*>(lv_event_get_target(e));
        auto slot_index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(slot)));
        self->handle_slot_tap(slot_index);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_unload_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_unload_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_unload();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_home_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_home_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_home();
    }
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// Observer Callbacks
// ============================================================================

void AmsPanel::on_gates_version_changed(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    spdlog::debug("[AmsPanel] Gates version changed - refreshing slots");
    self->refresh_slots();
}

void AmsPanel::on_action_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    auto action = static_cast<AmsAction>(lv_subject_get_int(subject));
    spdlog::debug("[AmsPanel] Action changed: {}", ams_action_to_string(action));
    self->update_action_display(action);
}

void AmsPanel::on_current_gate_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    int gate = lv_subject_get_int(subject);
    spdlog::debug("[AmsPanel] Current gate changed: {}", gate);
    self->update_current_gate_highlight(gate);
}

// ============================================================================
// Action Handlers
// ============================================================================

void AmsPanel::handle_slot_tap(int slot_index) {
    spdlog::info("[{}] Slot {} tapped", get_name(), slot_index);

    // Validate slot index against configured gate count
    int gate_count = lv_subject_get_int(AmsState::instance().get_gate_count_subject());
    if (slot_index < 0 || slot_index >= gate_count) {
        spdlog::warn("[{}] Invalid slot index {} (gate_count={})", get_name(), slot_index,
                     gate_count);
        return;
    }

    // Show context menu near the tapped slot
    if (slot_index >= 0 && slot_index < MAX_VISIBLE_SLOTS && slot_widgets_[slot_index]) {
        show_context_menu(slot_index, slot_widgets_[slot_index]);
    }
}

void AmsPanel::handle_unload() {
    spdlog::info("[{}] Unload requested", get_name());

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->unload_filament();
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Unload failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_home() {
    spdlog::info("[{}] Home requested", get_name());

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->home();
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Home failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_context_load() {
    if (context_menu_slot_ < 0) {
        return;
    }

    spdlog::info("[{}] Context menu: Load from slot {}", get_name(), context_menu_slot_);
    hide_context_menu();

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    // Check if backend is busy
    AmsSystemInfo info = backend->get_system_info();
    if (info.action != AmsAction::IDLE && info.action != AmsAction::ERROR) {
        NOTIFY_WARNING("AMS is busy: {}", ams_action_to_string(info.action));
        return;
    }

    AmsError error = backend->load_filament(context_menu_slot_);
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Load failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_context_unload() {
    if (context_menu_slot_ < 0) {
        return;
    }

    spdlog::info("[{}] Context menu: Unload slot {}", get_name(), context_menu_slot_);
    hide_context_menu();

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->unload_filament();
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Unload failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_context_edit() {
    if (context_menu_slot_ < 0) {
        return;
    }

    spdlog::info("[{}] Context menu: Edit slot {}", get_name(), context_menu_slot_);
    hide_context_menu();

    // TODO: Phase 3 - Open edit modal with Spoolman integration
    NOTIFY_INFO("Edit feature coming in Phase 3");
}

// ============================================================================
// Context Menu Management
// ============================================================================

void AmsPanel::show_context_menu(int slot_index, lv_obj_t* near_widget) {
    // Hide any existing context menu first
    hide_context_menu();

    if (!parent_screen_ || !near_widget) {
        return;
    }

    // Store which slot the menu is for
    context_menu_slot_ = slot_index;

    // Create context menu from XML component
    context_menu_ =
        static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "ams_context_menu", nullptr));
    if (!context_menu_) {
        spdlog::error("[{}] Failed to create context menu", get_name());
        return;
    }

    // Find and wire up callbacks
    lv_obj_t* backdrop = lv_obj_find_by_name(context_menu_, "context_backdrop");
    lv_obj_t* menu_card = lv_obj_find_by_name(context_menu_, "context_menu");
    lv_obj_t* btn_load = lv_obj_find_by_name(context_menu_, "btn_load");
    lv_obj_t* btn_unload = lv_obj_find_by_name(context_menu_, "btn_unload");
    lv_obj_t* btn_edit = lv_obj_find_by_name(context_menu_, "btn_edit");

    if (backdrop) {
        lv_obj_add_event_cb(backdrop, on_context_backdrop_clicked, LV_EVENT_CLICKED, this);
    }
    if (btn_load) {
        lv_obj_add_event_cb(btn_load, on_context_load_clicked, LV_EVENT_CLICKED, this);
    }
    if (btn_unload) {
        lv_obj_add_event_cb(btn_unload, on_context_unload_clicked, LV_EVENT_CLICKED, this);
    }
    if (btn_edit) {
        lv_obj_add_event_cb(btn_edit, on_context_edit_clicked, LV_EVENT_CLICKED, this);
    }

    // Position the menu card near the tapped widget
    if (menu_card) {
        // Update layout to get accurate dimensions before positioning
        lv_obj_update_layout(menu_card);

        // Get the position of the slot widget in screen coordinates
        lv_point_t slot_pos;
        lv_obj_get_coords(near_widget, (lv_area_t*)&slot_pos);

        // Position menu to the right of the slot, or left if near edge
        int32_t screen_width = lv_obj_get_width(parent_screen_);
        int32_t menu_width = lv_obj_get_width(menu_card);
        int32_t slot_center_x = slot_pos.x + lv_obj_get_width(near_widget) / 2;
        int32_t slot_center_y = slot_pos.y + lv_obj_get_height(near_widget) / 2;

        int32_t menu_x = slot_center_x + 20; // Position to the right
        if (menu_x + menu_width > screen_width - 10) {
            menu_x = slot_center_x - menu_width - 20; // Position to the left instead
        }

        // Vertical: center on the slot
        int32_t menu_y = slot_center_y - lv_obj_get_height(menu_card) / 2;

        // Clamp to screen bounds
        int32_t screen_height = lv_obj_get_height(parent_screen_);
        if (menu_y < 10)
            menu_y = 10;
        if (menu_y + lv_obj_get_height(menu_card) > screen_height - 10) {
            menu_y = screen_height - lv_obj_get_height(menu_card) - 10;
        }

        lv_obj_set_pos(menu_card, menu_x, menu_y);
    }

    spdlog::debug("[{}] Context menu shown for slot {}", get_name(), slot_index);
}

void AmsPanel::hide_context_menu() {
    if (context_menu_) {
        lv_obj_delete(context_menu_);
        context_menu_ = nullptr;
        context_menu_slot_ = -1;
        spdlog::debug("[{}] Context menu hidden", get_name());
    }
}

// ============================================================================
// Context Menu Callbacks
// ============================================================================

void AmsPanel::on_context_backdrop_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_backdrop_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->hide_context_menu();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_context_load_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_load_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_context_load();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_context_unload_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_unload_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_context_unload();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_context_edit_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_edit_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_context_edit();
    }
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<AmsPanel> g_ams_panel;

AmsPanel& get_global_ams_panel() {
    if (!g_ams_panel) {
        g_ams_panel = std::make_unique<AmsPanel>(get_printer_state(), nullptr);
    }
    return *g_ams_panel;
}
