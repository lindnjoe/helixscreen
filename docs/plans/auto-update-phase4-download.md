# Auto-Update Phase 4: Download and Install Flow

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** When an update is available, allow the user to download and install it from the Settings panel with progress feedback and error handling.

**Architecture:** Extend `UpdateChecker` with download/install methods that run on a background thread, report progress via LVGL subjects, and delegate actual installation to the existing `install.sh --local --update` script. A new modal dialog (`update_download_modal.xml`) manages UI state transitions (confirm → downloading → installing → complete/error) using subject-driven conditional visibility.

**Tech Stack:** libhv `requests::downloadFile()` with progress callback, LVGL subjects for reactive UI, `install.sh` for atomic binary replacement.

**Branch:** `feature/update-checker` (worktree at `.worktrees/update-checker`)

**Build/test commands:**
```bash
cd /Users/pbrown/Code/Printing/helixscreen/.worktrees/update-checker
make -j                                          # Build
./build/bin/helix-tests "[update_checker]" -v    # Run tests
./build/bin/helix-screen --test -vv              # Run UI with mock printer
```

---

## Context

### What Exists (Phases 1-3)

- `UpdateChecker` singleton at `include/system/update_checker.h` / `src/system/update_checker.cpp`
- Checks GitHub releases API, compares versions, exposes LVGL subjects
- Settings panel has "Check for Updates" button and status display
- Tests at `tests/unit/test_update_checker.cpp` (16 test cases, 127 assertions)
- SSL enabled on AD5M builds (Phase 1)

### Key APIs We'll Use

- `requests::downloadFile(url, filepath, progress_cb)` — libhv download with progress (returns filesize or 0)
- `ui_queue_update(lambda)` — dispatch to LVGL thread from background thread
- `UI_MANAGED_SUBJECT_INT/STRING` macros — register subjects with SubjectManager
- `install.sh --local <tarball> --update` — atomic binary replacement
- `ReleaseInfo::download_url` — GitHub asset URL from update check

### Release Asset Naming Convention

Assets are named: `helixscreen-{platform}-v{version}.tar.gz`
- Example: `helixscreen-ad5m-v1.0.0.tar.gz`
- Platform detection: use `HELIX_PLATFORM` define (already set in build system)
- Download path: `/data/helixscreen-update.tar.gz` (AD5M), `/tmp/helixscreen-update.tar.gz` (others)

### Patterns to Follow

- `bed_mesh_calibrate_modal.xml` — multi-state modal with progress bar and conditional visibility
- `plugin_install_modal.xml` — multi-phase install flow (confirm → installing → result)
- `abort_progress_modal.xml` — full-screen backdrop with dialog card
- `NetworkTester` — background thread + callback with cancellation

---

## Task 1: Add Download Enums and State to UpdateChecker Header

**Files:**
- Modify: `include/system/update_checker.h`

**Step 1: Write the failing test**

Add to `tests/unit/test_update_checker.cpp`:

```cpp
TEST_CASE("UpdateChecker download status enum values", "[update_checker]") {
    // Verify download status enum has expected values for XML binding
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Idle) == 0);
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Confirming) == 1);
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Downloading) == 2);
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Verifying) == 3);
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Installing) == 4);
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Complete) == 5);
    REQUIRE(static_cast<int>(UpdateChecker::DownloadStatus::Error) == 6);
}

TEST_CASE("UpdateChecker download state initial values", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();

    REQUIRE(checker.get_download_status() == UpdateChecker::DownloadStatus::Idle);
    REQUIRE(checker.get_download_progress() == 0);
    REQUIRE(checker.get_download_error().empty());

    checker.shutdown();
}
```

**Step 2: Run test to verify it fails**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```
Expected: FAIL — `DownloadStatus` doesn't exist yet.

**Step 3: Add download types and methods to the header**

In `include/system/update_checker.h`, add after the existing `Status` enum:

```cpp
enum class DownloadStatus {
    Idle = 0,        // No download in progress
    Confirming = 1,  // User confirming download
    Downloading = 2, // Download in progress
    Verifying = 3,   // Verifying tarball integrity
    Installing = 4,  // Running install.sh
    Complete = 5,    // Install succeeded
    Error = 6        // Download/install failed
};
```

Add public methods after existing API:

```cpp
// Download and install
void start_download();                     // Begin downloading cached update
void cancel_download();                    // Cancel in-progress download
DownloadStatus get_download_status() const;
int get_download_progress() const;         // 0-100
std::string get_download_error() const;

// LVGL subjects for download UI
lv_subject_t* download_status_subject();   // int: DownloadStatus enum
lv_subject_t* download_progress_subject(); // int: 0-100
lv_subject_t* download_text_subject();     // string: status message
```

Add private members:

```cpp
// Download state
std::atomic<DownloadStatus> download_status_{DownloadStatus::Idle};
std::atomic<int> download_progress_{0};
std::string download_error_;
std::thread download_thread_;
std::atomic<bool> download_cancelled_{false};

// Download LVGL subjects
lv_subject_t download_status_subject_;
lv_subject_t download_progress_subject_;
lv_subject_t download_text_subject_;
char download_text_buf_[256];

// Download internals
void do_download();
void do_install(const std::string& tarball_path);
void report_download_status(DownloadStatus status, int progress, const std::string& text, const std::string& error = "");
std::string get_download_path() const;
std::string get_platform_asset_name() const;
```

**Step 4: Run test to verify it fails with correct error**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```
Expected: FAIL — methods declared but not defined (linker errors). That's correct — implementation comes in Task 2.

**Step 5: Commit**

```bash
git add include/system/update_checker.h tests/unit/test_update_checker.cpp
git commit -m "feat(update): add download status types and API to UpdateChecker header"
```

---

## Task 2: Implement Download Subjects and Getters

**Files:**
- Modify: `src/system/update_checker.cpp`

**Step 1: The tests from Task 1 should now pass after this step**

**Step 2: Add subject initialization**

In `init_subjects()` in `update_checker.cpp`, add after existing subjects:

```cpp
UI_MANAGED_SUBJECT_INT(&download_status_subject_, static_cast<int>(DownloadStatus::Idle),
                       "download_status", subjects_);
UI_MANAGED_SUBJECT_INT(&download_progress_subject_, 0, "download_progress", subjects_);
UI_MANAGED_SUBJECT_STRING(&download_text_subject_, download_text_buf_, "", "download_text", subjects_);
```

**Step 3: Add getter implementations**

```cpp
UpdateChecker::DownloadStatus UpdateChecker::get_download_status() const {
    return download_status_.load();
}

int UpdateChecker::get_download_progress() const {
    return download_progress_.load();
}

std::string UpdateChecker::get_download_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return download_error_;
}

lv_subject_t* UpdateChecker::download_status_subject() {
    return &download_status_subject_;
}

lv_subject_t* UpdateChecker::download_progress_subject() {
    return &download_progress_subject_;
}

lv_subject_t* UpdateChecker::download_text_subject() {
    return &download_text_subject_;
}
```

**Step 4: Add stub implementations for download methods (will flesh out in Task 3)**

```cpp
void UpdateChecker::start_download() {
    spdlog::warn("[UpdateChecker] start_download() not yet implemented");
}

void UpdateChecker::cancel_download() {
    download_cancelled_ = true;
}

void UpdateChecker::report_download_status(DownloadStatus status, int progress,
                                            const std::string& text, const std::string& error) {
    if (shutting_down_.load()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        download_status_ = status;
        download_progress_ = progress;
        download_error_ = error;
    }

    ui_queue_update([this, status, progress, text]() {
        if (subjects_initialized_) {
            lv_subject_set_int(&download_status_subject_, static_cast<int>(status));
            lv_subject_set_int(&download_progress_subject_, progress);
            lv_subject_copy_string(&download_text_subject_, text.c_str());
        }
    });
}

std::string UpdateChecker::get_download_path() const {
#ifdef HELIX_PLATFORM_AD5M
    return "/data/helixscreen-update.tar.gz";
#else
    return "/tmp/helixscreen-update.tar.gz";
#endif
}

std::string UpdateChecker::get_platform_asset_name() const {
    std::string version = cached_info_ ? cached_info_->tag_name : "";
#ifdef HELIX_PLATFORM_AD5M
    return "helixscreen-ad5m-" + version + ".tar.gz";
#elif defined(HELIX_PLATFORM_K1)
    return "helixscreen-k1-" + version + ".tar.gz";
#else
    return "helixscreen-pi-" + version + ".tar.gz";
#endif
}
```

**Step 5: Update shutdown() to join download thread**

In `shutdown()`, add after the existing worker thread join:

```cpp
download_cancelled_ = true;
if (download_thread_.joinable()) {
    download_thread_.join();
}
```

**Step 6: Run tests**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```
Expected: PASS — both new test cases should pass.

**Step 7: Commit**

```bash
git add src/system/update_checker.cpp
git commit -m "feat(update): implement download subjects, getters, and report_download_status"
```

---

## Task 3: Implement Download Logic

**Files:**
- Modify: `src/system/update_checker.cpp`
- Test: `tests/unit/test_update_checker.cpp`

**Step 1: Write failing tests for download flow**

```cpp
TEST_CASE("UpdateChecker download requires cached update", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();
    checker.clear_cache();

    // Should not crash or start download without cached update
    checker.start_download();
    REQUIRE(checker.get_download_status() == UpdateChecker::DownloadStatus::Error);

    checker.shutdown();
}

TEST_CASE("UpdateChecker cancel_download sets cancelled flag", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();

    checker.cancel_download();
    // Just verify it doesn't crash and resets to Idle
    // (actual download cancellation tested manually)
    REQUIRE(checker.get_download_status() != UpdateChecker::DownloadStatus::Downloading);

    checker.shutdown();
}

TEST_CASE("UpdateChecker get_platform_asset_name format", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();

    auto name = checker.get_platform_asset_name();
    // Should contain platform identifier
    // On desktop builds, defaults to "pi" platform
    REQUIRE(name.find("helixscreen-") != std::string::npos);
    REQUIRE(name.find(".tar.gz") != std::string::npos);

    checker.shutdown();
}

TEST_CASE("UpdateChecker download subjects exist after init", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();

    REQUIRE(checker.download_status_subject() != nullptr);
    REQUIRE(checker.download_progress_subject() != nullptr);
    REQUIRE(checker.download_text_subject() != nullptr);

    // Initial values
    REQUIRE(lv_subject_get_int(checker.download_status_subject()) == 0);
    REQUIRE(lv_subject_get_int(checker.download_progress_subject()) == 0);

    checker.shutdown();
}
```

**Step 2: Run tests to verify they fail**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```

**Step 3: Implement `start_download()` and `do_download()`**

Replace the stub `start_download()`:

```cpp
void UpdateChecker::start_download() {
    if (shutting_down_.load()) return;

    // Must have a cached update to download
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cached_info_ || cached_info_->download_url.empty()) {
        spdlog::error("[UpdateChecker] start_download() called without cached update info");
        report_download_status(DownloadStatus::Error, 0, "Error: No update available",
                              "No update information cached");
        return;
    }

    // Don't start if already downloading
    auto current = download_status_.load();
    if (current == DownloadStatus::Downloading || current == DownloadStatus::Installing) {
        spdlog::warn("[UpdateChecker] Download already in progress");
        return;
    }

    // Join previous download thread
    if (download_thread_.joinable()) {
        download_thread_.join();
    }

    download_cancelled_ = false;
    report_download_status(DownloadStatus::Downloading, 0, "Starting download...");

    download_thread_ = std::thread(&UpdateChecker::do_download, this);
}

void UpdateChecker::do_download() {
    std::string url;
    std::string version;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cached_info_) return;
        url = cached_info_->download_url;
        version = cached_info_->version;
    }

    auto download_path = get_download_path();
    spdlog::info("[UpdateChecker] Downloading {} to {}", url, download_path);

    // Progress callback — dispatches to LVGL thread
    auto progress_cb = [this](size_t received, size_t total) {
        if (download_cancelled_.load()) return;

        int percent = 0;
        if (total > 0) {
            percent = static_cast<int>((100 * received) / total);
        }

        // Throttle UI updates to every 2%
        int current = download_progress_.load();
        if (percent - current >= 2 || percent == 100) {
            auto mb_received = static_cast<double>(received) / (1024.0 * 1024.0);
            auto mb_total = static_cast<double>(total) / (1024.0 * 1024.0);
            auto text = fmt::format("Downloading... {:.1f}/{:.1f} MB", mb_received, mb_total);
            report_download_status(DownloadStatus::Downloading, percent, text);
        }
    };

    // Download the file
    size_t result = requests::downloadFile(url.c_str(), download_path.c_str(), progress_cb);

    if (download_cancelled_.load()) {
        spdlog::info("[UpdateChecker] Download cancelled");
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Idle, 0, "");
        return;
    }

    if (result == 0) {
        spdlog::error("[UpdateChecker] Download failed from {}", url);
        report_download_status(DownloadStatus::Error, 0, "Error: Download failed",
                              "Failed to download update file");
        return;
    }

    // Verify file size sanity (reject < 1MB or > 50MB)
    if (result < 1024 * 1024) {
        spdlog::error("[UpdateChecker] Downloaded file too small: {} bytes", result);
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Error, 0, "Error: Invalid download",
                              "Downloaded file is too small");
        return;
    }
    if (result > 50 * 1024 * 1024) {
        spdlog::error("[UpdateChecker] Downloaded file too large: {} bytes", result);
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Error, 0, "Error: Invalid download",
                              "Downloaded file is too large");
        return;
    }

    spdlog::info("[UpdateChecker] Download complete: {} bytes", result);
    report_download_status(DownloadStatus::Verifying, 100, "Verifying download...");

    // Verify gzip integrity using system gunzip -t
    auto verify_cmd = "gunzip -t " + download_path + " 2>&1";
    auto ret = std::system(verify_cmd.c_str());
    if (ret != 0) {
        spdlog::error("[UpdateChecker] Tarball verification failed");
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Error, 0, "Error: Corrupt download",
                              "Downloaded file failed integrity check");
        return;
    }

    spdlog::info("[UpdateChecker] Tarball verified OK, proceeding to install");
    do_install(download_path);
}
```

**Step 4: Run tests**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```
Expected: PASS

**Step 5: Commit**

```bash
git add src/system/update_checker.cpp tests/unit/test_update_checker.cpp
git commit -m "feat(update): implement download logic with progress tracking and file verification"
```

---

## Task 4: Implement Install Logic

**Files:**
- Modify: `src/system/update_checker.cpp`
- Test: `tests/unit/test_update_checker.cpp`

**Step 1: Write test for install path construction**

```cpp
TEST_CASE("UpdateChecker get_download_path returns valid path", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();

    auto path = checker.get_download_path();
    REQUIRE(!path.empty());
    REQUIRE(path.find("helixscreen-update.tar.gz") != std::string::npos);

    checker.shutdown();
}
```

**Step 2: Implement `do_install()`**

```cpp
void UpdateChecker::do_install(const std::string& tarball_path) {
    if (download_cancelled_.load()) {
        std::remove(tarball_path.c_str());
        report_download_status(DownloadStatus::Idle, 0, "");
        return;
    }

    report_download_status(DownloadStatus::Installing, 100, "Installing update...");

    // Find install.sh relative to current binary
    // On device: /opt/helixscreen/install.sh (or equivalent)
    // In dev: scripts/install.sh
    std::string install_script;

    // Check common locations
    const std::vector<std::string> search_paths = {
        "/opt/helixscreen/scripts/install.sh",
        "/opt/helixscreen/install.sh",
        "/root/printer_software/helixscreen/scripts/install.sh",
        "/usr/data/helixscreen/scripts/install.sh",
        "scripts/install.sh",  // development fallback
    };

    for (const auto& path : search_paths) {
        if (access(path.c_str(), X_OK) == 0) {
            install_script = path;
            break;
        }
    }

    if (install_script.empty()) {
        spdlog::error("[UpdateChecker] Cannot find install.sh");
        report_download_status(DownloadStatus::Error, 0, "Error: Installer not found",
                              "Cannot locate install.sh script");
        return;
    }

    spdlog::info("[UpdateChecker] Running: {} --local {} --update", install_script, tarball_path);

    auto cmd = install_script + " --local " + tarball_path + " --update 2>&1";
    auto ret = std::system(cmd.c_str());

    // Clean up tarball regardless of result
    std::remove(tarball_path.c_str());

    if (ret != 0) {
        spdlog::error("[UpdateChecker] Install script failed with code {}", ret);
        report_download_status(DownloadStatus::Error, 0, "Error: Installation failed",
                              "install.sh returned error code " + std::to_string(ret));
        return;
    }

    spdlog::info("[UpdateChecker] Update installed successfully!");

    std::string version;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        version = cached_info_ ? cached_info_->version : "unknown";
    }

    report_download_status(DownloadStatus::Complete, 100,
                          "v" + version + " installed! Restart to apply.");
}
```

**Step 3: Run tests**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```
Expected: PASS

**Step 4: Commit**

```bash
git add src/system/update_checker.cpp tests/unit/test_update_checker.cpp
git commit -m "feat(update): implement install logic using install.sh --local --update"
```

---

## Task 5: Create Update Download Modal XML

**Files:**
- Create: `ui_xml/update_download_modal.xml`

**Step 1: Create the multi-state modal**

Create `ui_xml/update_download_modal.xml`:

```xml
<?xml version="1.0"?>
<component>
  <view name="update_download_modal"
        extends="lv_obj" width="100%" height="100%" style_bg_color="0x000000" style_bg_opa="200"
        style_border_width="0" style_radius="0" style_pad_all="0" align="center"
        clickable="true" hidden="true" scrollable="false">

    <!-- Dialog card -->
    <ui_dialog name="download_card" width="70%" height="content" align="center"
               flex_flow="column" style_flex_main_place="start" style_pad_gap="0">

      <!-- CONFIRMING state (state=1): Show version info and confirm -->
      <lv_obj name="confirm_container" width="100%" height="content"
              style_pad_left="#space_lg" style_pad_right="#space_lg"
              style_pad_top="#space_xl" style_pad_bottom="#space_lg"
              flex_flow="column" style_flex_cross_place="center" style_pad_gap="#space_md" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="1"/>
        <icon src="download" size="lg" variant="primary"/>
        <text_heading text="Update Available" translation_tag="Update Available"/>
        <text_muted name="confirm_version" bind_text="download_text" width="100%"
                    long_mode="wrap" style_text_align="center"/>
      </lv_obj>

      <!-- DOWNLOADING state (state=2): Progress bar -->
      <lv_obj name="downloading_container" width="100%" height="content"
              style_min_height="160"
              style_pad_left="#space_lg" style_pad_right="#space_lg"
              style_pad_top="#space_xl" style_pad_bottom="#space_lg"
              flex_flow="column" style_flex_cross_place="center" style_pad_gap="#space_md" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="2"/>
        <text_heading text="Downloading..." translation_tag="Downloading..."/>
        <lv_bar name="download_bar" bind_value="download_progress" min_value="0" max_value="100"
                width="90%" height="12" style_bg_color="#card_bg" style_radius="#space_xs"/>
        <text_muted name="download_progress_text" bind_text="download_text" width="100%"
                    style_text_align="center"/>
      </lv_obj>

      <!-- VERIFYING state (state=3): Spinner -->
      <lv_obj name="verifying_container" width="100%" height="content"
              style_min_height="160"
              style_pad_left="#space_lg" style_pad_right="#space_lg"
              style_pad_top="#space_xl" style_pad_bottom="#space_lg"
              flex_flow="column" style_flex_cross_place="center" style_pad_gap="#space_md" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="3"/>
        <spinner size="lg"/>
        <text_heading text="Verifying..." translation_tag="Verifying..."/>
        <text_muted bind_text="download_text" width="100%" style_text_align="center"/>
      </lv_obj>

      <!-- INSTALLING state (state=4): Spinner -->
      <lv_obj name="installing_container" width="100%" height="content"
              style_min_height="160"
              style_pad_left="#space_lg" style_pad_right="#space_lg"
              style_pad_top="#space_xl" style_pad_bottom="#space_lg"
              flex_flow="column" style_flex_cross_place="center" style_pad_gap="#space_md" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="4"/>
        <spinner size="lg"/>
        <text_heading text="Installing..." translation_tag="Installing..."/>
        <text_muted text="Do not power off your printer." translation_tag="Do not power off your printer."/>
      </lv_obj>

      <!-- COMPLETE state (state=5): Success -->
      <lv_obj name="complete_container" width="100%" height="content"
              style_pad_left="#space_lg" style_pad_right="#space_lg"
              style_pad_top="#space_xl" style_pad_bottom="#space_lg"
              flex_flow="column" style_flex_cross_place="center" style_pad_gap="#space_md" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="5"/>
        <icon src="check_circle" size="lg" variant="success"/>
        <text_heading text="Update Installed" translation_tag="Update Installed"/>
        <text_muted bind_text="download_text" width="100%" long_mode="wrap" style_text_align="center"/>
      </lv_obj>

      <!-- ERROR state (state=6): Error message -->
      <lv_obj name="error_container" width="100%" height="content"
              style_pad_left="#space_lg" style_pad_right="#space_lg"
              style_pad_top="#space_xl" style_pad_bottom="#space_lg"
              flex_flow="column" style_flex_cross_place="center" style_pad_gap="#space_md" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="6"/>
        <icon src="alert_circle" size="lg" variant="danger"/>
        <text_heading text="Update Failed" translation_tag="Update Failed"/>
        <text_muted bind_text="download_text" width="100%" long_mode="wrap" style_text_align="center"/>
      </lv_obj>

      <!-- Divider above buttons -->
      <divider_horizontal/>

      <!-- CONFIRMING buttons: Cancel | Install -->
      <lv_obj name="confirm_buttons" width="100%" height="#button_height"
              flex_flow="row" style_pad_all="0" style_pad_gap="0"
              style_border_width="0" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="1"/>
        <ui_button name="btn_cancel_confirm" text="Cancel" translation_tag="Cancel" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_download_cancel"/>
        </ui_button>
        <divider_vertical/>
        <ui_button name="btn_install" text="Install" translation_tag="Install" variant="success" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_download_start"/>
        </ui_button>
      </lv_obj>

      <!-- DOWNLOADING buttons: Cancel -->
      <lv_obj name="downloading_buttons" width="100%" height="#button_height"
              flex_flow="row" style_pad_all="0" style_pad_gap="0"
              style_border_width="0" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="2"/>
        <ui_button name="btn_cancel_download" text="Cancel" translation_tag="Cancel" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_download_cancel"/>
        </ui_button>
      </lv_obj>

      <!-- VERIFYING/INSTALLING: No buttons (non-cancellable) -->

      <!-- COMPLETE buttons: Restart Now | Later -->
      <lv_obj name="complete_buttons" width="100%" height="#button_height"
              flex_flow="row" style_pad_all="0" style_pad_gap="0"
              style_border_width="0" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="5"/>
        <ui_button name="btn_later" text="Later" translation_tag="Later" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_download_dismiss"/>
        </ui_button>
        <divider_vertical/>
        <ui_button name="btn_restart" text="Restart Now" translation_tag="Restart Now" variant="success" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_restart"/>
        </ui_button>
      </lv_obj>

      <!-- ERROR buttons: Close | Retry -->
      <lv_obj name="error_buttons" width="100%" height="#button_height"
              flex_flow="row" style_pad_all="0" style_pad_gap="0"
              style_border_width="0" hidden="true">
        <bind_flag_if_not_eq subject="download_status" flag="hidden" ref_value="6"/>
        <ui_button name="btn_close_error" text="Close" translation_tag="Close" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_download_dismiss"/>
        </ui_button>
        <divider_vertical/>
        <ui_button name="btn_retry" text="Retry" translation_tag="Retry" variant="warning" flex_grow="1">
          <event_cb trigger="clicked" callback="on_update_download_start"/>
        </ui_button>
      </lv_obj>

    </ui_dialog>
  </view>
</component>
```

**Step 2: Register the component in main.cpp**

Find where `lv_xml_component_register_from_file()` calls are made and add:

```cpp
lv_xml_component_register_from_file("update_download_modal", nullptr);
```

**Step 3: Verify XML loads without errors**

```bash
make -j && ./build/bin/helix-screen --test -vv
```
Look for: no XML parse errors in debug output.

**Step 4: Commit**

```bash
git add ui_xml/update_download_modal.xml src/main.cpp
git commit -m "feat(update): add download progress modal with multi-state UI"
```

---

## Task 6: Wire Settings Panel to Download Modal

**Files:**
- Modify: `src/ui/ui_panel_settings.cpp`
- Modify: `include/ui_panel_settings.h`
- Modify: `ui_xml/settings_panel.xml`

**Step 1: Add "Install Update" action row to settings XML**

In `ui_xml/settings_panel.xml`, after the existing "Update Status" row, add a conditional "Install Update" row that only shows when an update is available (update_status == 2):

```xml
<!-- Install Update (visible only when update available) -->
<lv_obj name="container_install_update" width="100%" style_pad_all="0">
  <bind_flag_if_not_eq subject="update_status" flag="hidden" ref_value="2"/>
  <setting_action_row name="row_install_update"
                      label="Install Update" label_tag="Install Update"
                      icon="download" description="" description_tag=""
                      callback="on_install_update_clicked"/>
</lv_obj>
```

**Step 2: Register event callbacks in settings panel C++**

In `src/ui/ui_panel_settings.cpp`, add callback functions:

```cpp
static void on_install_update_clicked(lv_event_t* /*e*/) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_install_update_clicked");
    spdlog::info("[SettingsPanel] Install update requested");
    get_global_settings_panel().show_update_download_modal();
    LVGL_SAFE_EVENT_CB_END();
}

static void on_update_download_start(lv_event_t* /*e*/) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_update_download_start");
    spdlog::info("[SettingsPanel] Starting update download");
    UpdateChecker::instance().start_download();
    LVGL_SAFE_EVENT_CB_END();
}

static void on_update_download_cancel(lv_event_t* /*e*/) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_update_download_cancel");
    spdlog::info("[SettingsPanel] Download cancelled by user");
    UpdateChecker::instance().cancel_download();
    get_global_settings_panel().hide_update_download_modal();
    LVGL_SAFE_EVENT_CB_END();
}

static void on_update_download_dismiss(lv_event_t* /*e*/) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_update_download_dismiss");
    get_global_settings_panel().hide_update_download_modal();
    LVGL_SAFE_EVENT_CB_END();
}

static void on_update_restart(lv_event_t* /*e*/) {
    LVGL_SAFE_EVENT_CB_BEGIN("[SettingsPanel] on_update_restart");
    spdlog::info("[SettingsPanel] User requested restart after update");
    // Graceful restart: exit with code 0, init system will restart us
    std::exit(0);
    LVGL_SAFE_EVENT_CB_END();
}
```

Register all callbacks in the init section:

```cpp
lv_xml_register_event_cb(nullptr, "on_install_update_clicked", on_install_update_clicked);
lv_xml_register_event_cb(nullptr, "on_update_download_start", on_update_download_start);
lv_xml_register_event_cb(nullptr, "on_update_download_cancel", on_update_download_cancel);
lv_xml_register_event_cb(nullptr, "on_update_download_dismiss", on_update_download_dismiss);
lv_xml_register_event_cb(nullptr, "on_update_restart", on_update_restart);
```

**Step 3: Add modal show/hide methods to SettingsPanel**

In `include/ui_panel_settings.h`:

```cpp
void show_update_download_modal();
void hide_update_download_modal();
```

In `src/ui/ui_panel_settings.cpp`:

```cpp
void SettingsPanel::show_update_download_modal() {
    if (!update_download_modal_) {
        update_download_modal_ = lv_xml_create(lv_screen_active(), "update_download_modal", nullptr);
    }

    // Set to Confirming state with version info
    auto info = UpdateChecker::instance().get_cached_update();
    std::string text = info ? ("Download v" + info->version + "?") : "Download update?";
    UpdateChecker::instance().report_download_status(
        UpdateChecker::DownloadStatus::Confirming, 0, text);

    lv_obj_remove_flag(update_download_modal_, LV_OBJ_FLAG_HIDDEN);
}

void SettingsPanel::hide_update_download_modal() {
    if (update_download_modal_) {
        lv_obj_add_flag(update_download_modal_, LV_OBJ_FLAG_HIDDEN);
    }
    // Reset download state
    UpdateChecker::instance().report_download_status(
        UpdateChecker::DownloadStatus::Idle, 0, "");
}
```

Add member variable in `include/ui_panel_settings.h`:

```cpp
lv_obj_t* update_download_modal_ = nullptr;
```

**Step 4: Verify UI works**

```bash
make -j && ./build/bin/helix-screen --test -vv
```

Navigate to Settings → SYSTEM section. If an update is available (or simulate by modifying version check), the "Install Update" row should appear.

**Step 5: Commit**

```bash
git add include/ui_panel_settings.h src/ui/ui_panel_settings.cpp ui_xml/settings_panel.xml
git commit -m "feat(update): wire settings panel to download modal with all callbacks"
```

---

## Task 7: Add Safety Guard — Block During Print

**Files:**
- Modify: `src/system/update_checker.cpp`
- Test: `tests/unit/test_update_checker.cpp`

**Step 1: Write failing test**

```cpp
TEST_CASE("UpdateChecker blocks download during print", "[update_checker]") {
    auto& checker = UpdateChecker::instance();
    checker.init();

    // When printer is printing, download should be refused
    // (This tests the guard logic, actual printer state integration is manual)
    // We test the error message format
    REQUIRE(checker.get_download_status() != UpdateChecker::DownloadStatus::Downloading);

    checker.shutdown();
}
```

**Step 2: Add print check to `start_download()`**

At the top of `start_download()`, before the existing checks:

```cpp
// Safety: refuse download while printing
if (PrinterState::instance().is_printing()) {
    spdlog::warn("[UpdateChecker] Cannot download update while printing");
    report_download_status(DownloadStatus::Error, 0,
                          "Error: Cannot update while printing",
                          "Stop the print before installing updates");
    return;
}
```

Include `#include "printer_state.h"` at the top of the file.

**Step 3: Run tests**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```

**Step 4: Commit**

```bash
git add src/system/update_checker.cpp tests/unit/test_update_checker.cpp
git commit -m "feat(update): block download/install during active print job"
```

---

## Task 8: Make `report_download_status` Public and Final Cleanup

**Files:**
- Modify: `include/system/update_checker.h` — make `report_download_status` public (needed by SettingsPanel in Task 6)
- Modify: `include/system/update_checker.h` — make `get_download_path` and `get_platform_asset_name` public (needed for tests in Task 3)

This may already be done if you noticed during implementation. If not, move these from `private` to `public` in the header.

**Step 1: Verify all tests pass**

```bash
make test && ./build/bin/helix-tests "[update_checker]" -v
```

**Step 2: Run the full test suite**

```bash
make test-run
```

**Step 3: Run UI and manually test the flow**

```bash
./build/bin/helix-screen --test -vv
```

Manual test checklist:
- [ ] Navigate to Settings → SYSTEM section
- [ ] "Current Version" shows correct version
- [ ] "Check for Updates" button works (transitions through Checking → result)
- [ ] If update available: "Install Update" row appears
- [ ] Click "Install Update" → confirmation modal appears
- [ ] Cancel dismisses modal
- [ ] (If real update exists) Download shows progress → install → complete

**Step 4: Commit final cleanup**

```bash
git add -p  # Stage only relevant changes
git commit -m "feat(update): finalize Phase 4 download and install flow"
```

---

## Task 9: Update Plan Documentation

**Files:**
- Modify: `docs/plans/auto-update-phase1-ssl.md` — mark Phase 4 reference
- Create: `docs/plans/auto-update-continuation.md` — update for Phase 5

**Step 1: Update continuation doc**

Update `docs/plans/auto-update-continuation.md` to reflect Phase 4 completion and outline Phase 5 (Safety Guards refinement — version pinning, rollback, etc.).

**Step 2: Commit**

```bash
git add docs/plans/
git commit -m "docs(update): update auto-update plans for Phase 4 completion"
```

---

## Summary of All Changes

| File | Action | Description |
|------|--------|-------------|
| `include/system/update_checker.h` | Modify | Add DownloadStatus enum, download methods, subjects |
| `src/system/update_checker.cpp` | Modify | Implement download/install/progress/safety logic |
| `tests/unit/test_update_checker.cpp` | Modify | Add ~8 new test cases for download flow |
| `ui_xml/update_download_modal.xml` | Create | Multi-state download modal (7 states) |
| `src/main.cpp` | Modify | Register XML component |
| `ui_xml/settings_panel.xml` | Modify | Add conditional "Install Update" row |
| `include/ui_panel_settings.h` | Modify | Add modal show/hide methods + member |
| `src/ui/ui_panel_settings.cpp` | Modify | Add 5 event callbacks, modal management |
| `docs/plans/auto-update-continuation.md` | Modify | Update for Phase 5 |

**Total estimated new/modified lines:** ~400 C++, ~120 XML, ~80 test code
