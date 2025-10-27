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

#include "lvgl/lvgl.h"
#include "config.h"
#include "moonraker_client.h"

#include <functional>

/**
 * @brief First-Run Configuration Wizard
 *
 * Multi-step wizard for initial HelixScreen setup:
 * 1. Connect to Moonraker instance
 * 2. Auto-discover printer components
 * 3. Map components to UI defaults
 * 4. Save configuration
 */

/**
 * @brief Wizard steps enum
 */
enum class WizardStep {
  WIFI_SETUP = 0,
  CONNECTION = 1,
  PRINTER_IDENTIFY = 2,
  BED_SELECT = 3,
  HOTEND_SELECT = 4,
  FAN_SELECT = 5,
  LED_SELECT = 6,
  SUMMARY = 7,
  TOTAL_STEPS = 8
};

/**
 * @brief Initialize wizard subjects
 *
 * Must be called BEFORE creating XML components.
 */
void ui_wizard_init_subjects();

/**
 * @brief Create wizard UI
 *
 * Creates the wizard container and displays the first screen.
 *
 * @param parent Parent object (typically screen root)
 * @param config Config instance
 * @param mr_client MoonrakerClient instance for connection testing
 * @param on_complete Callback invoked when wizard completes successfully
 * @return The wizard root object
 */
lv_obj_t* ui_wizard_create(lv_obj_t* parent,
                            Config* config,
                            MoonrakerClient* mr_client,
                            std::function<void()> on_complete);

/**
 * @brief Navigate to specific wizard step
 *
 * @param step Step to navigate to
 */
void ui_wizard_goto_step(WizardStep step);

/**
 * @brief Navigate to next wizard step
 *
 * Validates current step before proceeding.
 */
void ui_wizard_next();

/**
 * @brief Navigate to previous wizard step
 */
void ui_wizard_back();

/**
 * @brief Get current wizard step
 *
 * @return Current step
 */
WizardStep ui_wizard_get_current_step();

/**
 * @brief Check if wizard is active
 *
 * @return true if wizard is currently shown
 */
bool ui_wizard_is_active();

/**
 * @brief Hide wizard (without completing)
 *
 * Used when user cancels or wizard is dismissed.
 */
void ui_wizard_hide();

/**
 * @brief Complete wizard and save configuration
 *
 * Saves all selections to config file and invokes completion callback.
 */
void ui_wizard_complete();
