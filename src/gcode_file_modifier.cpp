// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gcode_file_modifier.h"

#include "moonraker_api.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>

namespace gcode {

// ============================================================================
// TempGCodeFile Implementation
// ============================================================================

TempGCodeFile::TempGCodeFile(std::string moonraker_path, std::string original_filename,
                             CleanupCallback cleanup_callback)
    : moonraker_path_(std::move(moonraker_path)),
      original_filename_(std::move(original_filename)),
      cleanup_callback_(std::move(cleanup_callback)),
      owns_file_(true) {
    spdlog::debug("[TempGCodeFile] Created handle for {} (original: {})", moonraker_path_,
                  original_filename_);
}

TempGCodeFile::~TempGCodeFile() {
    if (owns_file_ && cleanup_callback_) {
        spdlog::debug("[TempGCodeFile] Triggering cleanup for {}", moonraker_path_);
        cleanup_callback_(moonraker_path_);
    }
}

TempGCodeFile::TempGCodeFile(TempGCodeFile&& other) noexcept
    : moonraker_path_(std::move(other.moonraker_path_)),
      original_filename_(std::move(other.original_filename_)),
      cleanup_callback_(std::move(other.cleanup_callback_)),
      owns_file_(other.owns_file_) {
    other.owns_file_ = false;  // Transfer ownership
}

TempGCodeFile& TempGCodeFile::operator=(TempGCodeFile&& other) noexcept {
    if (this != &other) {
        // Cleanup current file if we own it
        if (owns_file_ && cleanup_callback_) {
            cleanup_callback_(moonraker_path_);
        }

        moonraker_path_ = std::move(other.moonraker_path_);
        original_filename_ = std::move(other.original_filename_);
        cleanup_callback_ = std::move(other.cleanup_callback_);
        owns_file_ = other.owns_file_;
        other.owns_file_ = false;
    }
    return *this;
}

void TempGCodeFile::release() {
    spdlog::debug("[TempGCodeFile] Releasing ownership of {}", moonraker_path_);
    owns_file_ = false;
}

// ============================================================================
// GCodeFileModifier Implementation
// ============================================================================

GCodeFileModifier::GCodeFileModifier(MoonrakerAPI& api, const ModifierConfig& config)
    : api_(api), config_(config) {
    spdlog::debug("[GCodeFileModifier] Created with temp_dir={}", config_.temp_dir);
}

void GCodeFileModifier::create_skip_copy(const std::string& original_path,
                                          const std::vector<DetectedOperation>& ops_to_skip,
                                          SuccessCallback on_success, ErrorCallback on_error) {
    if (ops_to_skip.empty()) {
        spdlog::warn("[GCodeFileModifier] create_skip_copy called with no ops to skip");
        if (on_error) {
            on_error("No operations specified to skip");
        }
        return;
    }

    spdlog::info("[GCodeFileModifier] Creating skip copy of {} with {} operations commented out",
                 original_path, ops_to_skip.size());

    for (const auto& op : ops_to_skip) {
        spdlog::debug("[GCodeFileModifier] Will skip: {} at line {}", op.display_name(),
                      op.line_number);
    }

    // First ensure the temp directory exists
    ensure_temp_directory(
        [this, original_path, ops_to_skip, on_success, on_error]() {
            // Step 1: Download the original file via HTTP GET
            spdlog::debug("[GCodeFileModifier] Downloading original file: {}", original_path);

            api_.download_file(
                "gcodes", original_path,
                // Download success - modify and upload
                [this, original_path, ops_to_skip, on_success,
                 on_error](const std::string& content) {
                    spdlog::debug("[GCodeFileModifier] Downloaded {} bytes, modifying content",
                                  content.size());

                    // Step 2: Modify the content
                    auto modification_result = generate_modified_content(content, ops_to_skip);
                    std::string modified_content = modification_result.first;
                    size_t lines_modified = modification_result.second;

                    spdlog::info("[GCodeFileModifier] Modified {} lines in file", lines_modified);

                    // Build the temp file path
                    // Extract just the filename from original_path
                    std::string filename = original_path;
                    size_t last_slash = original_path.rfind('/');
                    if (last_slash != std::string::npos) {
                        filename = original_path.substr(last_slash + 1);
                    }

                    std::string temp_moonraker_path = config_.temp_dir + "/" + filename;
                    std::string upload_path = "gcodes/" + temp_moonraker_path;

                    spdlog::debug("[GCodeFileModifier] Uploading modified file to: {}", upload_path);

                    // Step 3: Upload the modified content
                    api_.upload_file_with_name(
                        "gcodes", config_.temp_dir, filename, modified_content,
                        // Upload success
                        [this, temp_moonraker_path, filename, ops_to_skip, lines_modified,
                         on_success]() {
                            spdlog::info("[GCodeFileModifier] Successfully created skip copy: {}",
                                         temp_moonraker_path);

                            // Create RAII handle with cleanup callback
                            auto temp_file = std::make_unique<TempGCodeFile>(
                                temp_moonraker_path, filename,
                                [this](const std::string& path) { delete_temp_file(path); });

                            // Build result
                            SkipCopyResult result;
                            result.temp_file = std::move(temp_file);
                            result.lines_modified = lines_modified;

                            // Extract operation types from skipped operations
                            for (const auto& op : ops_to_skip) {
                                result.skipped_ops.push_back(op.type);
                            }

                            if (on_success) {
                                on_success(std::move(result));
                            }
                        },
                        // Upload error
                        [on_error](const MoonrakerError& err) {
                            spdlog::error("[GCodeFileModifier] Failed to upload modified file: {}",
                                          err.message);
                            if (on_error) {
                                on_error("Failed to upload modified file: " + err.message);
                            }
                        });
                },
                // Download error
                [on_error](const MoonrakerError& err) {
                    spdlog::error("[GCodeFileModifier] Failed to download original file: {}",
                                  err.message);
                    if (on_error) {
                        on_error("Failed to download original file: " + err.message);
                    }
                });
        },
        on_error);
}

void GCodeFileModifier::ensure_temp_directory(std::function<void()> on_success,
                                               ErrorCallback on_error) {
    std::string full_path = "gcodes/" + config_.temp_dir;

    spdlog::debug("[GCodeFileModifier] Ensuring temp directory exists: {}", full_path);

    api_.create_directory(
        full_path,
        [on_success, full_path]() {
            spdlog::debug("[GCodeFileModifier] Temp directory ready: {}", full_path);
            if (on_success) {
                on_success();
            }
        },
        [on_success, on_error, full_path](const MoonrakerError& err) {
            // Directory might already exist - that's OK
            if (err.message.find("exists") != std::string::npos ||
                err.message.find("already") != std::string::npos) {
                spdlog::debug("[GCodeFileModifier] Temp directory already exists: {}", full_path);
                if (on_success) {
                    on_success();
                }
            } else {
                spdlog::error("[GCodeFileModifier] Failed to create temp directory: {}",
                              err.message);
                if (on_error) {
                    on_error(err.message);
                }
            }
        });
}

void GCodeFileModifier::cleanup_all_temp_files(std::function<void(int deleted_count)> on_success,
                                                ErrorCallback on_error) {
    std::string temp_path = config_.temp_dir;

    spdlog::info("[GCodeFileModifier] Cleaning up all temp files in {}", temp_path);

    api_.list_files(
        "gcodes", temp_path, false,
        [this, on_success, temp_path](const std::vector<FileInfo>& files) {
            int count = 0;
            for (const auto& file : files) {
                if (!file.is_dir) {
                    std::string full_path = temp_path + "/" + file.filename;
                    spdlog::debug("[GCodeFileModifier] Deleting orphaned temp file: {}", full_path);
                    delete_temp_file(full_path);
                    count++;
                }
            }
            spdlog::info("[GCodeFileModifier] Cleaned up {} orphaned temp files", count);
            if (on_success) {
                on_success(count);
            }
        },
        [on_success, on_error](const MoonrakerError& err) {
            // Directory might not exist - that's OK, nothing to clean
            if (err.message.find("not found") != std::string::npos ||
                err.message.find("does not exist") != std::string::npos) {
                spdlog::debug("[GCodeFileModifier] Temp directory doesn't exist, nothing to clean");
                if (on_success) {
                    on_success(0);
                }
            } else {
                spdlog::error("[GCodeFileModifier] Failed to list temp files: {}", err.message);
                if (on_error) {
                    on_error(err.message);
                }
            }
        });
}

std::pair<std::string, size_t> GCodeFileModifier::generate_modified_content(
    const std::string& original_content,
    const std::vector<DetectedOperation>& ops_to_skip) const {

    // Build set of line numbers to skip for O(1) lookup
    std::set<size_t> lines_to_skip;
    for (const auto& op : ops_to_skip) {
        lines_to_skip.insert(op.line_number);
    }

    std::ostringstream modified;
    std::istringstream input(original_content);
    std::string line;
    size_t line_number = 0;
    size_t modified_count = 0;

    // Add header comment if configured
    if (config_.add_header_comment) {
        std::string original_filename = "unknown";
        if (!ops_to_skip.empty() && !ops_to_skip[0].raw_line.empty()) {
            // Try to extract filename from context - caller should provide this
        }
        modified << generate_header_comment(original_filename, ops_to_skip);
    }

    while (std::getline(input, line)) {
        line_number++;

        if (lines_to_skip.count(line_number) > 0) {
            // Comment out this line
            modified << config_.skip_prefix << line;
            if (!line.empty() && line.back() != '\n') {
                modified << " ; HelixScreen: operation disabled by user";
            }
            modified << "\n";
            modified_count++;

            spdlog::trace("[GCodeFileModifier] Commented out line {}: {}", line_number, line);
        } else {
            modified << line << "\n";
        }
    }

    return {modified.str(), modified_count};
}

std::string GCodeFileModifier::generate_header_comment(
    const std::string& original_filename,
    const std::vector<DetectedOperation>& ops_to_skip) const {

    std::ostringstream header;

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    header << "; ============================================================================\n";
    header << "; Modified by HelixScreen\n";
    header << "; Original file: " << original_filename << "\n";
    header << "; Modified at: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << "\n";
    header << "; \n";
    header << "; The following operations were disabled by user request:\n";

    for (const auto& op : ops_to_skip) {
        header << ";   - " << op.display_name();
        if (op.line_number > 0) {
            header << " (line " << op.line_number << ")";
        }
        header << "\n";
    }

    header << "; \n";
    header << "; Lines prefixed with '" << config_.skip_prefix << "' were originally active.\n";
    header << "; ============================================================================\n";
    header << "\n";

    return header.str();
}

void GCodeFileModifier::delete_temp_file(const std::string& moonraker_path) {
    spdlog::debug("[GCodeFileModifier] Deleting temp file: {}", moonraker_path);

    api_.delete_file(
        moonraker_path, []() { spdlog::debug("[GCodeFileModifier] Temp file deleted"); },
        [moonraker_path](const MoonrakerError& err) {
            // Log but don't fail - temp file cleanup is best-effort
            spdlog::warn("[GCodeFileModifier] Failed to delete temp file {}: {}", moonraker_path,
                         err.message);
        });
}

// ============================================================================
// JobHistoryPatcher Implementation
// ============================================================================

JobHistoryPatcher::JobHistoryPatcher(MoonrakerAPI& api) : api_(api) {}

void JobHistoryPatcher::patch_latest_job(const std::string& original_filename,
                                          SuccessCallback on_success, ErrorCallback on_error) {
    spdlog::info("[JobHistoryPatcher] Patching latest job to show filename: {}", original_filename);

    // TODO: Implement when HTTP support is added
    //
    // 1. GET /server/history/list?limit=1 to get latest job_id
    // 2. POST /server/history/job with job_id and new filename
    //
    // For now, log a warning

    spdlog::warn("[JobHistoryPatcher] Job history patching not yet implemented");
    spdlog::info("[JobHistoryPatcher] Would patch to show: {}", original_filename);

    // Don't treat this as an error - it's a nice-to-have feature
    if (on_success) {
        on_success();
    }
}

void JobHistoryPatcher::patch_job(const std::string& job_id,
                                   const std::string& original_filename,
                                   SuccessCallback on_success, ErrorCallback on_error) {
    spdlog::info("[JobHistoryPatcher] Patching job {} to show filename: {}", job_id,
                 original_filename);

    // TODO: Implement when HTTP support is added
    //
    // POST /server/history/job
    // Body: {"job_id": job_id, "filename": original_filename}

    spdlog::warn("[JobHistoryPatcher] Job history patching not yet implemented");

    if (on_success) {
        on_success();
    }
}

}  // namespace gcode
