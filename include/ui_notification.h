// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "lvgl.h"
#include "ui_toast.h"

/**
 * @brief Unified notification API for HelixScreen
 *
 * Provides a high-level interface for showing notifications throughout the app.
 * Routes notifications to appropriate display mechanisms:
 * - Non-critical messages → toast notifications (auto-dismiss)
 * - Critical errors → modal dialogs (require acknowledgment)
 *
 * **Thread Safety:**
 * All functions are thread-safe. They automatically detect if called from a
 * background thread and use lv_async_call() to marshal to the LVGL main thread.
 * Safe to call from any thread (main thread, libhv callbacks, WiFi events, etc.).
 *
 * Also integrates with the reactive subject system so any module can emit
 * notifications without direct dependencies on UI code.
 */

/**
 * @brief Notification data structure for reactive subject
 *
 * Used to emit notifications via lv_subject_t for decoupled notification
 * from any module in the application.
 *
 * Usage:
 * ```cpp
 * #include "app_globals.h"
 * NotificationData notif = {severity, title, message, show_modal};
 * lv_subject_set_pointer(&get_notification_subject(), &notif);
 * ```
 */
typedef struct {
    ToastSeverity severity;  ///< Notification severity level
    const char* title;       ///< Title for modal dialogs (can be nullptr for toasts)
    const char* message;     ///< Notification message text
    bool show_modal;         ///< true = modal dialog, false = toast notification
} NotificationData;

/**
 * @brief Initialize the notification system
 *
 * Sets up subject observers and prepares the notification infrastructure.
 * Must be called during app initialization after app_globals_init_subjects().
 * Also captures the main thread ID for automatic thread-safety detection.
 */
void ui_notification_init();

/**
 * @brief Show an informational toast notification
 *
 * Displays a non-blocking blue toast message that auto-dismisses after 4 seconds.
 *
 * **Thread-safe**: Automatically detects if called from background thread and
 * marshals to LVGL main thread. Safe to call from any thread.
 *
 * @param message Message text to display
 */
void ui_notification_info(const char* message);

/**
 * @brief Show a success toast notification
 *
 * Displays a non-blocking green toast message that auto-dismisses after 4 seconds.
 *
 * **Thread-safe**: Automatically detects if called from background thread and
 * marshals to LVGL main thread. Safe to call from any thread.
 *
 * @param message Message text to display
 */
void ui_notification_success(const char* message);

/**
 * @brief Show a warning notification
 *
 * Displays a non-blocking orange toast message that auto-dismisses after 5 seconds.
 *
 * **Thread-safe**: Automatically detects if called from background thread and
 * marshals to LVGL main thread. Safe to call from any thread.
 *
 * @param message Message text to display
 */
void ui_notification_warning(const char* message);

/**
 * @brief Show an error notification
 *
 * Can display either a blocking modal dialog or a toast notification depending
 * on the modal parameter. Critical errors should use modal=true.
 *
 * **Thread-safe**: Automatically detects if called from background thread and
 * marshals to LVGL main thread. Safe to call from any thread.
 *
 * @param title Error title (used for modal dialogs, can be nullptr for toasts)
 * @param message Error message text
 * @param modal If true, shows blocking modal dialog; if false, shows toast
 */
void ui_notification_error(const char* title, const char* message, bool modal = true);
