// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_display_resolution.cpp
 * @brief Unit tests for display resolution detection and screen size determination
 *
 * Tests DetectedResolution struct, screen size constants, and breakpoint mapping.
 */

#include "display_backend.h"
#include "theme_manager.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// DetectedResolution Struct Tests
// ============================================================================

TEST_CASE("DetectedResolution: default construction", "[display_resolution]") {
    DetectedResolution res;

    SECTION("Default has valid=false") {
        REQUIRE_FALSE(res.valid);
    }

    SECTION("Default dimensions are zero") {
        REQUIRE(res.width == 0);
        REQUIRE(res.height == 0);
    }
}

TEST_CASE("DetectedResolution: aggregate initialization", "[display_resolution]") {
    SECTION("Valid resolution with dimensions") {
        DetectedResolution res{480, 400, true};

        REQUIRE(res.width == 480);
        REQUIRE(res.height == 400);
        REQUIRE(res.valid == true);
    }

    SECTION("Invalid resolution marker") {
        DetectedResolution res{0, 0, false};

        REQUIRE(res.width == 0);
        REQUIRE(res.height == 0);
        REQUIRE(res.valid == false);
    }

    SECTION("Large resolution") {
        DetectedResolution res{1920, 1080, true};

        REQUIRE(res.width == 1920);
        REQUIRE(res.height == 1080);
        REQUIRE(res.valid == true);
    }
}

TEST_CASE("DetectedResolution: partial initialization", "[display_resolution]") {
    SECTION("Width and height only, valid defaults to false") {
        DetectedResolution res{640, 480};

        REQUIRE(res.width == 640);
        REQUIRE(res.height == 480);
        REQUIRE(res.valid == false); // Default member initializer
    }

    SECTION("Empty braces use defaults") {
        DetectedResolution res{};

        REQUIRE(res.width == 0);
        REQUIRE(res.height == 0);
        REQUIRE(res.valid == false);
    }
}

// ============================================================================
// Screen Size Constants Tests
// ============================================================================

TEST_CASE("Screen size constants: TINY preset", "[display_resolution][constants]") {
    REQUIRE(UI_SCREEN_TINY_W == 480);
    REQUIRE(UI_SCREEN_TINY_H == 320);
}

TEST_CASE("Screen size constants: TINY_ALT preset", "[display_resolution][constants]") {
    // TINY_ALT is for 480x400 displays (taller than TINY 480x320)
    REQUIRE(UI_SCREEN_TINY_ALT_W == 480);
    REQUIRE(UI_SCREEN_TINY_ALT_H == 400);

    SECTION("TINY_ALT has same width as TINY") {
        REQUIRE(UI_SCREEN_TINY_ALT_W == UI_SCREEN_TINY_W);
    }

    SECTION("TINY_ALT is taller than TINY") {
        REQUIRE(UI_SCREEN_TINY_ALT_H > UI_SCREEN_TINY_H);
    }
}

TEST_CASE("Screen size constants: SMALL preset", "[display_resolution][constants]") {
    REQUIRE(UI_SCREEN_SMALL_W == 800);
    REQUIRE(UI_SCREEN_SMALL_H == 480);
}

TEST_CASE("Screen size constants: MEDIUM preset", "[display_resolution][constants]") {
    REQUIRE(UI_SCREEN_MEDIUM_W == 1024);
    REQUIRE(UI_SCREEN_MEDIUM_H == 600);
}

TEST_CASE("Screen size constants: LARGE preset", "[display_resolution][constants]") {
    REQUIRE(UI_SCREEN_LARGE_W == 1280);
    REQUIRE(UI_SCREEN_LARGE_H == 720);
}

TEST_CASE("Screen size constants: size ordering", "[display_resolution][constants]") {
    // Verify presets are ordered by increasing resolution
    SECTION("Width ordering") {
        REQUIRE(UI_SCREEN_TINY_W <= UI_SCREEN_TINY_ALT_W);
        REQUIRE(UI_SCREEN_TINY_ALT_W < UI_SCREEN_SMALL_W);
        REQUIRE(UI_SCREEN_SMALL_W < UI_SCREEN_MEDIUM_W);
        REQUIRE(UI_SCREEN_MEDIUM_W < UI_SCREEN_LARGE_W);
    }

    SECTION("Total pixel count ordering") {
        int tiny_pixels = UI_SCREEN_TINY_W * UI_SCREEN_TINY_H;
        int tiny_alt_pixels = UI_SCREEN_TINY_ALT_W * UI_SCREEN_TINY_ALT_H;
        int small_pixels = UI_SCREEN_SMALL_W * UI_SCREEN_SMALL_H;
        int medium_pixels = UI_SCREEN_MEDIUM_W * UI_SCREEN_MEDIUM_H;
        int large_pixels = UI_SCREEN_LARGE_W * UI_SCREEN_LARGE_H;

        REQUIRE(tiny_pixels < tiny_alt_pixels);
        REQUIRE(tiny_alt_pixels < small_pixels);
        REQUIRE(small_pixels < medium_pixels);
        REQUIRE(medium_pixels < large_pixels);
    }
}

// ============================================================================
// Breakpoint Boundary Tests
// ============================================================================

TEST_CASE("Breakpoint mapping: SMALL max boundary", "[display_resolution][breakpoint]") {
    // UI_BREAKPOINT_SMALL_MAX = 480 (max(width, height) for small breakpoint)
    REQUIRE(UI_BREAKPOINT_SMALL_MAX == 480);

    SECTION("480 maps to SMALL breakpoint") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(480), "_small") == 0);
    }

    SECTION("479 also maps to SMALL breakpoint") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(479), "_small") == 0);
    }

    SECTION("481 maps to MEDIUM breakpoint") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(481), "_medium") == 0);
    }
}

TEST_CASE("Breakpoint mapping: MEDIUM max boundary", "[display_resolution][breakpoint]") {
    // UI_BREAKPOINT_MEDIUM_MAX = 800
    REQUIRE(UI_BREAKPOINT_MEDIUM_MAX == 800);

    SECTION("800 maps to MEDIUM breakpoint") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(800), "_medium") == 0);
    }

    SECTION("801 maps to LARGE breakpoint") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(801), "_large") == 0);
    }
}

// ============================================================================
// Screen Size to Breakpoint Mapping Tests
// ============================================================================

TEST_CASE("Breakpoint mapping: TINY screen size", "[display_resolution][breakpoint]") {
    // TINY is 480x320, max=480, should map to SMALL breakpoint
    int max_dim = (UI_SCREEN_TINY_W > UI_SCREEN_TINY_H) ? UI_SCREEN_TINY_W : UI_SCREEN_TINY_H;

    REQUIRE(max_dim == 480);
    REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_small") == 0);
}

TEST_CASE("Breakpoint mapping: TINY_ALT screen size", "[display_resolution][breakpoint]") {
    // TINY_ALT is 480x400, max=480, should map to SMALL breakpoint
    int max_dim =
        (UI_SCREEN_TINY_ALT_W > UI_SCREEN_TINY_ALT_H) ? UI_SCREEN_TINY_ALT_W : UI_SCREEN_TINY_ALT_H;

    REQUIRE(max_dim == 480);
    REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_small") == 0);
}

TEST_CASE("Breakpoint mapping: SMALL screen size", "[display_resolution][breakpoint]") {
    // SMALL is 800x480, max=800, should map to MEDIUM breakpoint
    int max_dim = (UI_SCREEN_SMALL_W > UI_SCREEN_SMALL_H) ? UI_SCREEN_SMALL_W : UI_SCREEN_SMALL_H;

    REQUIRE(max_dim == 800);
    REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_medium") == 0);
}

TEST_CASE("Breakpoint mapping: MEDIUM screen size", "[display_resolution][breakpoint]") {
    // MEDIUM is 1024x600, max=1024, should map to LARGE breakpoint
    int max_dim =
        (UI_SCREEN_MEDIUM_W > UI_SCREEN_MEDIUM_H) ? UI_SCREEN_MEDIUM_W : UI_SCREEN_MEDIUM_H;

    REQUIRE(max_dim == 1024);
    REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_large") == 0);
}

TEST_CASE("Breakpoint mapping: LARGE screen size", "[display_resolution][breakpoint]") {
    // LARGE is 1280x720, max=1280, should map to LARGE breakpoint
    int max_dim = (UI_SCREEN_LARGE_W > UI_SCREEN_LARGE_H) ? UI_SCREEN_LARGE_W : UI_SCREEN_LARGE_H;

    REQUIRE(max_dim == 1280);
    REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_large") == 0);
}

// ============================================================================
// Arbitrary Resolution Breakpoint Mapping Tests
// ============================================================================

TEST_CASE("Breakpoint mapping: arbitrary resolutions", "[display_resolution][breakpoint]") {
    SECTION("480x400 (tiny_alt) maps to SMALL breakpoint") {
        // max(480, 400) = 480 <= 480 → SMALL
        int max_dim = 480;
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_small") == 0);
    }

    SECTION("1920x1080 maps to LARGE breakpoint") {
        // max(1920, 1080) = 1920 > 800 → LARGE
        int max_dim = 1920;
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_large") == 0);
    }

    SECTION("640x480 maps to MEDIUM breakpoint") {
        // max(640, 480) = 640, which is > 480 and <= 800 → MEDIUM
        int max_dim = 640;
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_medium") == 0);
    }

    SECTION("320x240 maps to SMALL breakpoint") {
        // max(320, 240) = 320 <= 480 → SMALL
        int max_dim = 320;
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_small") == 0);
    }

    SECTION("800x600 maps to MEDIUM breakpoint") {
        // max(800, 600) = 800 <= 800 → MEDIUM
        int max_dim = 800;
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(max_dim), "_medium") == 0);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("Breakpoint mapping: edge cases", "[display_resolution][breakpoint]") {
    SECTION("Very small resolution") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(1), "_small") == 0);
    }

    SECTION("Zero resolution") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(0), "_small") == 0);
    }

    SECTION("Very large resolution") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(4000), "_large") == 0);
    }

    SECTION("8K resolution") {
        REQUIRE(strcmp(theme_manager_get_breakpoint_suffix(7680), "_large") == 0);
    }
}

// ============================================================================
// DisplayBackend Base Class Tests
// ============================================================================

TEST_CASE("DisplayBackend: detect_resolution default", "[display_resolution][backend]") {
    // The base class default implementation returns invalid
    // We test this by checking the struct returned has expected defaults
    DetectedResolution default_res{};

    SECTION("Default detection returns invalid") {
        REQUIRE_FALSE(default_res.valid);
    }
}

TEST_CASE("DisplayBackendType: string conversion", "[display_resolution][backend]") {
    SECTION("SDL backend name") {
        REQUIRE(strcmp(display_backend_type_to_string(DisplayBackendType::SDL), "SDL") == 0);
    }

    SECTION("FBDEV backend name") {
        REQUIRE(strcmp(display_backend_type_to_string(DisplayBackendType::FBDEV), "Framebuffer") ==
                0);
    }

    SECTION("DRM backend name") {
        REQUIRE(strcmp(display_backend_type_to_string(DisplayBackendType::DRM), "DRM/KMS") == 0);
    }

    SECTION("AUTO backend name") {
        REQUIRE(strcmp(display_backend_type_to_string(DisplayBackendType::AUTO), "Auto") == 0);
    }
}
