// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_moonraker_server_identify.cpp
 * @brief Unit tests for server.connection.identify functionality
 *
 * Tests the client identification flow that enables Moonraker to send
 * notifications like notify_filelist_changed to the client.
 */

#include "../../include/moonraker_client_mock.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "../catch_amalgamated.hpp"

// ============================================================================
// server.connection.identify Mock Handler Tests
// ============================================================================

TEST_CASE("MoonrakerClientMock handles server.connection.identify",
          "[moonraker][connection][identify]") {
    MoonrakerClientMock mock(MoonrakerClientMock::PrinterType::VORON_24);
    mock.connect("ws://mock/websocket", []() {}, []() {});

    SECTION("Identify returns successful response with connection_id") {
        std::atomic<bool> callback_invoked{false};
        int connection_id = -1;

        json identify_params = {{"client_name", "TestClient"},
                                {"version", "1.0.0"},
                                {"type", "display"},
                                {"url", "https://example.com"}};

        mock.send_jsonrpc(
            "server.connection.identify", identify_params,
            [&](json response) {
                // Verify response structure
                REQUIRE(response.contains("result"));
                REQUIRE(response["result"].contains("connection_id"));
                connection_id = response["result"]["connection_id"].get<int>();
                callback_invoked.store(true);
            },
            [](const MoonrakerError& err) { FAIL("Error callback invoked: " << err.message); });

        // Give the mock time to invoke callback
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        REQUIRE(callback_invoked.load());
        REQUIRE(connection_id >= 1000); // Mock starts at 1000
    }

    SECTION("Multiple identify calls return unique connection IDs") {
        std::vector<int> connection_ids;
        std::atomic<int> callbacks_received{0};

        for (int i = 0; i < 3; i++) {
            json params = {{"client_name", "Test"}, {"version", "1.0"}, {"type", "display"}};

            mock.send_jsonrpc(
                "server.connection.identify", params,
                [&](json response) {
                    connection_ids.push_back(response["result"]["connection_id"].get<int>());
                    callbacks_received.fetch_add(1);
                },
                [](const MoonrakerError&) {});
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(callbacks_received.load() == 3);
        REQUIRE(connection_ids.size() == 3);

        // Each ID should be unique
        std::set<int> unique_ids(connection_ids.begin(), connection_ids.end());
        REQUIRE(unique_ids.size() == 3);
    }

    mock.stop_temperature_simulation();
    mock.disconnect();
}

// ============================================================================
// Identify Integration with Discovery Flow
// ============================================================================

TEST_CASE("MoonrakerClientMock discover_printer doesn't fail due to identify",
          "[moonraker][connection][discovery]") {
    MoonrakerClientMock mock(MoonrakerClientMock::PrinterType::VORON_24);
    mock.connect("ws://mock/websocket", []() {}, []() {});

    SECTION("discover_printer completes successfully") {
        std::atomic<bool> discovery_complete{false};

        mock.discover_printer([&]() { discovery_complete.store(true); });

        // Mock's discover_printer is synchronous - callback should be invoked immediately
        REQUIRE(discovery_complete.load());

        // Verify discovery populated expected data
        REQUIRE_FALSE(mock.get_hostname().empty());
        REQUIRE_FALSE(mock.get_heaters().empty());
    }

    mock.stop_temperature_simulation();
    mock.disconnect();
}
