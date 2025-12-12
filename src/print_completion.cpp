// SPDX-License-Identifier: GPL-3.0-or-later

#include "print_completion.h"

#include "ui_modal.h"
#include "ui_nav_manager.h"
#include "ui_panel_print_status.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_utils.h"

#include "app_globals.h"
#include "printer_state.h"
#include "settings_manager.h"

#include <spdlog/spdlog.h>

#include <lvgl.h>

namespace helix {

// Track previous state to detect transitions to terminal states
static PrintJobState prev_print_state = PrintJobState::STANDBY;

// Helper to format duration as "Xh YYm" or "Ym"
static void format_duration(int seconds, char* buf, size_t buf_size) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    if (hours > 0) {
        snprintf(buf, buf_size, "%dh %02dm", hours, minutes);
    } else {
        snprintf(buf, buf_size, "%dm", minutes);
    }
}

// Helper to show the rich print completion modal
static void show_rich_completion_modal(PrintJobState state, const char* filename) {
    auto& printer_state = get_printer_state();

    // Get print stats
    int duration_secs = lv_subject_get_int(printer_state.get_print_duration_subject());
    int total_layers = lv_subject_get_int(printer_state.get_print_layer_total_subject());

    // Determine icon colors and title based on state
    const char* icon_color = "#success_color";
    const char* title = "Print Complete";

    switch (state) {
    case PrintJobState::COMPLETE:
        icon_color = "#success_color";
        title = "Print Complete";
        break;
    case PrintJobState::CANCELLED:
        icon_color = "#warning_color";
        title = "Print Cancelled";
        break;
    case PrintJobState::ERROR:
        icon_color = "#error_color";
        title = "Print Failed";
        break;
    default:
        break;
    }

    // Create modal from XML
    lv_obj_t* modal = static_cast<lv_obj_t*>(
        lv_xml_create(lv_screen_active(), "print_completion_modal", nullptr));

    if (!modal) {
        spdlog::error("[PrintComplete] Failed to create print_completion_modal");
        return;
    }

    // Find and update the icon
    lv_obj_t* icon_widget = lv_obj_find_by_name(modal, "status_icon");
    if (icon_widget) {
        // Update icon source
        lv_obj_t* icon_label = lv_obj_get_child(icon_widget, 0);
        if (icon_label) {
            // Icon component stores the icon name, we need to update via the icon API
            // For now, just update the color - the icon is set in XML
            lv_color_t color = ui_theme_parse_color(icon_color);
            lv_obj_set_style_text_color(icon_label, color, LV_PART_MAIN);
        }
    }

    // Update title
    lv_obj_t* title_label = lv_obj_find_by_name(modal, "title_label");
    if (title_label) {
        lv_label_set_text(title_label, title);
    }

    // Update filename
    lv_obj_t* filename_label = lv_obj_find_by_name(modal, "filename_label");
    if (filename_label) {
        lv_label_set_text(filename_label, filename);
    }

    // Update duration
    lv_obj_t* duration_label = lv_obj_find_by_name(modal, "duration_label");
    if (duration_label) {
        char duration_buf[32];
        format_duration(duration_secs, duration_buf, sizeof(duration_buf));
        lv_label_set_text(duration_label, duration_buf);
    }

    // Update layers
    lv_obj_t* layers_label = lv_obj_find_by_name(modal, "layers_label");
    if (layers_label) {
        char layers_buf[32];
        snprintf(layers_buf, sizeof(layers_buf), "%d layers", total_layers);
        lv_label_set_text(layers_label, layers_buf);
    }

    // Hide filament stat for now (would need metadata fetch)
    lv_obj_t* filament_stat = lv_obj_find_by_name(modal, "filament_stat");
    if (filament_stat) {
        lv_obj_add_flag(filament_stat, LV_OBJ_FLAG_HIDDEN);
    }

    // Wire up OK button to dismiss modal
    lv_obj_t* ok_btn = lv_obj_find_by_name(modal, "btn_ok");
    if (ok_btn) {
        lv_obj_set_user_data(ok_btn, modal);
        lv_obj_add_event_cb(
            ok_btn,
            [](lv_event_t* e) {
                auto* btn = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
                auto* dlg = static_cast<lv_obj_t*>(lv_obj_get_user_data(btn));
                if (dlg) {
                    lv_obj_delete(dlg);
                }
            },
            LV_EVENT_CLICKED, nullptr);
    }

    // Also dismiss on backdrop click
    lv_obj_add_event_cb(
        modal,
        [](lv_event_t* e) {
            lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* current = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
            // Only close if clicking the backdrop itself, not child widgets
            if (target == current) {
                lv_obj_delete(current);
            }
        },
        LV_EVENT_CLICKED, nullptr);

    spdlog::info("[PrintComplete] Showing rich completion modal: {} ({})", title, filename);
}

// Callback for print state changes - triggers completion notifications
static void on_print_state_changed_for_notification(lv_observer_t* observer,
                                                    lv_subject_t* subject) {
    (void)observer;
    auto current = static_cast<PrintJobState>(lv_subject_get_int(subject));

    spdlog::debug("[PrintComplete] State change: {} -> {}", static_cast<int>(prev_print_state),
                  static_cast<int>(current));

    // Check for transitions to terminal states (from active print states)
    bool was_active =
        (prev_print_state == PrintJobState::PRINTING || prev_print_state == PrintJobState::PAUSED);
    bool is_terminal = (current == PrintJobState::COMPLETE || current == PrintJobState::CANCELLED ||
                        current == PrintJobState::ERROR);

    spdlog::debug("[PrintComplete] was_active={}, is_terminal={}", was_active, is_terminal);

    if (was_active && is_terminal) {
        auto mode = SettingsManager::instance().get_completion_alert_mode();
        if (mode == CompletionAlertMode::OFF) {
            spdlog::debug("[PrintComplete] Notification disabled");
            prev_print_state = current;
            return;
        }

        // Get filename from PrinterState and format for display
        // Resolve modified temp paths first, then get display name
        const char* raw_filename =
            lv_subject_get_string(get_printer_state().get_print_filename_subject());
        std::string resolved_filename =
            (raw_filename && raw_filename[0]) ? resolve_gcode_filename(raw_filename) : "";
        std::string display_name =
            !resolved_filename.empty() ? get_display_filename(resolved_filename) : "Unknown";

        // Wake display if sleeping
        SettingsManager::instance().wake_display();

        // Check if user is on print status panel (as an overlay)
        lv_obj_t* print_status_panel = get_global_print_status_panel().get_panel();
        bool on_print_status = NavigationManager::instance().is_panel_in_stack(print_status_panel);

        spdlog::info("[PrintComplete] Print {} - on_print_status={}, mode={}",
                     (current == PrintJobState::COMPLETE)    ? "complete"
                     : (current == PrintJobState::CANCELLED) ? "cancelled"
                                                             : "failed",
                     on_print_status, static_cast<int>(mode));

        // Failed prints ALWAYS show modal - users need to know something went wrong
        // Other states: toast on print status panel, modal elsewhere
        bool show_modal = (current == PrintJobState::ERROR) || !on_print_status;

        if (show_modal) {
            // Show rich modal requiring dismissal
            show_rich_completion_modal(current, display_name.c_str());
        } else {
            // User is on print status panel for complete/cancelled - show brief toast
            char message[128];
            ToastSeverity severity = ToastSeverity::SUCCESS;

            switch (current) {
            case PrintJobState::COMPLETE:
                snprintf(message, sizeof(message), "Print complete: %s", display_name.c_str());
                severity = ToastSeverity::SUCCESS;
                break;
            case PrintJobState::CANCELLED:
                snprintf(message, sizeof(message), "Print cancelled: %s", display_name.c_str());
                severity = ToastSeverity::WARNING;
                break;
            default:
                break;
            }

            ui_toast_show(severity, message, 5000);
        }
    }

    prev_print_state = current;
}

ObserverGuard init_print_completion_observer() {
    // Initialize prev_print_state to current state to prevent false trigger on startup
    prev_print_state = static_cast<PrintJobState>(
        lv_subject_get_int(get_printer_state().get_print_state_enum_subject()));
    spdlog::debug("[PrintComplete] Observer registered (initial state={})",
                  static_cast<int>(prev_print_state));
    return ObserverGuard(get_printer_state().get_print_state_enum_subject(),
                         on_print_state_changed_for_notification, nullptr);
}

} // namespace helix
