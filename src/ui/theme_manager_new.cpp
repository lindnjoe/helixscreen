// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_manager.h"

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

void ThemeManager::init() {
    if (initialized_)
        return;

    register_style_configs();

    // Default palette for testing
    current_palette_.card_bg = lv_color_hex(0x2E3440);
    current_palette_.overlay_bg = lv_color_hex(0x3B4252);
    current_palette_.border = lv_color_hex(0x4C566A);
    current_palette_.text = lv_color_hex(0xECEFF4);
    current_palette_.primary = lv_color_hex(0x88C0D0);
    current_palette_.border_radius = 8;
    current_palette_.border_width = 1;
    current_palette_.border_opacity = 40;

    apply_palette(current_palette_);
    initialized_ = true;
}

void ThemeManager::shutdown() {
    for (auto& entry : styles_) {
        lv_style_reset(&entry.style);
    }
    initialized_ = false;
}

lv_style_t* ThemeManager::get_style(StyleRole role) {
    auto idx = static_cast<size_t>(role);
    if (idx >= styles_.size())
        return nullptr;
    return &styles_[idx].style;
}

void ThemeManager::register_style_configs() {
    // Initialize all styles
    for (size_t i = 0; i < styles_.size(); ++i) {
        styles_[i].role = static_cast<StyleRole>(i);
        lv_style_init(&styles_[i].style);
    }
    // Configure functions will be registered in Phase 3
}

void ThemeManager::apply_palette(const ThemePalette& palette) {
    current_palette_ = palette;
    for (auto& entry : styles_) {
        if (entry.configure) {
            lv_style_reset(&entry.style);
            entry.configure(&entry.style, palette);
        }
    }
}
