// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 HelixScreen Contributors

#include "memory_monitor.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>

#ifdef __linux__
#include <fstream>
#include <sstream>
#include <string>
#endif

namespace helix {

MemoryMonitor& MemoryMonitor::instance() {
    static MemoryMonitor instance;
    return instance;
}

MemoryMonitor::~MemoryMonitor() {
    stop();
}

void MemoryMonitor::start(int interval_ms) {
    if (running_.load()) {
        return; // Already running
    }

    interval_ms_.store(interval_ms);
    running_.store(true);

    monitor_thread_ = std::thread([this]() { monitor_loop(); });

    spdlog::info("[MemoryMonitor] Started (interval={}ms)", interval_ms);
}

void MemoryMonitor::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    spdlog::debug("[MemoryMonitor] Stopped");
}

MemoryStats MemoryMonitor::get_current_stats() {
    MemoryStats stats;

#ifdef __linux__
    std::ifstream status("/proc/self/status");
    if (!status.is_open()) {
        return stats;
    }

    std::string line;
    while (std::getline(status, line)) {
        // Parse lines like "VmRSS:      6520 kB"
        if (line.compare(0, 7, "VmSize:") == 0) {
            sscanf(line.c_str(), "VmSize: %zu", &stats.vm_size_kb);
        } else if (line.compare(0, 6, "VmRSS:") == 0) {
            sscanf(line.c_str(), "VmRSS: %zu", &stats.vm_rss_kb);
        } else if (line.compare(0, 7, "VmData:") == 0) {
            sscanf(line.c_str(), "VmData: %zu", &stats.vm_data_kb);
        } else if (line.compare(0, 7, "VmSwap:") == 0) {
            sscanf(line.c_str(), "VmSwap: %zu", &stats.vm_swap_kb);
        } else if (line.compare(0, 7, "VmPeak:") == 0) {
            sscanf(line.c_str(), "VmPeak: %zu", &stats.vm_peak_kb);
        } else if (line.compare(0, 6, "VmHWM:") == 0) {
            sscanf(line.c_str(), "VmHWM: %zu", &stats.vm_hwm_kb);
        }
    }
#endif

    return stats;
}

void MemoryMonitor::log_now(const char* context) {
    MemoryStats stats = get_current_stats();

    if (context) {
        // Context-based logs go to DEBUG so they're visible with -vv
        spdlog::debug(
            "[MemoryMonitor] [{}] RSS={}kB VmSize={}kB VmData={}kB Swap={}kB (Peak: RSS={}kB "
            "Vm={}kB)",
            context, stats.vm_rss_kb, stats.vm_size_kb, stats.vm_data_kb, stats.vm_swap_kb,
            stats.vm_hwm_kb, stats.vm_peak_kb);
    } else {
        spdlog::trace("[MemoryMonitor] RSS={}kB VmSize={}kB VmData={}kB Swap={}kB (Peak: RSS={}kB "
                      "Vm={}kB)",
                      stats.vm_rss_kb, stats.vm_size_kb, stats.vm_data_kb, stats.vm_swap_kb,
                      stats.vm_hwm_kb, stats.vm_peak_kb);
    }
}

void MemoryMonitor::monitor_loop() {
    // Log initial state
    log_now("start");

    MemoryStats prev_stats = get_current_stats();

    while (running_.load()) {
        // Sleep in small chunks so we can respond to stop() quickly
        int remaining_ms = interval_ms_.load();
        while (remaining_ms > 0 && running_.load()) {
            int sleep_ms = std::min(remaining_ms, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            remaining_ms -= sleep_ms;
        }

        if (!running_.load()) {
            break;
        }

        MemoryStats stats = get_current_stats();

        // Calculate deltas
        int64_t rss_delta =
            static_cast<int64_t>(stats.vm_rss_kb) - static_cast<int64_t>(prev_stats.vm_rss_kb);
        int64_t vm_delta =
            static_cast<int64_t>(stats.vm_size_kb) - static_cast<int64_t>(prev_stats.vm_size_kb);

        // Log with delta if significant change (>100kB)
        if (std::abs(rss_delta) > 100 || std::abs(vm_delta) > 100) {
            spdlog::trace(
                "[MemoryMonitor] RSS={}kB ({:+}kB) VmSize={}kB ({:+}kB) VmData={}kB Swap={}kB",
                stats.vm_rss_kb, rss_delta, stats.vm_size_kb, vm_delta, stats.vm_data_kb,
                stats.vm_swap_kb);
        } else {
            spdlog::trace("[MemoryMonitor] RSS={}kB VmSize={}kB VmData={}kB Swap={}kB",
                          stats.vm_rss_kb, stats.vm_size_kb, stats.vm_data_kb, stats.vm_swap_kb);
        }

        prev_stats = stats;
    }

    // Log final state
    log_now("stop");
}

} // namespace helix
