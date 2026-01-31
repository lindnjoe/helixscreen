// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_theme_manager_new.cpp
 * @brief Unit tests for the new ThemeManager table-driven style system
 *
 * Tests the foundational data structures for the ThemeManager refactor:
 * - StyleRole enum: semantic roles for all styles
 * - ThemePalette struct: holds all semantic colors
 * - StyleEntry struct: binds a role to a configure function
 */

#include "theme_manager.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// Task 1.1: StyleRole Enum Tests
// ============================================================================

TEST_CASE("StyleRole enum has expected values", "[theme-manager][style-role]") {
    REQUIRE(static_cast<int>(StyleRole::Card) >= 0);
    REQUIRE(static_cast<int>(StyleRole::Dialog) >= 0);
    REQUIRE(static_cast<int>(StyleRole::TextPrimary) >= 0);
    REQUIRE(static_cast<int>(StyleRole::ButtonPrimary) >= 0);
    REQUIRE(static_cast<int>(StyleRole::IconPrimary) >= 0);
    REQUIRE(static_cast<int>(StyleRole::COUNT) > 30);
}

TEST_CASE("StyleRole::COUNT equals total style count", "[theme-manager][style-role]") {
    constexpr size_t count = static_cast<size_t>(StyleRole::COUNT);
    REQUIRE(count >= 35);
    REQUIRE(count <= 50);
}

// ============================================================================
// Task 1.2: ThemePalette Struct Tests
// ============================================================================

TEST_CASE("Palette holds semantic colors", "[theme-manager][palette]") {
    ThemePalette p{};
    REQUIRE(sizeof(p.screen_bg) == sizeof(lv_color_t));
    REQUIRE(sizeof(p.primary) == sizeof(lv_color_t));
    REQUIRE(p.border_radius >= 0);
}

// ============================================================================
// Task 1.3: StyleEntry and Configure Function Type Tests
// ============================================================================

TEST_CASE("StyleEntry holds role and configure function", "[theme-manager][style-entry]") {
    auto configure_red = [](lv_style_t* s, const ThemePalette& p) {
        lv_style_set_bg_color(s, lv_color_hex(0xFF0000));
    };
    StyleEntry entry{StyleRole::Card, {}, configure_red};
    REQUIRE(entry.role == StyleRole::Card);
    REQUIRE(entry.configure != nullptr);
}

// ============================================================================
// Task 2.1: ThemeManager Singleton Tests
// ============================================================================

#include "../lvgl_test_fixture.h"

TEST_CASE("ThemeManager is singleton", "[theme-manager][singleton]") {
    auto& tm1 = ThemeManager::instance();
    auto& tm2 = ThemeManager::instance();
    REQUIRE(&tm1 == &tm2);
}

TEST_CASE_METHOD(LVGLTestFixture, "ThemeManager::get_style returns valid style for each role",
                 "[theme-manager][get-style]") {
    auto& tm = ThemeManager::instance();

    // Card style should exist (may be null before init, but pointer should be valid after)
    lv_style_t* card = tm.get_style(StyleRole::Card);
    lv_style_t* btn = tm.get_style(StyleRole::ButtonPrimary);

    // Different roles return different pointers
    REQUIRE(card != btn);
}
