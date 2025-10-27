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

#include "ui_switch.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/others/xml/lv_xml.h"
#include "lvgl/src/others/xml/lv_xml_widget.h"
#include "lvgl/src/others/xml/lv_xml_parser.h"
#include "lvgl/src/others/xml/lv_xml_style.h"
#include "lvgl/src/others/xml/parsers/lv_xml_obj_parser.h"
#include <spdlog/spdlog.h>
#include <cstring>

/**
 * Register the ui_switch widget with LVGL's XML system
 *
 * TODO: XML widget registration temporarily disabled due to LVGL 9 API changes.
 * The following functions need updating to new LVGL 9 XML API:
 * - lv_xml_state_get_parent() -> new API
 * - lv_xml_state_get_item() -> new API
 * - lv_xml_obj_apply() -> new API
 * - lv_xml_widget_register() -> new API
 *
 * For now, switches should be created programmatically using lv_switch_create().
 */
void ui_switch_register()
{
    spdlog::warn("[Switch] XML registration temporarily disabled - use programmatic creation");
}
