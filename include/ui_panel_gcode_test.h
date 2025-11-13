// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 HelixScreen Contributors
 *
 * This file is part of HelixScreen, which is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * See <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_panel_gcode_test.h
 * @brief G-Code Viewer test/demonstration panel
 *
 * Test panel for the 3D G-code visualization widget. Provides:
 * - Full-screen G-code viewer
 * - Camera control buttons (preset views)
 * - Test file loading
 * - Statistics display
 *
 * Access via: ./build/bin/helix-ui-proto -p gcode-test
 */

/**
 * @brief Create G-code test panel
 * @param parent Parent LVGL object
 * @return Panel object or NULL on failure
 */
lv_obj_t* ui_panel_gcode_test_create(lv_obj_t* parent);

/**
 * @brief Cleanup G-code test panel resources
 */
void ui_panel_gcode_test_cleanup(void);

#ifdef __cplusplus
}
#endif
