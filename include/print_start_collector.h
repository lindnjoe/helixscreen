// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "moonraker_client.h"
#include "printer_state.h"

#include <atomic>
#include <memory>
#include <regex>
#include <set>
#include <string>

/**
 * @file print_start_collector.h
 * @brief Monitors G-code responses to detect PRINT_START macro phases
 *
 * Subscribes to Moonraker's notify_gcode_response to parse G-code output
 * during print initialization. Detects common PRINT_START phases like
 * homing, heating, QGL, bed mesh, and purging through pattern matching.
 *
 * ## Usage
 * The collector is started when a print begins and stopped when the print
 * transitions to actual printing (or is cancelled). Progress is reported
 * through PrinterState subjects which XML can bind to directly.
 *
 * ## Pattern Detection
 * Uses best-effort regex matching on G-code responses. Not all macros will
 * output all phases - the progress calculation handles missing phases gracefully.
 *
 * @see PrintStartPhase enum in printer_state.h
 */
class PrintStartCollector : public std::enable_shared_from_this<PrintStartCollector> {
  public:
    /**
     * @brief Construct a PrintStartCollector
     * @param client MoonrakerClient for registering callbacks
     * @param state PrinterState to update with phase progress
     */
    PrintStartCollector(MoonrakerClient& client, PrinterState& state);

    ~PrintStartCollector();

    // Non-copyable
    PrintStartCollector(const PrintStartCollector&) = delete;
    PrintStartCollector& operator=(const PrintStartCollector&) = delete;

    /**
     * @brief Start monitoring for PRINT_START phases
     *
     * Registers for notify_gcode_response notifications and begins
     * parsing G-code output for phase detection patterns.
     */
    void start();

    /**
     * @brief Stop monitoring
     *
     * Unregisters callback and resets state. Called when print
     * initialization completes or print is cancelled.
     */
    void stop();

    /**
     * @brief Check if collector is currently active
     */
    [[nodiscard]] bool is_active() const {
        return active_.load();
    }

    /**
     * @brief Reset detected phases (for new print)
     */
    void reset();

  private:
    /**
     * @brief Phase pattern for regex matching
     */
    struct PhasePattern {
        PrintStartPhase phase;
        std::regex pattern;
        const char* message;
        int weight; // Progress weight (0-100, all phases should sum to 100)
    };

    /**
     * @brief Handle incoming G-code response
     */
    void on_gcode_response(const nlohmann::json& msg);

    /**
     * @brief Check line against phase patterns
     */
    void check_phase_patterns(const std::string& line);

    /**
     * @brief Update phase and recalculate progress
     */
    void update_phase(PrintStartPhase phase, const char* message);

    /**
     * @brief Calculate overall progress based on detected phases
     */
    int calculate_progress() const;

    /**
     * @brief Check for PRINT_START start marker
     */
    bool is_print_start_marker(const std::string& line) const;

    /**
     * @brief Check for print start completion (layer 1, etc.)
     */
    bool is_completion_marker(const std::string& line) const;

    // Dependencies
    MoonrakerClient& client_;
    PrinterState& state_;

    // Registration state
    std::string handler_name_;
    std::atomic<bool> active_{false};
    std::atomic<bool> registered_{false};

    // Phase tracking
    std::set<PrintStartPhase> detected_phases_;
    PrintStartPhase current_phase_ = PrintStartPhase::IDLE;
    bool print_start_detected_ = false;

    // Static phase patterns (initialized once)
    static const std::vector<PhasePattern> phase_patterns_;
    static const std::regex print_start_pattern_;
    static const std::regex completion_pattern_;
};
