// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_error_reporting.h"
#include "ui_notification.h"

#include "moonraker_api.h"
#include "moonraker_api_internal.h"
#include "spdlog/spdlog.h"

using namespace moonraker_internal;

// ============================================================================
// Job Control Operations
// ============================================================================

void MoonrakerAPI::start_print(const std::string& filename, SuccessCallback on_success,
                               ErrorCallback on_error) {
    // Validate filename path
    if (!is_safe_path(filename)) {
        NOTIFY_ERROR("Cannot start print. File '{}' has invalid path.", filename);
        if (on_error) {
            MoonrakerError err;
            err.type = MoonrakerErrorType::VALIDATION_ERROR;
            err.message = "Invalid filename contains directory traversal or illegal characters";
            err.method = "start_print";
            on_error(err);
        }
        return;
    }

    json params = {{"filename", filename}};

    spdlog::info("[Moonraker API] Starting print: {}", filename);

    client_.send_jsonrpc(
        "printer.print.start", params,
        [on_success](json) {
            spdlog::info("[Moonraker API] Print started successfully");
            on_success();
        },
        on_error);
}

void MoonrakerAPI::pause_print(SuccessCallback on_success, ErrorCallback on_error) {
    spdlog::info("[Moonraker API] Pausing print");

    client_.send_jsonrpc(
        "printer.print.pause", json::object(),
        [on_success](json) {
            spdlog::info("[Moonraker API] Print paused successfully");
            on_success();
        },
        on_error);
}

void MoonrakerAPI::resume_print(SuccessCallback on_success, ErrorCallback on_error) {
    spdlog::info("[Moonraker API] Resuming print");

    client_.send_jsonrpc(
        "printer.print.resume", json::object(),
        [on_success](json) {
            spdlog::info("[Moonraker API] Print resumed successfully");
            on_success();
        },
        on_error);
}

void MoonrakerAPI::cancel_print(SuccessCallback on_success, ErrorCallback on_error) {
    spdlog::info("[Moonraker API] Canceling print");

    client_.send_jsonrpc(
        "printer.print.cancel", json::object(),
        [on_success](json) {
            spdlog::info("[Moonraker API] Print canceled successfully");
            on_success();
        },
        on_error);
}

// ============================================================================
// Motion Control Operations
// ============================================================================

// ============================================================================
// Query Operations
// ============================================================================

void MoonrakerAPI::is_printer_ready(BoolCallback on_result, ErrorCallback on_error) {
    client_.send_jsonrpc(
        "printer.info", json::object(),
        [on_result](json response) {
            bool ready = false;
            if (response.contains("result") && response["result"].contains("state")) {
                std::string state = response["result"]["state"].get<std::string>();
                ready = (state == "ready");
            }
            on_result(ready);
        },
        on_error);
}

void MoonrakerAPI::get_print_state(StringCallback on_result, ErrorCallback on_error) {
    json params = {{"objects", json::object({{"print_stats", nullptr}})}};

    client_.send_jsonrpc(
        "printer.objects.query", params,
        [on_result](json response) {
            std::string state = "unknown";
            if (response.contains("result") && response["result"].contains("status") &&
                response["result"]["status"].contains("print_stats") &&
                response["result"]["status"]["print_stats"].contains("state")) {
                state = response["result"]["status"]["print_stats"]["state"].get<std::string>();
            }
            on_result(state);
        },
        on_error);
}
