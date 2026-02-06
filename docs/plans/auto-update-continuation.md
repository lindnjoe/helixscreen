# Auto-Update Feature - Session Continuation Prompt

**Copy everything below and paste as your prompt to continue this work:**

---

## Continue: Auto-Update Feature for HelixScreen - Phase 5

### Completed Phases

#### Phase 1: SSL Enablement ✅
- Commit: `66b7181f feat(build): enable SSL/TLS on AD5M builds for HTTPS support`
- Added OpenSSL 1.1.1w cross-compilation to `docker/Dockerfile.ad5m`
- Changed `ENABLE_SSL := yes` in `mk/cross.mk` for AD5M
- Plan doc: `docs/plans/auto-update-phase1-ssl.md`

#### Phase 2: Update Checker Service ✅
- Commit: `6cab1bac feat(system): add UpdateChecker service for GitHub release checking`
- Files: `include/system/update_checker.h`, `src/system/update_checker.cpp`
- Tests: `tests/unit/test_update_checker.cpp`
- Features: async HTTP to GitHub API, version comparison, rate limiting (1hr), thread-safe
- Plan doc: `docs/plans/auto-update-phase2-checker.md`

#### Phase 3: Settings UI Integration ✅
- Commit: `9882223d feat(settings): add Software Update section with LVGL subject binding (Phase 3)`
- Added 4 LVGL subjects to UpdateChecker for declarative UI binding
- Added "Software Update" section to Settings panel (3 XML rows)
- Plan doc: `docs/plans/auto-update-phase3-settings-ui.md`

#### Phase 4: Download and Install Flow ✅
- Commits:
  - `057690ed feat(update): add download status types, subjects, and getters to UpdateChecker`
  - `1945fa99 feat(update): implement download with progress tracking and install via install.sh`
  - `02ab76ec feat(update): add download progress modal and wire settings panel`
  - `1242603c feat(update): block download/install during active print job`
- Added DownloadStatus enum (7 states) and 3 new LVGL subjects for download UI
- Implemented download via `requests::downloadFile()` with progress callback
- Implemented install via `install.sh --local <tarball> --update`
- Created `update_download_modal.xml` with multi-state UI (confirm → download → verify → install → complete/error)
- Added "Install Update" conditional row to settings panel (visible when update available)
- Safety guard: blocks download during active print
- Tests: 24 test cases, 149 assertions — all pass
- Plan doc: `docs/plans/auto-update-phase4-download.md`

**Branch:** `feature/update-checker` (worktree at `.worktrees/update-checker`)

---

### What Phase 4 Delivers

The Settings panel SYSTEM section now has:
1. **Current Version** — shows HELIX_VERSION
2. **Check for Updates** — triggers async GitHub API check
3. **Update Status** — shows result (up to date / version available / error)
4. **Install Update** — appears only when update is available (status == 2)

Clicking "Install Update" opens a modal that walks through:
- **Confirm**: "Download v1.2.3?" with Cancel/Install buttons
- **Downloading**: Progress bar with MB counter, Cancel button
- **Verifying**: Spinner, non-cancellable
- **Installing**: Spinner + "Do not power off" warning, non-cancellable
- **Complete**: Success icon, Later/Restart Now buttons
- **Error**: Error icon + message, Close/Retry buttons

Download subjects:
- `download_status` (int) — DownloadStatus enum (0-6)
- `download_progress` (int) — 0-100 percentage
- `download_text` (string) — human-readable status message

---

### Remaining Manual Testing

1. Run `./build/bin/helix-screen --test -vv` and navigate to Settings → SYSTEM
2. Verify "Current Version" shows correct version
3. Click "Check for Updates" — verify status transitions
4. If update available: verify "Install Update" row appears
5. Click "Install Update" — verify confirmation modal
6. Build for AD5M: `make ad5m-docker`
7. Deploy to AD5M: `make deploy-ad5m` — verify real GitHub API + download works

---

### What's Next: Phase 5 — Safety Guards and Polish

**Goal:** Harden the update flow for production use.

**Items:**
1. **Rollback capability** — Keep previous binary, restore if install fails
2. **Version pinning** — Allow user to skip/dismiss specific versions
3. **Automatic check on startup** — 5 seconds after UI ready (background timer)
4. **Daily timer** — Re-check if idle for 24 hours (not printing)
5. **Release notes display** — Show markdown release notes in confirmation dialog
6. **AD5M storage check** — Verify `/data/` has enough free space before download
7. **CA certificate handling** — Verify libhv/OpenSSL finds `/etc/ssl/certs/` on AD5M

---

### Build & Test Commands

```bash
cd /Users/pbrown/Code/Printing/helixscreen/.worktrees/update-checker

make -j                                          # Build binary
make test && ./build/bin/helix-tests "[update_checker]" --verbosity high  # Tests
./build/bin/helix-screen --test -vv              # Run UI with mock printer
make test-run                                     # Full test suite

make ad5m-docker                                  # Cross-compile for AD5M
make deploy-ad5m                                  # Deploy to device
```

---

### Key Files

| File | Purpose |
|------|---------|
| `include/system/update_checker.h` | UpdateChecker API: check, download, install |
| `src/system/update_checker.cpp` | Implementation with threading and progress |
| `tests/unit/test_update_checker.cpp` | 24 test cases |
| `ui_xml/update_download_modal.xml` | Multi-state download/install modal |
| `ui_xml/settings_panel.xml` | Settings panel with update rows |
| `src/ui/ui_panel_settings.cpp` | Callbacks and modal management |
| `docs/plans/auto-update-phase4-download.md` | Phase 4 implementation plan |

---

### Plan Documents

| Phase | Doc | Status |
|-------|-----|--------|
| 1 - SSL | `docs/plans/auto-update-phase1-ssl.md` | ✅ Complete |
| 2 - Checker | `docs/plans/auto-update-phase2-checker.md` | ✅ Complete |
| 3 - Settings UI | `docs/plans/auto-update-phase3-settings-ui.md` | ✅ Complete |
| 4 - Download | `docs/plans/auto-update-phase4-download.md` | ✅ Complete |
| 5 - Safety | (not yet created) | Not started |
