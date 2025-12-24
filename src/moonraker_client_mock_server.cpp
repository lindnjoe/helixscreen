// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "moonraker_client_mock_internal.h"

#include <spdlog/spdlog.h>

namespace mock_internal {

void register_server_handlers(std::unordered_map<std::string, MethodHandler>& registry) {
    // server.connection.identify - Identify client to Moonraker for notifications
    // https://moonraker.readthedocs.io/en/latest/web_api/#identify-connection
    registry["server.connection.identify"] =
        [](MoonrakerClientMock* /*self*/, const json& params, std::function<void(json)> success_cb,
           std::function<void(const MoonrakerError&)> /*error_cb*/) -> bool {
        // Log the identification for debugging
        std::string client_name = params.value("client_name", "unknown");
        std::string version = params.value("version", "unknown");
        std::string type = params.value("type", "unknown");

        spdlog::debug("[MoonrakerClientMock] server.connection.identify: {} v{} ({})", client_name,
                      version, type);

        // Return a successful response with mock connection_id
        // This matches the real Moonraker response format
        static std::atomic<int> connection_counter{1000};
        json response = {{"jsonrpc", "2.0"},
                         {"result", {{"connection_id", connection_counter.fetch_add(1)}}}};

        if (success_cb) {
            success_cb(response);
        }
        return true;
    };

    spdlog::debug("[MoonrakerClientMock] Registered {} server method handlers", 1);
}

} // namespace mock_internal
