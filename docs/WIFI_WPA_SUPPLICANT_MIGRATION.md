# WiFi Migration: wpa_supplicant + libhv Implementation Plan

**Date:** 2025-10-28
**Status:** PLANNED - Not yet implemented
**Target:** Replace mock/nmcli WiFi with production wpa_supplicant integration

---

## Executive Summary

Migrate WiFiManager from mock/nmcli to real wpa_supplicant using libhv async event loop. This brings the prototype in line with the parent GuppyScreen project's proven architecture and enables production deployment on embedded targets (Pi, K1, FlashForge).

**Key decisions:**
- ✅ Use wpa_supplicant (NOT nmcli) - universal Linux compatibility
- ✅ Use libhv event loop (NOT LVGL polling) - proven parent architecture
- ✅ Make both libhv and wpa_supplicant proper submodules - independent project lifecycle
- ✅ Port WpaEvent backend from parent - code reuse, proven patterns

---

## Why wpa_supplicant (Not nmcli)

### Decision Matrix

| Factor | wpa_supplicant | nmcli |
|--------|---------------|-------|
| **Embedded compatibility** | ✅ Universal (minimal Linux) | ❌ Requires NetworkManager |
| **Parent project uses** | ✅ Yes (proven working) | ❌ No |
| **Async events** | ✅ Real-time socket notifications | ❌ Must poll |
| **Architecture match** | ✅ Same as production code | ❌ Different implementation |
| **Process overhead** | ✅ Socket I/O only | ❌ Fork/exec per command |
| **Dependencies** | ✅ wpa_supplicant daemon only | ❌ NetworkManager + D-Bus + Python |
| **API stability** | ✅ wpa_ctrl C API | ❌ Text parsing of CLI output |
| **Target hardware** | ✅ Works on K1/FlashForge | ❌ May not be available |

**Verdict:** wpa_supplicant is the only choice that:
1. Works on all target hardware (including minimal embedded)
2. Matches parent GuppyScreen's proven architecture
3. Enables proper async event handling
4. Won't need rewrite when prototype becomes production

---

## Why libhv (Not LVGL Polling)

### Current libhv Usage

Already in use for Moonraker WebSocket client:
```cpp
class MoonrakerClient : public hv::WebSocketClient {
  // WebSocket to Klipper/Moonraker
  // Proven working on all platforms
}
```

### Parent Project Pattern

Parent GuppyScreen uses libhv for wpa_supplicant:
```cpp
class WpaEvent : private hv::EventLoopThread {
  // Async Unix socket monitoring
  // Event-driven, no polling
  // Thread-safe by design
}
```

**Location:** `../src/wpa_event.{h,cpp}` - can be ported almost verbatim

### Benefits Over LVGL Polling

| Approach | libhv Event Loop | LVGL Timer Polling |
|----------|------------------|-------------------|
| **CPU usage** | ✅ Idle until event | ❌ Constant polling overhead |
| **Latency** | ✅ Immediate notification | ❌ Depends on poll interval |
| **Thread safety** | ✅ Built-in | ❌ Manual synchronization |
| **Code complexity** | ✅ Simple callbacks | ❌ State machine in timer |
| **Proven** | ✅ Parent uses this | ❌ Custom implementation |
| **Consistency** | ✅ Same as Moonraker | ❌ Different pattern |

**Verdict:** libhv event loop is superior in every way and matches existing architecture.

---

## Future libhv Benefits

Once wpa_supplicant integration is done, libhv enables:

### Near-Term (Next 6 Months)
1. **Multiple printer connections** - Farm management UI
2. **Webcam streaming** - MJPEG over HTTP (view camera in UI)
3. **Firmware updates** - HTTP download + OTA flash

### Medium-Term (6-12 Months)
4. **Embedded REST API server** - Control UI from phone/tablet browser
5. **Network discovery** - Auto-detect printers on LAN (UDP broadcast)
6. **Cloud integration** - Upload prints to cloud, check for updates

### Long-Term (Future)
7. **Home automation integration** - REST API for Home Assistant, Node-RED
8. **Remote debugging** - WebSocket API for live state inspection
9. **Multi-protocol support** - Custom printer protocols beyond Moonraker

**Bottom line:** libhv is production-grade async networking that scales.

---

## Implementation Plan

### Stage 0: Convert libhv to Proper Submodule

**Current:** Symlink to parent (`libhv -> ../libhv`)
**Target:** Real git submodule

**Why:** Makes prototype-ui9 self-contained and CI/CD friendly

**Steps:**
```bash
# 1. Check parent's libhv commit
cd ../libhv && git rev-parse HEAD
# Output: <commit-hash>

# 2. Remove symlink
cd prototype-ui9
rm libhv

# 3. Add as submodule
git submodule add https://github.com/ithewei/libhv.git libhv

# 4. Pin to parent's version
cd libhv
git checkout <commit-hash>
cd ..

# 5. Update Makefile comment
# Change line 79: "# libhv (WebSocket client for Moonraker)"
# Remove "- symlinked from parent repo submodule"

# 6. Test build
make clean && make -j8

# 7. Commit
git add .gitmodules libhv Makefile
git commit -m "feat(deps): convert libhv from symlink to proper submodule"
```

**Success criteria:**
- ✅ `libhv/` is real submodule, not symlink
- ✅ Build works (MoonrakerClient still functions)
- ✅ `.gitmodules` has libhv entry

**Parent's libhv version:** v1.3.1-54-ga1d8185 (as of 2025-10-28)

---

### Stage 1: Add wpa_supplicant Submodule

**Target:** wpa_supplicant v2.11 (latest stable, released 2024-07-20)

**Steps:**
```bash
# 1. Add submodule
git submodule add git://w1.fi/hostap.git wpa_supplicant

# 2. Checkout v2.11
cd wpa_supplicant
git checkout hostap_2_11

# 3. Verify version
grep "VERSION=" wpa_supplicant/Makefile
# Should show: VERSION=2.11

# 4. Commit
cd ..
git add .gitmodules wpa_supplicant
git commit -m "feat(deps): add wpa_supplicant v2.11 submodule for WiFi control"
```

**wpa_supplicant info:**
- **Official repo:** git://w1.fi/hostap.git
- **Latest version:** v2.11 (2024-07-20)
- **Tag:** `hostap_2_11`
- **License:** BSD (compatible with GPL v3)
- **Features:** WPA3, WiFi 7 (EHT), MACsec, DPP

---

### Stage 2: Update Build System

**Goal:** Build libwpa_client.a and link into prototype

**Makefile changes** (~30 lines):

```make
# wpa_supplicant (WiFi control via wpa_ctrl interface)
WPA_DIR := wpa_supplicant
WPA_CLIENT_LIB := $(WPA_DIR)/wpa_supplicant/libwpa_client.a
WPA_INC := -I$(WPA_DIR)/src/common -I$(WPA_DIR)/src/utils

# Add to INCLUDES (line ~89)
INCLUDES := -I. -I$(INC_DIR) $(LVGL_INC) $(LIBHV_INC) $(SPDLOG_INC) $(WPA_INC) $(SDL2_CFLAGS)

# Platform-specific LDFLAGS
ifeq ($(UNAME_S),Darwin)
    # macOS: No wpa_supplicant (mock mode)
    LDFLAGS := $(SDL2_LIBS) $(LIBHV_LIB) -lm -lpthread -framework CoreFoundation -framework Security
else
    # Linux: Link libwpa_client.a
    LDFLAGS := $(SDL2_LIBS) $(LIBHV_LIB) $(WPA_CLIENT_LIB) -lm -lpthread
endif

# Build target (Linux only)
ifneq ($(UNAME_S),Darwin)
$(WPA_CLIENT_LIB):
	@echo "$(BOLD)$(BLUE)[WPA]$(RESET) Building wpa_supplicant client library..."
	$(Q)$(MAKE) -C $(WPA_DIR)/wpa_supplicant -j$(NPROC) libwpa_client.a

# Add to main build dependencies
$(BIN_DIR)/helix-ui-proto: ... $(WPA_CLIENT_LIB) ...
endif

# Clean target
clean:
	...
	ifneq ($(UNAME_S),Darwin)
		$(MAKE) -C $(WPA_DIR)/wpa_supplicant clean
	endif
```

**wpa_supplicant build config:**

Create `wpa_supplicant/wpa_supplicant/.config`:
```make
# Minimal config for libwpa_client.a only
CONFIG_CTRL_IFACE=y
CONFIG_CTRL_IFACE_UNIX=y
```

Or copy from parent:
```bash
cp ../wpa_supplicant/wpa_supplicant/.config wpa_supplicant/wpa_supplicant/
```

**Test:**
```bash
make clean
make -j8
# macOS: Should skip wpa_supplicant
# Linux: Should build libwpa_client.a first, then link
```

---

### Stage 3: Port WpaEvent Backend with libhv

**Reference:** `../src/wpa_event.{h,cpp}` in parent project

**Create `include/wifi_backend_wpa.h`:**

```cpp
/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 * Based on GuppyScreen WpaEvent implementation.
 * [GPL v3 header...]
 */

#pragma once

#include <string>
#include <functional>

// Platform-specific includes
#ifndef __APPLE__
#include "hv/EventLoop.h"
#include "hv/EventLoopThread.h"

struct wpa_ctrl;  // Forward declaration (from wpa_ctrl.h)

/**
 * @brief wpa_supplicant backend using libhv async event loop
 *
 * Provides async communication with wpa_supplicant daemon via Unix socket.
 * Uses libhv EventLoopThread for non-blocking I/O and event notifications.
 *
 * Architecture:
 * - Control connection: Send commands (SCAN, CONNECT, etc.)
 * - Monitor connection: Receive async events (SCAN-RESULTS, CONNECTED, etc.)
 * - libhv event loop: Handle socket I/O without blocking UI
 *
 * Ported from GuppyScreen's WpaEvent class (../src/wpa_event.cpp)
 */
class WifiBackendWpa : private hv::EventLoopThread {
public:
    WifiBackendWpa();
    ~WifiBackendWpa();

    /**
     * @brief Initialize and connect to wpa_supplicant socket
     *
     * @param socket_path Base path to wpa_supplicant sockets
     *                    (e.g., "/run/wpa_supplicant" or "/var/run/wpa_supplicant")
     *                    Will auto-detect actual interface socket (wlan0, etc.)
     * @return true if connected successfully
     */
    bool init(const std::string& socket_path);

    /**
     * @brief Send synchronous command to wpa_supplicant
     *
     * @param cmd Command string (e.g., "SCAN", "SCAN_RESULTS", "ADD_NETWORK")
     * @return Response string from wpa_supplicant
     */
    std::string send_command(const std::string& cmd);

    /**
     * @brief Register callback for async events
     *
     * Callback invoked when wpa_supplicant sends notifications:
     * - "<3>CTRL-EVENT-SCAN-RESULTS" - Scan complete
     * - "<3>CTRL-EVENT-CONNECTED" - Network connected
     * - "<3>CTRL-EVENT-DISCONNECTED" - Network disconnected
     *
     * @param cb Callback function receiving event string
     */
    void register_event_callback(std::function<void(const std::string&)> cb);

    /**
     * @brief Start event monitoring loop
     */
    void start();

    /**
     * @brief Stop event monitoring loop
     */
    void stop();

private:
    struct wpa_ctrl* ctrl_conn_;      // Command connection
    struct wpa_ctrl* monitor_conn_;   // Event monitoring connection (may be nullptr on macOS)
    std::function<void(const std::string&)> event_callback_;

    // libhv I/O callback (called when socket has data)
    static void handle_wpa_event_static(hio_t* io, void* data, int readbytes);
    void handle_wpa_event(void* data, int readbytes);

    // Socket path discovery
    std::string find_wpa_socket(const std::string& base_path);

    // Initialization (runs in event loop thread)
    void init_wpa_internal();
};

#else  // __APPLE__ - Mock stubs for macOS

class WifiBackendWpa {
public:
    WifiBackendWpa() {}
    ~WifiBackendWpa() {}
    bool init(const std::string&) { return false; }
    std::string send_command(const std::string&) { return ""; }
    void register_event_callback(std::function<void(const std::string&)>) {}
    void start() {}
    void stop() {}
};

#endif  // __APPLE__
```

**Create `src/wifi_backend_wpa.cpp`:**

```cpp
/*
 * Copyright (C) 2025 356C LLC
 * [GPL v3 header...]
 */

#include "wifi_backend_wpa.h"
#include <spdlog/spdlog.h>

#ifndef __APPLE__  // Linux only

#include "wpa_ctrl.h"
#include "hv/hloop.h"
#include <filesystem>
namespace fs = std::filesystem;

WifiBackendWpa::WifiBackendWpa()
    : hv::EventLoopThread(nullptr)
    , ctrl_conn_(nullptr)
    , monitor_conn_(nullptr) {
}

WifiBackendWpa::~WifiBackendWpa() {
    stop();
    if (ctrl_conn_) {
        wpa_ctrl_close(ctrl_conn_);
        ctrl_conn_ = nullptr;
    }
    if (monitor_conn_) {
        wpa_ctrl_close(monitor_conn_);
        monitor_conn_ = nullptr;
    }
}

bool WifiBackendWpa::init(const std::string& base_path) {
    std::string socket_path = find_wpa_socket(base_path);
    if (socket_path.empty()) {
        spdlog::error("[WiFi] No wpa_supplicant socket found in {}", base_path);
        return false;
    }

    // Open control connection
    ctrl_conn_ = wpa_ctrl_open(socket_path.c_str());
    if (!ctrl_conn_) {
        spdlog::error("[WiFi] Failed to open wpa_supplicant control: {}", socket_path);
        return false;
    }

    spdlog::info("[WiFi] Connected to wpa_supplicant: {}", socket_path);
    return true;
}

void WifiBackendWpa::start() {
    if (isRunning()) {
        loop()->runInLoop([this]() { init_wpa_internal(); });
    } else {
        hv::EventLoopThread::start(true, [this]() {
            init_wpa_internal();
            return 0;
        });
    }
}

void WifiBackendWpa::stop() {
    hv::EventLoopThread::stop(true);
}

void WifiBackendWpa::init_wpa_internal() {
    // This runs in the event loop thread

    // Open monitor connection (for async events)
    std::string socket_path = find_wpa_socket("/run/wpa_supplicant");
    if (socket_path.empty()) {
        socket_path = find_wpa_socket("/var/run/wpa_supplicant");
    }

    if (socket_path.empty()) {
        spdlog::error("[WiFi] Cannot find wpa_supplicant socket for monitoring");
        return;
    }

    monitor_conn_ = wpa_ctrl_open(socket_path.c_str());
    if (!monitor_conn_) {
        spdlog::error("[WiFi] Failed to open monitor connection");
        return;
    }

    // Attach to receive events
    if (wpa_ctrl_attach(monitor_conn_) != 0) {
        spdlog::error("[WiFi] Failed to attach to wpa_supplicant");
        wpa_ctrl_close(monitor_conn_);
        monitor_conn_ = nullptr;
        return;
    }

    // Get socket file descriptor
    int fd = wpa_ctrl_get_fd(monitor_conn_);

    // Register with libhv event loop
    hio_t* io = hio_get(loop()->loop(), fd);
    if (!io) {
        spdlog::error("[WiFi] Failed to register socket with libhv");
        return;
    }

    hio_set_context(io, this);
    hio_setcb_read(io, WifiBackendWpa::handle_wpa_event_static);
    hio_read_start(io);

    spdlog::info("[WiFi] Event monitoring started");
}

void WifiBackendWpa::handle_wpa_event_static(hio_t* io, void* data, int readbytes) {
    WifiBackendWpa* self = (WifiBackendWpa*)hio_context(io);
    self->handle_wpa_event(data, readbytes);
}

void WifiBackendWpa::handle_wpa_event(void* data, int readbytes) {
    std::string event((char*)data, readbytes);
    spdlog::debug("[WiFi] Event: {}", event);

    if (event_callback_) {
        event_callback_(event);
    }
}

std::string WifiBackendWpa::send_command(const std::string& cmd) {
    if (!ctrl_conn_) {
        spdlog::warn("[WiFi] send_command called but not connected");
        return "";
    }

    char reply[4096];
    size_t reply_len = sizeof(reply) - 1;

    spdlog::debug("[WiFi] Command: {}", cmd);

    if (wpa_ctrl_request(ctrl_conn_, cmd.c_str(), cmd.length(),
                         reply, &reply_len, nullptr) == 0) {
        reply[reply_len] = '\0';
        return std::string(reply, reply_len);
    }

    spdlog::error("[WiFi] Command failed: {}", cmd);
    return "";
}

void WifiBackendWpa::register_event_callback(std::function<void(const std::string&)> cb) {
    event_callback_ = cb;
}

std::string WifiBackendWpa::find_wpa_socket(const std::string& base_path) {
    // Check if base_path is already a socket
    if (fs::exists(base_path) && fs::is_socket(base_path)) {
        return base_path;
    }

    // Search directory for interface sockets (wlan0, wlan1, etc.)
    if (fs::is_directory(base_path)) {
        for (const auto& entry : fs::directory_iterator(base_path)) {
            if (fs::is_socket(entry.path())) {
                // Skip p2p sockets
                std::string name = entry.path().filename().string();
                if (name.find("p2p") == std::string::npos) {
                    spdlog::debug("[WiFi] Found socket: {}", entry.path().string());
                    return entry.path().string();
                }
            }
        }
    }

    return "";
}

#endif  // __APPLE__
```

**Key porting notes:**
1. Same architecture as parent's `wpa_event.cpp`
2. Uses `hv::EventLoopThread` for async I/O
3. Socket discovery logic from parent (lines 52-98)
4. Event handling from parent (lines 100-106)
5. Platform guards for macOS compilation

---

### Stage 4: Implement WiFiManager Linux Backend

**Goal:** Wire WifiBackendWpa into existing WiFiManager API

**Update `src/wifi_manager.cpp`:**

Add at top:
```cpp
#ifndef __APPLE__
#include "wifi_backend_wpa.h"
static WifiBackendWpa wpa_backend;
static bool wpa_initialized = false;
#endif
```

**Implement `set_enabled()`:**

```cpp
bool set_enabled(bool enabled) {
#if WIFI_MOCK_MODE
    wifi_enabled = enabled;
    spdlog::info("[WiFi] Mock: WiFi {}", enabled ? "enabled" : "disabled");
    return true;
#else
    if (!wpa_initialized) {
        wpa_initialized = wpa_backend.init("/run/wpa_supplicant");
        if (!wpa_initialized) {
            wpa_initialized = wpa_backend.init("/var/run/wpa_supplicant");
        }

        if (!wpa_initialized) {
            spdlog::error("[WiFi] Cannot initialize wpa_supplicant backend");
            return false;
        }

        // Register event callback
        wpa_backend.register_event_callback([](const std::string& event) {
            handle_wpa_event(event);
        });
    }

    if (enabled) {
        wpa_backend.start();
        wifi_enabled = true;
    } else {
        wpa_backend.stop();
        wifi_enabled = false;
    }

    return true;
#endif
}
```

**Implement `perform_scan()`:**

```cpp
static std::vector<WiFiNetwork> perform_scan() {
#if WIFI_MOCK_MODE
    // [existing mock code...]
#else
    if (!wpa_initialized) {
        spdlog::warn("[WiFi] Scan called but wpa not initialized");
        return std::vector<WiFiNetwork>();
    }

    // Trigger scan
    std::string result = wpa_backend.send_command("SCAN");
    if (result != "OK\n") {
        spdlog::warn("[WiFi] SCAN command failed: {}", result);
        return std::vector<WiFiNetwork>();
    }

    // Wait for CTRL-EVENT-SCAN-RESULTS (or poll SCAN_RESULTS after delay)
    // For now, wait 2 seconds (TODO: use async event)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Get results
    std::string results = wpa_backend.send_command("SCAN_RESULTS");
    return parse_scan_results(results);
#endif
}

// Helper function
static std::vector<WiFiNetwork> parse_scan_results(const std::string& results) {
    std::vector<WiFiNetwork> networks;
    std::istringstream stream(results);
    std::string line;

    while (std::getline(stream, line)) {
        // Skip header: "bssid / frequency / signal level / flags / ssid"
        if (line.find("bssid") == 0) continue;

        // Parse: "00:11:22:33:44:55\t2437\t-45\t[WPA2-PSK-CCMP][ESS]\tMyNetwork"
        auto parts = split(line, '\t');
        if (parts.size() < 5) continue;

        WiFiNetwork net;
        net.ssid = parts[4];

        // Convert signal (dBm) to percentage
        // -30 dBm = 100%, -90 dBm = 0%
        int signal_dbm = std::stoi(parts[2]);
        net.signal_strength = std::max(0, std::min(100,
            (int)((signal_dbm + 90) * 100.0 / 60.0)));

        // Check security
        std::string flags = parts[3];
        net.is_secured = (flags.find("WPA") != std::string::npos ||
                          flags.find("WEP") != std::string::npos);

        if (flags.find("WPA3") != std::string::npos) {
            net.security_type = "WPA3";
        } else if (flags.find("WPA2") != std::string::npos) {
            net.security_type = "WPA2";
        } else if (flags.find("WPA") != std::string::npos) {
            net.security_type = "WPA";
        } else if (flags.find("WEP") != std::string::npos) {
            net.security_type = "WEP";
        } else {
            net.security_type = "Open";
        }

        networks.push_back(net);
    }

    spdlog::debug("[WiFi] Parsed {} networks from scan results", networks.size());
    return networks;
}

// String split helper
static std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}
```

**Implement `connect()`:**

```cpp
void connect(const std::string& ssid,
            const std::string& password,
            std::function<void(bool success, const std::string& error)> on_complete) {
#if WIFI_MOCK_MODE
    // [existing mock code...]
#else
    if (!wpa_initialized) {
        if (on_complete) on_complete(false, "wpa_supplicant not initialized");
        return;
    }

    spdlog::info("[WiFi] Connecting to \"{}\"", ssid);

    // Add network
    std::string result = wpa_backend.send_command("ADD_NETWORK");
    if (result.empty() || result == "FAIL\n") {
        if (on_complete) on_complete(false, "Failed to add network");
        return;
    }

    int net_id = std::stoi(result);

    // Set SSID
    result = wpa_backend.send_command(
        fmt::format("SET_NETWORK {} ssid \"{}\"", net_id, ssid));
    if (result != "OK\n") {
        if (on_complete) on_complete(false, "Failed to set SSID");
        return;
    }

    // Set password (if secured)
    if (!password.empty()) {
        result = wpa_backend.send_command(
            fmt::format("SET_NETWORK {} psk \"{}\"", net_id, password));
        if (result != "OK\n") {
            if (on_complete) on_complete(false, "Failed to set password");
            return;
        }
    } else {
        // Open network
        result = wpa_backend.send_command(
            fmt::format("SET_NETWORK {} key_mgmt NONE", net_id));
        if (result != "OK\n") {
            if (on_complete) on_complete(false, "Failed to configure open network");
            return;
        }
    }

    // Select network (triggers connection)
    result = wpa_backend.send_command(fmt::format("SELECT_NETWORK {}", net_id));
    if (result != "OK\n") {
        if (on_complete) on_complete(false, "Failed to select network");
        return;
    }

    // Save configuration
    wpa_backend.send_command("SAVE_CONFIG");

    // Store callback for async completion
    // (Will be called from handle_wpa_event when CTRL-EVENT-CONNECTED received)
    connect_callback = on_complete;
    connecting_ssid = ssid;
#endif
}
```

**Implement event handler:**

```cpp
static std::function<void(bool, const std::string&)> connect_callback;
static std::string connecting_ssid;

static void handle_wpa_event(const std::string& event) {
    spdlog::debug("[WiFi] Event: {}", event);

    if (event.find("CTRL-EVENT-SCAN-RESULTS") != std::string::npos) {
        // Scan complete - trigger callback if registered
        if (scan_callback) {
            std::vector<WiFiNetwork> networks = perform_scan();
            scan_callback(networks);
        }
    }
    else if (event.find("CTRL-EVENT-CONNECTED") != std::string::npos) {
        // Connection successful
        wifi_connected = true;
        connected_ssid = connecting_ssid;

        // Get IP address (might take a moment)
        std::this_thread::sleep_for(std::chrono::seconds(1));
        connected_ip = get_interface_ip("wlan0");  // TODO: detect interface

        spdlog::info("[WiFi] Connected to \"{}\", IP: {}",
                     connected_ssid, connected_ip);

        if (connect_callback) {
            connect_callback(true, "");
            connect_callback = nullptr;
        }
    }
    else if (event.find("CTRL-EVENT-DISCONNECTED") != std::string::npos) {
        // Connection lost
        bool was_connecting = !connecting_ssid.empty();

        wifi_connected = false;
        connected_ssid.clear();
        connected_ip.clear();

        spdlog::warn("[WiFi] Disconnected");

        if (was_connecting && connect_callback) {
            connect_callback(false, "Connection failed");
            connect_callback = nullptr;
        }

        connecting_ssid.clear();
    }
}

// Helper: Get IP address of interface
static std::string get_interface_ip(const std::string& interface) {
    // TODO: Use getifaddrs() or parse ip addr show
    // For now, stub
    return "192.168.1.100";
}
```

**Implementation notes:**
- Parse scan results format: `bssid / freq / signal / flags / ssid`
- Signal conversion: -30 dBm (excellent) to -90 dBm (weak)
- Security detection from flags: `[WPA2-PSK-CCMP][ESS]`
- Async connection: command succeeds immediately, event arrives later
- Thread-safe: libhv event loop runs in separate thread, callbacks queued

---

### Stage 5: Testing Strategy

**Test Matrix:**

| Platform | Condition | Test | Expected |
|----------|-----------|------|----------|
| macOS | Always | Build | ✅ Compiles (stubs) |
| macOS | Always | Run | ✅ Mock networks show |
| Linux | No daemon | Build | ✅ Links libwpa_client.a |
| Linux | No daemon | Run | ⚠️ Falls back to mock |
| Linux | With daemon | Scan | ✅ Real networks |
| Linux | With daemon | Connect (open) | ✅ Joins network |
| Linux | With daemon | Connect (WPA2) | ✅ Authenticates |
| Linux | With daemon | Disconnect | ✅ Leaves network |
| Linux | With daemon | Async events | ✅ UI updates |

**Linux wpa_supplicant setup:**

```bash
# 1. Install wpa_supplicant (if not present)
sudo apt install wpasupplicant  # Debian/Ubuntu

# 2. Create minimal config
sudo nano /etc/wpa_supplicant/wpa_supplicant.conf
```

Contents:
```
ctrl_interface=/run/wpa_supplicant
update_config=1
```

```bash
# 3. Start daemon
sudo systemctl start wpa_supplicant

# 4. Verify socket exists
ls -la /run/wpa_supplicant/
# Should see wlan0 or similar

# 5. Test manually
wpa_cli -i wlan0 scan
wpa_cli -i wlan0 scan_results
```

**Debugging:**

```bash
# Check daemon status
systemctl status wpa_supplicant

# Monitor events live
wpa_cli -i wlan0
> scan
> scan_results

# Check socket permissions
ls -la /run/wpa_supplicant/wlan0

# Enable debug logging in code
spdlog::set_level(spdlog::level::debug);
```

---

### Stage 6: Documentation

**Update CLAUDE.md:**

Add to "Build Requirements" section:
```markdown
### WiFi Support (Linux only)

**wpa_supplicant** - WiFi network management
- Daemon must be running: `systemctl status wpa_supplicant`
- Socket path: `/run/wpa_supplicant/wlan0` (auto-detected)
- Configuration: `/etc/wpa_supplicant/wpa_supplicant.conf`

**Note:** macOS builds use mock WiFi (no hardware access in simulator).
```

Add to "Architecture" section:
```markdown
### WiFi Architecture

**Linux (production):**
```
WiFiManager (public API)
    ↓
WifiBackendWpa (wpa_ctrl interface)
    ↓
libhv EventLoopThread (async I/O)
    ↓
wpa_supplicant daemon (Unix socket)
    ↓
Linux kernel (nl80211 / cfg80211)
```

**macOS (simulator):**
```
WiFiManager (public API)
    ↓
Mock implementation (static test data)
```

**Event flow:**
1. UI calls `WiFiManager::scan()`
2. Backend sends `SCAN` command to wpa_supplicant
3. wpa_supplicant scans WiFi channels
4. Event `CTRL-EVENT-SCAN-RESULTS` arrives on monitor socket
5. libhv detects readable socket, invokes callback
6. Callback parses `SCAN_RESULTS` and updates UI
```

**Create docs/WIFI_SETUP.md:**

```markdown
# WiFi Setup Guide

## Overview

HelixScreen uses wpa_supplicant for WiFi management on embedded Linux targets.

## Prerequisites

- Linux kernel with nl80211/cfg80211 support
- wpa_supplicant v2.0 or later
- WiFi hardware (wlan0, wlan1, etc.)

## Installation

### Debian/Ubuntu/Raspberry Pi OS
\`\`\`bash
sudo apt update
sudo apt install wpasupplicant
\`\`\`

### Buildroot (K1, custom embedded)
\`\`\`
BR2_PACKAGE_WPA_SUPPLICANT=y
BR2_PACKAGE_WPA_SUPPLICANT_CLI=y
BR2_PACKAGE_WPA_SUPPLICANT_WPA_CLIENT_SO=y
\`\`\`

## Configuration

Create `/etc/wpa_supplicant/wpa_supplicant.conf`:

\`\`\`
# Allow HelixScreen to control via Unix socket
ctrl_interface=/run/wpa_supplicant

# Allow dynamic config updates
update_config=1

# Optional: Pre-configure known networks
network={
    ssid="MyHomeWiFi"
    psk="password123"
}
\`\`\`

**Security note:** Protect this file:
\`\`\`bash
sudo chmod 600 /etc/wpa_supplicant/wpa_supplicant.conf
sudo chown root:root /etc/wpa_supplicant/wpa_supplicant.conf
\`\`\`

## Starting the Daemon

### systemd (Debian/Ubuntu/Pi OS)
\`\`\`bash
# Start now
sudo systemctl start wpa_supplicant

# Enable at boot
sudo systemctl enable wpa_supplicant

# Check status
systemctl status wpa_supplicant
\`\`\`

### Manual start
\`\`\`bash
sudo wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
\`\`\`

### OpenRC (Buildroot, Gentoo)
\`\`\`bash
rc-update add wpa_supplicant default
rc-service wpa_supplicant start
\`\`\`

## Verifying Setup

Check socket exists:
\`\`\`bash
ls -la /run/wpa_supplicant/
# Should show: wlan0
\`\`\`

Test with wpa_cli:
\`\`\`bash
wpa_cli -i wlan0 status
wpa_cli -i wlan0 scan
wpa_cli -i wlan0 scan_results
\`\`\`

## Troubleshooting

### Socket not found
\`\`\`bash
# Check daemon is running
ps aux | grep wpa_supplicant

# Check interface name
ip link show | grep wlan

# Manually specify interface
sudo wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
\`\`\`

### Permission denied
\`\`\`bash
# Add user to netdev group
sudo usermod -aG netdev $USER

# Or run HelixScreen as root (not recommended for production)
sudo ./build/bin/helix-ui-proto
\`\`\`

### Interface not ready
\`\`\`bash
# Bring interface up
sudo ip link set wlan0 up

# Check for firmware issues
dmesg | grep -i firmware
dmesg | grep -i wlan
\`\`\`

## Platform-Specific Notes

### Raspberry Pi
- Default interface: `wlan0`
- Built-in WiFi works out of box
- Consider external USB WiFi for better range

### Creality K1
- Check Creality's wpa_supplicant config
- May need custom kernel modules
- Socket path might differ

### FlashForge AD5M
- Pre-configured by manufacturer
- Usually `/var/run/wpa_supplicant`

## Security Considerations

1. **Socket permissions:** Ensure only authorized users can access control socket
2. **Config file:** Protect `wpa_supplicant.conf` (contains PSKs)
3. **Open networks:** Warn users about security risks
4. **WPA3:** Use latest wpa_supplicant (v2.10+) for WPA3 support

## See Also

- [wpa_supplicant documentation](https://w1.fi/wpa_supplicant/)
- [wpa_ctrl API reference](https://w1.fi/wpa_supplicant/devel/ctrl_iface_page.html)
- [Arch Linux wiki](https://wiki.archlinux.org/title/Wpa_supplicant)
```

---

## Important Implementation Details

### Thread Safety

**libhv event loop runs in separate thread:**
- `wpa_backend.send_command()` - Thread-safe (libhv handles mutex)
- Event callbacks - Run in libhv thread, not LVGL thread
- **CRITICAL:** Do NOT call LVGL functions from event callback directly

**Solution:** Queue events for LVGL thread (like Moonraker notifications):

```cpp
// In event callback (libhv thread)
static void handle_wpa_event(const std::string& event) {
    std::lock_guard<std::mutex> lock(wifi_event_mutex);
    wifi_event_queue.push(event);
}

// In LVGL timer callback (LVGL thread)
static void process_wifi_events(lv_timer_t* timer) {
    std::lock_guard<std::mutex> lock(wifi_event_mutex);
    while (!wifi_event_queue.empty()) {
        std::string event = wifi_event_queue.front();
        wifi_event_queue.pop();

        // Now safe to update LVGL widgets
        handle_wifi_event_ui_update(event);
    }
}
```

### Memory Management

- `wpa_ctrl` connections must be closed in destructor
- libhv event loop must be stopped before closing connections
- Monitor connection (`monitor_conn_`) separate from control (`ctrl_conn_`)

### Error Handling

- Socket not found: Fall back to mock mode, log warning
- Command timeout: wpa_ctrl has built-in timeout (default 10s)
- Connection failure: Callback with error message
- Daemon crash: Detect via socket error, attempt reconnect

---

## Timeline Estimate

| Stage | Description | Time |
|-------|-------------|------|
| 0 | libhv submodule | 15 min |
| 1 | wpa_supplicant submodule | 10 min |
| 2 | Build system | 30 min |
| 3 | Backend port | 90 min |
| 4 | WiFiManager impl | 60 min |
| 5 | Testing | 45 min |
| 6 | Documentation | 20 min |
| **Total** | | **~4.5 hours** |

---

## Success Criteria

### Build
- ✅ macOS builds without errors (mock mode)
- ✅ Linux builds libwpa_client.a and links
- ✅ No warnings about missing symbols

### Functionality
- ✅ Mock mode works on macOS (existing behavior)
- ✅ Linux detects wpa_supplicant socket
- ✅ Scan returns real networks (Linux)
- ✅ Connect joins network (Linux)
- ✅ Password authentication works (WPA2)
- ✅ Async events update UI in real-time
- ✅ Disconnect clears connection state

### Code Quality
- ✅ Matches parent GuppyScreen patterns
- ✅ GPL v3 headers on all new files
- ✅ spdlog used for all logging
- ✅ Thread-safe event handling
- ✅ No memory leaks (valgrind)

### Documentation
- ✅ CLAUDE.md updated
- ✅ WIFI_SETUP.md created
- ✅ README mentions new submodules
- ✅ Code comments explain architecture

---

## Follow-Up Work (Future)

After wpa_supplicant integration is stable:

1. **Improve scan UX:**
   - Progressive scan results (show networks as they appear)
   - Signal strength updates in real-time
   - Sort by signal strength

2. **Connection management:**
   - Remember networks (from wpa_supplicant config)
   - Auto-connect to known networks
   - Connection timeout handling

3. **Advanced features:**
   - Hidden network support (manual SSID entry)
   - Static IP configuration
   - Network priority management
   - Forget network functionality

4. **Error recovery:**
   - Automatic reconnect on disconnect
   - Fallback to previous network
   - Connection failure diagnostics

5. **libhv expansion:**
   - REST API server (control UI from browser)
   - WebSocket API for debugging
   - Multi-printer support

---

## References

- **Parent implementation:** `../src/wpa_event.{h,cpp}`, `../src/wifi_panel.cpp`
- **wpa_supplicant docs:** https://w1.fi/wpa_supplicant/
- **wpa_ctrl API:** https://w1.fi/wpa_supplicant/devel/ctrl_iface_page.html
- **libhv docs:** https://github.com/ithewei/libhv
- **This project:** `docs/ROADMAP.md`, `CLAUDE.md`

---

**Document version:** 1.0
**Last updated:** 2025-10-28
**Status:** READY FOR IMPLEMENTATION
