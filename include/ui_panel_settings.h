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

#include "lvgl/lvgl.h"

/**
 * @brief Settings Panel - Launcher and sub-screen management
 *
 * The settings panel provides a card-based launcher menu for accessing
 * different configuration and calibration screens (network, display, bed mesh).
 */

/**
 * @brief Initialize settings panel subjects
 *
 * Must be called BEFORE creating XML components.
 */
void ui_panel_settings_init_subjects();

/**
 * @brief Setup event handlers for settings panel launcher cards
 *
 * Wires click handlers to each launcher card.
 *
 * @param panel_obj The settings panel object returned from lv_xml_create()
 * @param screen The screen object (parent for overlay panels)
 */
void ui_panel_settings_wire_events(lv_obj_t* panel_obj, lv_obj_t* screen);

/**
 * @brief Get the settings panel object
 *
 * @return The settings panel object, or NULL if not created yet
 */
lv_obj_t* ui_panel_settings_get();

/**
 * @brief Set the settings panel object
 *
 * Called after XML creation to store reference.
 *
 * @param panel_obj The settings panel object
 */
void ui_panel_settings_set(lv_obj_t* panel_obj);
