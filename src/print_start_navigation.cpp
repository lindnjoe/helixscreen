// SPDX-License-Identifier: GPL-3.0-or-later

#include "print_start_navigation.h"

#include "ui_nav_manager.h"
#include "ui_panel_print_status.h"

#include "app_globals.h"
#include "printer_state.h"

#include <spdlog/spdlog.h>

#include <lvgl.h>

namespace helix {

// Track previous state to detect transitions TO printing
static PrintJobState prev_print_state = PrintJobState::STANDBY;

// Callback for print state changes - auto-navigates to print status on print start
static void on_print_state_changed_for_navigation(lv_observer_t* observer, lv_subject_t* subject) {
    (void)observer;
    auto current = static_cast<PrintJobState>(lv_subject_get_int(subject));

    spdlog::trace("[PrintStartNav] State change: {} -> {}", static_cast<int>(prev_print_state),
                  static_cast<int>(current));

    // Check for transition TO printing from a non-printing state
    bool was_not_printing =
        (prev_print_state != PrintJobState::PRINTING && prev_print_state != PrintJobState::PAUSED);
    bool is_now_printing = (current == PrintJobState::PRINTING);

    if (was_not_printing && is_now_printing) {
        spdlog::debug("[PrintStartNav] Print started (navigation handled by print select panel)");

        // NOTE: Auto-navigation disabled - PrintSelectPanel handles navigation explicitly
        // Kept for potential future use (e.g., prints started via API/console)
#if 0
        // A print just started - check if we should auto-navigate
        auto& nav = NavigationManager::instance();
        ui_panel_id_t active_panel = nav.get_active();

        spdlog::debug("[PrintStartNav] Print started - active_panel={} (PRINT_SELECT={})",
                      static_cast<int>(active_panel), static_cast<int>(UI_PANEL_PRINT_SELECT));

        // Only auto-navigate if we're on the print select panel (user just started a print)
        // Don't auto-navigate from home or other panels - handles restart mid-print gracefully
        if (active_panel == UI_PANEL_PRINT_SELECT) {
            // Get the print status panel widget
            lv_obj_t* print_status_widget = get_global_print_status_panel().get_panel();

            if (print_status_widget) {
                // Make sure it's not already showing
                if (!nav.is_panel_in_stack(print_status_widget)) {
                    spdlog::info("[PrintStartNav] Auto-navigating to print status (print started "
                                 "from print select)");
                    nav.push_overlay(print_status_widget);
                } else {
                    spdlog::debug(
                        "[PrintStartNav] Print status already showing, skipping navigation");
                }
            } else {
                spdlog::warn("[PrintStartNav] Print status panel widget not available");
            }
        } else {
            spdlog::debug("[PrintStartNav] Not on print select panel, skipping auto-navigation");
        }
#endif
    }

    prev_print_state = current;
}

ObserverGuard init_print_start_navigation_observer() {
    // Initialize prev_print_state to current state to prevent false trigger on startup
    prev_print_state = static_cast<PrintJobState>(
        lv_subject_get_int(get_printer_state().get_print_state_enum_subject()));
    spdlog::debug("[PrintStartNav] Observer registered (initial state={})",
                  static_cast<int>(prev_print_state));
    return ObserverGuard(get_printer_state().get_print_state_enum_subject(),
                         on_print_state_changed_for_navigation, nullptr);
}

} // namespace helix
