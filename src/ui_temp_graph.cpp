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

#include "ui_temp_graph.h"
#include "ui_theme.h"
#include "../lvgl/src/widgets/chart/lv_chart_private.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Helper: Find series metadata by ID
static ui_temp_series_meta_t* find_series(ui_temp_graph_t* graph, int series_id) {
    if (!graph || series_id < 0 || series_id >= UI_TEMP_GRAPH_MAX_SERIES) {
        return nullptr;
    }

    for (int i = 0; i < graph->series_count; i++) {
        if (graph->series_meta[i].id == series_id && graph->series_meta[i].chart_series != nullptr) {
            return &graph->series_meta[i];
        }
    }
    return nullptr;
}

// Event callback for drawing gradient fills under curves
static void draw_gradient_fill_cb(lv_event_t* e) {
    lv_obj_t* chart = (lv_obj_t*)lv_event_get_target(e);
    ui_temp_graph_t* graph = (ui_temp_graph_t*)lv_event_get_user_data(e);

    if (!graph || !chart) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_MAIN) return;

    // Get layer for drawing
    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer) return;

    // Get chart dimensions
    lv_area_t chart_area;
    lv_obj_get_coords(chart, &chart_area);

    // Get chart padding to find data area
    lv_coord_t pad_left = lv_obj_get_style_pad_left(chart, LV_PART_MAIN);
    lv_coord_t pad_top = lv_obj_get_style_pad_top(chart, LV_PART_MAIN);
    lv_coord_t pad_right = lv_obj_get_style_pad_right(chart, LV_PART_MAIN);
    lv_coord_t pad_bottom = lv_obj_get_style_pad_bottom(chart, LV_PART_MAIN);

    lv_coord_t data_w = lv_area_get_width(&chart_area) - pad_left - pad_right;
    lv_coord_t data_h = lv_area_get_height(&chart_area) - pad_top - pad_bottom;

    // Data area coordinates
    lv_coord_t data_x1 = chart_area.x1 + pad_left;
    lv_coord_t data_y1 = chart_area.y1 + pad_top;
    lv_coord_t data_y2 = data_y1 + data_h;

    // Get point count from chart
    uint32_t point_cnt = lv_chart_get_point_count(chart);
    if (point_cnt == 0) return;

    // Draw gradient fill for each visible series
    for (int i = 0; i < graph->series_count; i++) {
        ui_temp_series_meta_t* meta = &graph->series_meta[i];
        if (!meta->visible || !meta->chart_series) continue;

        int32_t* y_points = meta->chart_series->y_points;
        if (!y_points) continue;

        // Calculate temperature range for Y mapping
        float temp_range = graph->max_temp - graph->min_temp;
        if (temp_range == 0.0f) continue;

        // Draw triangular strips to fill area under curve with gradient
        // We'll draw vertical strips for each segment between data points
        for (uint32_t pt = 0; pt < point_cnt - 1; pt++) {
            // Get Y values - skip if either point has no data
            int32_t y_val1 = y_points[pt];
            int32_t y_val2 = y_points[pt + 1];

            // Skip segments with no data (LV_CHART_POINT_NONE = INT32_MAX)
            if (y_val1 == LV_CHART_POINT_NONE || y_val2 == LV_CHART_POINT_NONE) {
                continue;
            }

            // Calculate X positions for this segment
            float x_frac1 = (float)pt / (float)(point_cnt - 1);
            float x_frac2 = (float)(pt + 1) / (float)(point_cnt - 1);
            lv_coord_t x1 = data_x1 + (lv_coord_t)(data_w * x_frac1);
            lv_coord_t x2 = data_x1 + (lv_coord_t)(data_w * x_frac2);

            // Map temperature values to Y pixel coordinates (inverted: high temp = low Y)
            float y_frac1 = (float)(y_val1 - (int32_t)graph->min_temp) / temp_range;
            float y_frac2 = (float)(y_val2 - (int32_t)graph->min_temp) / temp_range;

            lv_coord_t y1 = data_y2 - (lv_coord_t)(data_h * y_frac1);
            lv_coord_t y2 = data_y2 - (lv_coord_t)(data_h * y_frac2);

            // Clamp Y values to data area
            if (y1 < data_y1) y1 = data_y1;
            if (y2 < data_y1) y2 = data_y1;
            if (y1 > data_y2) y1 = data_y2;
            if (y2 > data_y2) y2 = data_y2;

            // Draw trapezoid (two triangles) for this segment
            // Triangle 1: (x1,y1) -> (x2,y2) -> (x1,data_y2)
            lv_draw_triangle_dsc_t tri1_dsc;
            lv_draw_triangle_dsc_init(&tri1_dsc);
            tri1_dsc.p[0].x = x1; tri1_dsc.p[0].y = y1;      // Top left
            tri1_dsc.p[1].x = x2; tri1_dsc.p[1].y = y2;      // Top right
            tri1_dsc.p[2].x = x1; tri1_dsc.p[2].y = data_y2; // Bottom left
            tri1_dsc.grad.dir = LV_GRAD_DIR_VER;
            tri1_dsc.grad.stops[0].color = meta->color;
            tri1_dsc.grad.stops[0].opa = meta->gradient_bottom_opa;
            tri1_dsc.grad.stops[0].frac = 0;
            tri1_dsc.grad.stops[1].color = meta->color;
            tri1_dsc.grad.stops[1].opa = meta->gradient_top_opa;
            tri1_dsc.grad.stops[1].frac = 255;
            tri1_dsc.grad.stops_count = 2;

            // Triangle 2: (x2,y2) -> (x2,data_y2) -> (x1,data_y2)
            lv_draw_triangle_dsc_t tri2_dsc;
            lv_draw_triangle_dsc_init(&tri2_dsc);
            tri2_dsc.p[0].x = x2; tri2_dsc.p[0].y = y2;      // Top right
            tri2_dsc.p[1].x = x2; tri2_dsc.p[1].y = data_y2; // Bottom right
            tri2_dsc.p[2].x = x1; tri2_dsc.p[2].y = data_y2; // Bottom left
            tri2_dsc.grad.dir = LV_GRAD_DIR_VER;
            tri2_dsc.grad.stops[0].color = meta->color;
            tri2_dsc.grad.stops[0].opa = meta->gradient_bottom_opa;
            tri2_dsc.grad.stops[0].frac = 0;
            tri2_dsc.grad.stops[1].color = meta->color;
            tri2_dsc.grad.stops[1].opa = meta->gradient_top_opa;
            tri2_dsc.grad.stops[1].frac = 255;
            tri2_dsc.grad.stops_count = 2;

            // Draw both triangles
            lv_draw_triangle(layer, &tri1_dsc);
            lv_draw_triangle(layer, &tri2_dsc);
        }
    }
}

// Create temperature graph widget
ui_temp_graph_t* ui_temp_graph_create(lv_obj_t* parent) {
    if (!parent) {
        printf("[TempGraph] Error: NULL parent\n");
        return nullptr;
    }

    // Allocate graph structure
    ui_temp_graph_t* graph = (ui_temp_graph_t*)malloc(sizeof(ui_temp_graph_t));
    if (!graph) {
        printf("[TempGraph] Error: Failed to allocate graph structure\n");
        return nullptr;
    }

    memset(graph, 0, sizeof(ui_temp_graph_t));

    // Initialize defaults
    graph->point_count = UI_TEMP_GRAPH_DEFAULT_POINTS;
    graph->min_temp = UI_TEMP_GRAPH_DEFAULT_MIN_TEMP;
    graph->max_temp = UI_TEMP_GRAPH_DEFAULT_MAX_TEMP;
    graph->series_count = 0;
    graph->next_series_id = 0;

    // Create LVGL chart
    graph->chart = lv_chart_create(parent);
    if (!graph->chart) {
        printf("[TempGraph] Error: Failed to create chart widget\n");
        free(graph);
        return nullptr;
    }

    // Configure chart
    lv_chart_set_type(graph->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(graph->chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(graph->chart, graph->point_count);

    // Set Y-axis range
    lv_chart_set_axis_range(graph->chart, LV_CHART_AXIS_PRIMARY_Y,
                            (int32_t)graph->min_temp, (int32_t)graph->max_temp);

    // Style chart background
    lv_obj_set_style_bg_color(graph->chart, UI_COLOR_PANEL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(graph->chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(graph->chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(graph->chart, 12, LV_PART_MAIN);

    // Style division lines - use LV_PART_MAIN but only for line properties
    lv_obj_set_style_line_color(graph->chart, lv_color_hex(0x505050), LV_PART_MAIN);
    lv_obj_set_style_line_width(graph->chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(graph->chart, LV_OPA_30, LV_PART_MAIN);  // Subtle - 30% opacity

    // Style data series lines
    lv_obj_set_style_line_width(graph->chart, 3, LV_PART_ITEMS);  // Thicker series lines
    lv_obj_set_style_line_opa(graph->chart, LV_OPA_COVER, LV_PART_ITEMS);  // Full opacity for series

    // Configure division line count
    lv_chart_set_div_line_count(graph->chart, 5, 10);  // 5 horizontal, 10 vertical division lines

    // Attach gradient draw callback
    lv_obj_add_event_cb(graph->chart, draw_gradient_fill_cb, LV_EVENT_DRAW_MAIN, graph);

    // Store graph pointer in chart user data for retrieval
    lv_obj_set_user_data(graph->chart, graph);

    printf("[TempGraph] Created: %d points, %.0f-%.0f°C range\n",
           graph->point_count, graph->min_temp, graph->max_temp);

    return graph;
}

// Destroy temperature graph widget
void ui_temp_graph_destroy(ui_temp_graph_t* graph) {
    if (!graph) return;

    // Remove all series (cursors will be cleaned up automatically)
    for (int i = 0; i < graph->series_count; i++) {
        if (graph->series_meta[i].chart_series) {
            lv_chart_remove_series(graph->chart, graph->series_meta[i].chart_series);
        }
    }

    // Delete chart widget
    if (graph->chart) {
        lv_obj_del(graph->chart);
    }

    free(graph);
    printf("[TempGraph] Destroyed\n");
}

// Get underlying chart widget
lv_obj_t* ui_temp_graph_get_chart(ui_temp_graph_t* graph) {
    return graph ? graph->chart : nullptr;
}

// Add a new temperature series
int ui_temp_graph_add_series(ui_temp_graph_t* graph, const char* name, lv_color_t color) {
    if (!graph || !name) {
        printf("[TempGraph] Error: NULL graph or name\n");
        return -1;
    }

    if (graph->series_count >= UI_TEMP_GRAPH_MAX_SERIES) {
        printf("[TempGraph] Error: Maximum series count (%d) reached\n", UI_TEMP_GRAPH_MAX_SERIES);
        return -1;
    }

    // Find next available slot
    int slot = -1;
    for (int i = 0; i < UI_TEMP_GRAPH_MAX_SERIES; i++) {
        if (graph->series_meta[i].chart_series == nullptr) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("[TempGraph] Error: No available series slots\n");
        return -1;
    }

    // Create LVGL chart series
    lv_chart_series_t* ser = lv_chart_add_series(graph->chart, color, LV_CHART_AXIS_PRIMARY_Y);
    if (!ser) {
        printf("[TempGraph] Error: Failed to create chart series\n");
        return -1;
    }

    // Initialize series metadata
    ui_temp_series_meta_t* meta = &graph->series_meta[slot];
    meta->id = graph->next_series_id++;
    meta->chart_series = ser;
    meta->color = color;
    strncpy(meta->name, name, sizeof(meta->name) - 1);
    meta->name[sizeof(meta->name) - 1] = '\0';
    meta->visible = true;
    meta->show_target = false;
    meta->target_temp = 0.0f;
    meta->gradient_bottom_opa = UI_TEMP_GRAPH_GRADIENT_BOTTOM_OPA;
    meta->gradient_top_opa = UI_TEMP_GRAPH_GRADIENT_TOP_OPA;

    // Create target temperature cursor (horizontal line, initially hidden)
    meta->target_cursor = lv_chart_add_cursor(graph->chart, color, LV_DIR_HOR);
    if (meta->target_cursor) {
        lv_chart_set_cursor_point(graph->chart, meta->target_cursor, ser, 0);
    }

    graph->series_count++;

    printf("[TempGraph] Added series %d '%s' (slot %d, color 0x%06X)\n",
           meta->id, meta->name, slot, lv_color_to_u32(color));

    return meta->id;
}

// Remove a temperature series
void ui_temp_graph_remove_series(ui_temp_graph_t* graph, int series_id) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    // Remove cursor (if exists)
    if (meta->target_cursor) {
        // LVGL doesn't have lv_chart_remove_cursor, cursor is auto-freed with chart
        meta->target_cursor = nullptr;
    }

    // Remove chart series
    lv_chart_remove_series(graph->chart, meta->chart_series);

    // Clear metadata
    memset(meta, 0, sizeof(ui_temp_series_meta_t));
    meta->chart_series = nullptr;

    graph->series_count--;

    printf("[TempGraph] Removed series %d (%d series remaining)\n", series_id, graph->series_count);
}

// Show or hide a series
void ui_temp_graph_show_series(ui_temp_graph_t* graph, int series_id, bool visible) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    meta->visible = visible;

    // Use LVGL's hidden flag for the series
    if (meta->chart_series) {
        meta->chart_series->hidden = visible ? 0 : 1;
    }

    lv_obj_invalidate(graph->chart);
    printf("[TempGraph] Series %d '%s' %s\n", series_id, meta->name, visible ? "shown" : "hidden");
}

// Add a single temperature point (push mode)
void ui_temp_graph_update_series(ui_temp_graph_t* graph, int series_id, float temp) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    // Add point to series (shifts old data left)
    lv_chart_set_next_value(graph->chart, meta->chart_series, (int32_t)temp);
}

// Replace all data points (array mode)
void ui_temp_graph_set_series_data(ui_temp_graph_t* graph, int series_id, const float* temps, int count) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta || !temps || count <= 0) {
        printf("[TempGraph] Error: Invalid parameters\n");
        return;
    }

    // Clear existing data
    lv_chart_series_t* ser = meta->chart_series;
    for (int i = 0; i < graph->point_count; i++) {
        ser->y_points[i] = LV_CHART_POINT_NONE;
    }

    // Copy new data (up to point_count)
    int points_to_copy = count > graph->point_count ? graph->point_count : count;
    for (int i = 0; i < points_to_copy; i++) {
        ser->y_points[i] = (lv_coord_t)temps[i];
    }

    lv_chart_refresh(graph->chart);
    printf("[TempGraph] Series %d '%s' data set (%d points)\n", series_id, meta->name, points_to_copy);
}

// Clear all data
void ui_temp_graph_clear(ui_temp_graph_t* graph) {
    if (!graph) return;

    for (int i = 0; i < graph->series_count; i++) {
        ui_temp_series_meta_t* meta = &graph->series_meta[i];
        if (meta->chart_series) {
            lv_chart_series_t* ser = meta->chart_series;
            for (int p = 0; p < graph->point_count; p++) {
                ser->y_points[p] = LV_CHART_POINT_NONE;
            }
        }
    }

    lv_chart_refresh(graph->chart);
    printf("[TempGraph] All data cleared\n");
}

// Clear data for a specific series
void ui_temp_graph_clear_series(ui_temp_graph_t* graph, int series_id) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    lv_chart_series_t* ser = meta->chart_series;
    for (int i = 0; i < graph->point_count; i++) {
        ser->y_points[i] = LV_CHART_POINT_NONE;
    }

    lv_chart_refresh(graph->chart);
    printf("[TempGraph] Series %d '%s' cleared\n", series_id, meta->name);
}

// Set target temperature and visibility
void ui_temp_graph_set_series_target(ui_temp_graph_t* graph, int series_id, float target, bool show) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    meta->target_temp = target;
    meta->show_target = show;

    if (meta->target_cursor && show) {
        // Update cursor position - set Y position to target temperature
        meta->target_cursor->pos.y = (lv_coord_t)target;
        meta->target_cursor->pos_set = 1;  // Mark position as manually set

        // Update cursor color (brighter version of series color)
        lv_color_t bright_color = lv_color_lighten(meta->color, LV_OPA_40);
        meta->target_cursor->color = bright_color;

        lv_obj_invalidate(graph->chart);
    }

    printf("[TempGraph] Series %d target: %.1f°C (%s)\n", series_id, target, show ? "shown" : "hidden");
}

// Show or hide target temperature line
void ui_temp_graph_show_target(ui_temp_graph_t* graph, int series_id, bool show) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    ui_temp_graph_set_series_target(graph, series_id, meta->target_temp, show);
}

// Set Y-axis temperature range
void ui_temp_graph_set_temp_range(ui_temp_graph_t* graph, float min, float max) {
    if (!graph || min >= max) {
        printf("[TempGraph] Error: Invalid temperature range\n");
        return;
    }

    graph->min_temp = min;
    graph->max_temp = max;

    lv_chart_set_axis_range(graph->chart, LV_CHART_AXIS_PRIMARY_Y, (int32_t)min, (int32_t)max);

    printf("[TempGraph] Temperature range set: %.0f - %.0f°C\n", min, max);
}

// Set point count
void ui_temp_graph_set_point_count(ui_temp_graph_t* graph, int count) {
    if (!graph || count <= 0) {
        printf("[TempGraph] Error: Invalid point count\n");
        return;
    }

    graph->point_count = count;
    lv_chart_set_point_count(graph->chart, count);

    printf("[TempGraph] Point count set: %d\n", count);
}

// Set gradient opacity for a series
void ui_temp_graph_set_series_gradient(ui_temp_graph_t* graph, int series_id, lv_opa_t bottom_opa, lv_opa_t top_opa) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        printf("[TempGraph] Error: Series %d not found\n", series_id);
        return;
    }

    meta->gradient_bottom_opa = bottom_opa;
    meta->gradient_top_opa = top_opa;

    lv_obj_invalidate(graph->chart);

    printf("[TempGraph] Series %d gradient: bottom=%d%%, top=%d%%\n",
           series_id, (bottom_opa * 100) / 255, (top_opa * 100) / 255);
}
