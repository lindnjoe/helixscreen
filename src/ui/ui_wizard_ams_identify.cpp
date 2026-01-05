// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_wizard_ams_identify.h"

#include "ams_state.h"
#include "ams_types.h"
#include "lvgl/lvgl.h"
#include "static_panel_registry.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<WizardAmsIdentifyStep> g_wizard_ams_identify_step;

WizardAmsIdentifyStep* get_wizard_ams_identify_step() {
    if (!g_wizard_ams_identify_step) {
        g_wizard_ams_identify_step = std::make_unique<WizardAmsIdentifyStep>();
        StaticPanelRegistry::instance().register_destroy(
            "WizardAmsIdentifyStep", []() { g_wizard_ams_identify_step.reset(); });
    }
    return g_wizard_ams_identify_step.get();
}

void destroy_wizard_ams_identify_step() {
    g_wizard_ams_identify_step.reset();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

WizardAmsIdentifyStep::WizardAmsIdentifyStep() {
    spdlog::debug("[{}] Instance created", get_name());
}

WizardAmsIdentifyStep::~WizardAmsIdentifyStep() {
    // NOTE: Do NOT call LVGL functions here - LVGL may be destroyed first
    // NOTE: Do NOT log here - spdlog may be destroyed first
    screen_root_ = nullptr;
}

// ============================================================================
// Move Semantics
// ============================================================================

WizardAmsIdentifyStep::WizardAmsIdentifyStep(WizardAmsIdentifyStep&& other) noexcept
    : screen_root_(other.screen_root_) {
    other.screen_root_ = nullptr;
}

WizardAmsIdentifyStep& WizardAmsIdentifyStep::operator=(WizardAmsIdentifyStep&& other) noexcept {
    if (this != &other) {
        screen_root_ = other.screen_root_;
        other.screen_root_ = nullptr;
    }
    return *this;
}

// ============================================================================
// Subject Initialization (no-op for this step)
// ============================================================================

void WizardAmsIdentifyStep::init_subjects() {
    spdlog::debug("[{}] Initializing subjects (no-op)", get_name());
    // No subjects needed - this is a display-only step
}

// ============================================================================
// Callback Registration (no-op for this step)
// ============================================================================

void WizardAmsIdentifyStep::register_callbacks() {
    spdlog::debug("[{}] Register callbacks (no-op)", get_name());
    // No callbacks needed - this is a display-only step
}

// ============================================================================
// Screen Creation
// ============================================================================

lv_obj_t* WizardAmsIdentifyStep::create(lv_obj_t* parent) {
    spdlog::debug("[{}] Creating AMS identify screen", get_name());

    // Safety check: cleanup should have been called by wizard navigation
    if (screen_root_) {
        spdlog::warn("[{}] Screen pointer not null - cleanup may not have been called properly",
                     get_name());
        screen_root_ = nullptr; // Reset pointer, wizard framework handles deletion
    }

    // Create screen from XML
    screen_root_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "wizard_ams_identify", nullptr));
    if (!screen_root_) {
        spdlog::error("[{}] Failed to create screen from XML", get_name());
        return nullptr;
    }

    // Update display with current AMS info
    update_display();

    spdlog::debug("[{}] Screen created successfully", get_name());
    return screen_root_;
}

// ============================================================================
// Display Update
// ============================================================================

void WizardAmsIdentifyStep::update_display() {
    if (!screen_root_) {
        return;
    }

    // Find and update the AMS type label
    lv_obj_t* type_label = lv_obj_find_by_name(screen_root_, "ams_type_label");
    if (type_label) {
        std::string type_name = get_ams_type_name();
        lv_label_set_text(type_label, type_name.c_str());
        spdlog::debug("[{}] Set type label: {}", get_name(), type_name);
    }

    // Find and update the details label
    lv_obj_t* details_label = lv_obj_find_by_name(screen_root_, "ams_details_label");
    if (details_label) {
        std::string details = get_ams_details();
        lv_label_set_text(details_label, details.c_str());
        spdlog::debug("[{}] Set details label: {}", get_name(), details);
    }
}

std::string WizardAmsIdentifyStep::get_ams_type_name() const {
    auto& ams = AmsState::instance();
    AmsBackend* backend = ams.get_backend();

    if (!backend) {
        return "Unknown";
    }

    AmsType type = backend->get_type();
    switch (type) {
    case AmsType::AFC:
        return "AFC (Armored Turtle)";
    case AmsType::HAPPY_HARE:
        return "Happy Hare MMU";
    case AmsType::VALGACE:
        return "ValgACE (ACE Pro)";
    case AmsType::TOOL_CHANGER:
        return "Tool Changer";
    default:
        return "Unknown";
    }
}

std::string WizardAmsIdentifyStep::get_ams_details() const {
    auto& ams = AmsState::instance();
    AmsBackend* backend = ams.get_backend();

    if (!backend) {
        return "System detected";
    }

    AmsSystemInfo info = backend->get_system_info();
    if (info.total_slots > 0) {
        return std::to_string(info.total_slots) + " lanes detected";
    }

    return "System detected";
}

// ============================================================================
// Cleanup
// ============================================================================

void WizardAmsIdentifyStep::cleanup() {
    spdlog::debug("[{}] Cleaning up resources", get_name());

    // No config to save - this is a display-only step

    // Reset UI references
    // Note: Do NOT call lv_obj_del() here - the wizard framework handles
    // object deletion when clearing wizard_content container
    screen_root_ = nullptr;

    spdlog::debug("[{}] Cleanup complete", get_name());
}

// ============================================================================
// Validation
// ============================================================================

bool WizardAmsIdentifyStep::is_validated() const {
    // Always return true - this is a display-only step
    return true;
}

// ============================================================================
// Skip Logic
// ============================================================================

bool WizardAmsIdentifyStep::should_skip() const {
    auto& ams = AmsState::instance();
    AmsBackend* backend = ams.get_backend();

    // Skip if no backend available
    if (!backend) {
        spdlog::debug("[{}] No AMS backend, skipping step", get_name());
        return true;
    }

    AmsType type = backend->get_type();
    bool skip = (type == AmsType::NONE);

    if (skip) {
        spdlog::info("[{}] No AMS detected (type=NONE), skipping step", get_name());
    } else {
        spdlog::debug("[{}] AMS detected (type={}), showing step", get_name(),
                      static_cast<int>(type));
    }

    return skip;
}
