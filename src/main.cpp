// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file main.cpp
 * @brief Application entry point
 *
 * This file is intentionally minimal. All application logic is implemented
 * in the Application class (src/application/application.cpp).
 *
 * @see Application
 */

#include "application.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

/**
 * @brief SIGSEGV handler that dumps backtrace before crashing
 *
 * Captures and logs a stack trace so the watchdog log shows where the crash
 * occurred, then re-raises the signal for normal crash behavior.
 */
static void crash_signal_handler(int sig) {
    // Use async-signal-safe write for the header
    const char* msg = "\n=== CRASH: Signal ";
    write(STDERR_FILENO, msg, 18);

    // Print signal number
    char sig_str[4];
    int len = snprintf(sig_str, sizeof(sig_str), "%d", sig);
    write(STDERR_FILENO, sig_str, static_cast<size_t>(len));

    const char* msg2 = " ===\nBacktrace:\n";
    write(STDERR_FILENO, msg2, 15);

    // Capture backtrace (backtrace() is async-signal-safe on Linux)
    void* callstack[32];
    int frames = backtrace(callstack, 32);
    backtrace_symbols_fd(callstack, frames, STDERR_FILENO);

    // Try to flush spdlog (not strictly async-signal-safe but usually works)
    try {
        spdlog::critical("=== CRASH: Segmentation fault (signal {}) ===", sig);
        spdlog::dump_backtrace();
        spdlog::default_logger()->flush();
    } catch (...) {
        // Ignore - we're crashing anyway
    }

    // Re-raise signal for default handler (generates core dump if enabled)
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char** argv) {
    // Install crash signal handlers BEFORE anything else
    signal(SIGSEGV, crash_signal_handler);
    signal(SIGABRT, crash_signal_handler);
    signal(SIGBUS, crash_signal_handler);

    Application app;
    return app.run(argc, argv);
}
