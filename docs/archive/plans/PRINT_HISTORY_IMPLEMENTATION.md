# Print History Feature Implementation Plan

## Current Status

| Stage | Name | Status | Commits |
|-------|------|--------|---------|
| 1 | Data Layer Foundation | ✅ Complete | Merged to main |
| 2 | Dashboard Panel - Stats Display | ✅ Complete | `f5379eb` and earlier |
| 3 | History List Panel - Basic List | ✅ Complete | `f5379eb` |
| 4 | Search, Filter, Sort | ✅ Complete | `d3c57b9`, `ad69972`, `d013da5` |
| 5 | Print Details Overlay | ✅ Complete | `2d1de9f` |
| 6 | Dashboard Charts | ✅ Complete | Session 2025-12-10 |
| 7 | Small Screen Adaptations | 🔲 Not Started | - |

**Branch**: `feature/print-history`
**Worktree**: `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Last Updated**: 2025-12-10

### Session 3 Bug Fixes (2025-12-08)

Three bugs discovered and fixed during testing:

1. **`cc04ef8`** - Back button in dashboard not wired
   - Missing `ui_panel_setup_back_button()` call in `ui_panel_history_dashboard.cpp`

2. **`ad69972`** - Dropdown chevrons not rendering
   - Must set BOTH symbol AND `LV_PART_INDICATOR` font to MDI

3. **`d013da5`** - Search/filter/sort not working
   - Event callbacks registered in `setup()` (AFTER XML creation)
   - Moved to `init_global_history_list_panel()` (BEFORE XML creation)

### Key Lessons Learned

1. **XML event callbacks must be registered BEFORE `lv_xml_create()`** - the parser looks up callbacks during parsing, not after.

2. **Dropdown chevrons require TWO steps**: `lv_dropdown_set_symbol()` + `lv_obj_set_style_text_font(..., LV_PART_INDICATOR)`.

3. **Follow the pattern exactly** - compare with working code (`ui_panel_history_dashboard.cpp` vs `ui_panel_history_list.cpp`).

---

## Overview

A two-panel print history feature providing printer lifetime statistics and historical print records.

**Entry Point**: Advanced panel → "Print History" action row
**Target**: Moonraker's `server.history.*` API endpoints

### Components

| Component | Type | Description |
|-----------|------|-------------|
| **Dashboard Panel** | Overlay | Stats summary with sparklines, bar chart, time filters |
| **History List Panel** | Full-screen | Scrollable list with search/filter/sort/actions |
| **Detail Overlay** | Overlay | Full print metadata with Reprint/Delete actions |

---

## Navigation Flow

```
Advanced Panel
    └── "Print History" action row (click)
            └── Dashboard Panel (overlay via ui_nav_push_overlay)
                    ├── Back button → Advanced Panel
                    └── "View Full History" button
                            └── History List Panel (full-screen panel)
                                    ├── Back button → Dashboard
                                    └── List row click
                                            └── Detail Overlay
                                                    ├── Reprint → start_print()
                                                    ├── Delete → confirm → delete_history_job()
                                                    └── Close → History List
```

---

## File Structure

### New Files to Create

```
include/
  print_history_data.h              # Data structures (PrintHistoryJob, PrintHistoryTotals)
  ui_panel_history_dashboard.h      # Dashboard panel class
  ui_panel_history_list.h           # History list panel class

src/
  ui_panel_history_dashboard.cpp    # Dashboard implementation
  ui_panel_history_list.cpp         # History list implementation

ui_xml/
  history_dashboard_panel.xml       # Dashboard stats layout
  history_list_panel.xml            # Full-screen list layout
  history_list_row.xml              # List item component
  history_detail_overlay.xml        # Print details overlay
```

### Files to Modify

```
include/moonraker_api.h             # Add history API method signatures
src/moonraker_api.cpp               # Implement history API methods
src/moonraker_client_mock.cpp       # Add mock history responses
ui_xml/advanced_panel.xml           # Add "Print History" action row
src/ui_panel_advanced.cpp           # Wire up action row click handler
src/main.cpp                        # Register XML components, instantiate panels
```

---

## Data Structures

### `print_history_data.h`

```cpp
#pragma once

#include <string>
#include <cstdint>

namespace helix {

/**
 * @brief Single print job from Moonraker history
 *
 * Maps to server.history.list response structure.
 * See: https://moonraker.readthedocs.io/en/latest/web_api/#get-job-list
 */
struct PrintHistoryJob {
    std::string job_id;           // Unique job identifier
    std::string filename;         // G-code filename
    std::string status;           // "completed", "cancelled", "error", "in_progress"
    double start_time = 0.0;      // Unix timestamp
    double end_time = 0.0;        // Unix timestamp
    double print_duration = 0.0;  // Seconds of actual printing
    double total_duration = 0.0;  // Total job time including pauses
    double filament_used = 0.0;   // Filament in mm
    bool exists = false;          // File still exists on disk

    // Metadata from G-code file
    std::string filament_type;    // PLA, PETG, ABS, etc.
    uint32_t layer_count = 0;
    double layer_height = 0.0;
    double nozzle_temp = 0.0;
    double bed_temp = 0.0;
    std::string thumbnail_path;   // Path to cached thumbnail

    // Pre-formatted strings for display
    std::string duration_str;     // "2h 15m"
    std::string date_str;         // "Dec 1, 14:30"
    std::string filament_str;     // "12.5m"
};

/**
 * @brief Aggregated history statistics
 *
 * Maps to server.history.totals response.
 */
struct PrintHistoryTotals {
    uint64_t total_jobs = 0;
    uint64_t total_time = 0;         // Seconds
    double total_filament_used = 0.0; // mm
    uint64_t total_completed = 0;
    uint64_t total_cancelled = 0;
    uint64_t total_failed = 0;
    double longest_job = 0.0;        // Seconds
};

/**
 * @brief Time filter for dashboard queries
 */
enum class HistoryTimeFilter {
    DAY,      // Last 24 hours
    WEEK,     // Last 7 days
    MONTH,    // Last 30 days
    YEAR,     // Last 365 days
    ALL_TIME  // No filter
};

/**
 * @brief Filament usage aggregated by material type
 */
struct FilamentUsageByType {
    std::string type;        // "PLA", "PETG", etc.
    double usage_mm = 0.0;
    uint32_t print_count = 0;
};

} // namespace helix
```

---

## Moonraker API Additions

### Method Signatures (add to `moonraker_api.h`)

```cpp
// Callback types
using HistoryListCallback = std::function<void(const std::vector<PrintHistoryJob>&, uint64_t total_count)>;
using HistoryTotalsCallback = std::function<void(const PrintHistoryTotals&)>;
using HistoryJobCallback = std::function<void(const PrintHistoryJob&)>;

/**
 * @brief Get paginated list of print history jobs
 * @param limit Max jobs to return (default 50)
 * @param start Offset for pagination
 * @param since Unix timestamp - only jobs after this time (0 = no filter)
 * @param before Unix timestamp - only jobs before this time (0 = no filter)
 */
void get_history_list(int limit, int start, double since, double before,
                      HistoryListCallback on_success, ErrorCallback on_error);

/**
 * @brief Get aggregated history totals
 */
void get_history_totals(HistoryTotalsCallback on_success, ErrorCallback on_error);

/**
 * @brief Delete a job from history
 */
void delete_history_job(const std::string& job_id,
                        SuccessCallback on_success, ErrorCallback on_error);
```

### Moonraker Endpoints

| Method | Endpoint | Purpose |
|--------|----------|---------|
| `get_history_list` | `server.history.list` | Paginated job list |
| `get_history_totals` | `server.history.totals` | Aggregated statistics |
| `delete_history_job` | `server.history.delete_job` | Remove from history |

---

## Implementation Stages

### Stage 1: Data Layer Foundation

**Goal**: Implement Moonraker history API methods and data structures

**Files to Create/Modify**:
- `include/print_history_data.h` (new)
- `include/moonraker_api.h` (modify)
- `src/moonraker_api.cpp` (modify)
- `src/moonraker_client_mock.cpp` (modify)

**Implementation Steps**:
1. Create `print_history_data.h` with structs above
2. Add callback types and method signatures to `moonraker_api.h`
3. Implement `get_history_list()` in `moonraker_api.cpp`:
   - Build JSON-RPC params: `{"limit": N, "start": N, "since": N, "before": N}`
   - Parse response `jobs` array into `PrintHistoryJob` vector
   - Extract metadata fields, format duration/date strings
4. Implement `get_history_totals()`:
   - Parse response into `PrintHistoryTotals` struct
5. Implement `delete_history_job()`:
   - Build params: `{"uid": job_id}`
6. Add mock responses to `moonraker_client_mock.cpp`

**Success Criteria**:
- [ ] `get_history_list()` returns parsed `PrintHistoryJob` vector
- [ ] `get_history_totals()` returns `PrintHistoryTotals` struct
- [ ] `delete_history_job()` calls Moonraker and invokes callback
- [ ] Mock mode (`--test`) returns 20 realistic mock jobs
- [ ] Unit tests pass for JSON parsing

**Testing**:
```bash
# Run with mock to verify API works
./build/bin/helix-screen --test -vvv

# Check logs for history API calls (add debug logging temporarily)
```

**Mock Data Requirements**:
- 20 jobs spread across last 30 days
- Status distribution: 70% completed, 15% cancelled, 15% failed
- Duration range: 5 minutes to 8 hours
- Filament types: PLA (50%), PETG (25%), ABS (15%), TPU (10%)
- Filenames: benchy.gcode, calibration_cube.gcode, phone_stand.gcode, etc.

---

### Stage 2: Dashboard Panel - Stats Display

**Goal**: Create dashboard panel showing stats, accessible from Advanced panel

**Files to Create/Modify**:
- `ui_xml/history_dashboard_panel.xml` (new)
- `include/ui_panel_history_dashboard.h` (new)
- `src/ui_panel_history_dashboard.cpp` (new)
- `ui_xml/advanced_panel.xml` (modify - add action row)
- `src/ui_panel_advanced.cpp` (modify - wire click handler)
- `src/main.cpp` (modify - register component)

**Implementation Steps**:
1. Create XML layout following `overlay_panel.xml` pattern:
   - Header bar with back button and "Print History" title
   - Time filter row: 5 segmented buttons (Day/Week/Month/Year/All)
   - Stats grid: 6 `ui_card` components for metrics
   - "View Full History" button at bottom
   - Empty state container (hidden when data exists)
2. Create `HistoryDashboardPanel` class extending `PanelBase`:
   - Subjects for each stat value
   - `refresh_data()` method calling API
   - `set_time_filter()` method
   - Static callbacks for filter buttons
3. Add action row to `advanced_panel.xml`
4. Wire click handler in `ui_panel_advanced.cpp` using lazy-create pattern
5. Register XML component in `main.cpp`

**Success Criteria**:
- [ ] "Print History" row appears in Advanced panel
- [ ] Clicking row opens dashboard overlay (slides in from right)
- [ ] Dashboard shows 6 stat cards with values from Moonraker
- [ ] Time filter buttons update displayed stats
- [ ] Back button returns to Advanced panel
- [ ] Empty state shows "No print history" when no jobs

**Testing**:
```bash
# Test with mock data
./build/bin/helix-screen --test -p advanced

# Verify:
# 1. Click "Print History" row
# 2. Dashboard overlay appears
# 3. Stats show non-zero values
# 4. Time filters change displayed values
# 5. Back button works
```

**Reference Files**:
- `ui_xml/overlay_panel.xml` - Overlay structure
- `ui_xml/home_panel.xml` - Card layout patterns
- `src/ui_panel_settings.cpp` - Action row click handling

---

### Stage 3: History List Panel - Basic List

**Goal**: Create full-screen list panel with print history items

**Files to Create/Modify**:
- `ui_xml/history_list_panel.xml` (new)
- `ui_xml/history_list_row.xml` (new)
- `include/ui_panel_history_list.h` (new)
- `src/ui_panel_history_list.cpp` (new)
- `src/ui_panel_history_dashboard.cpp` (modify - add navigation)
- `src/main.cpp` (modify - register component)

**Implementation Steps**:
1. Create `history_list_row.xml` component:
   - Props: filename, date, duration, filament_type, status_icon, thumbnail
   - Layout: 48x48 thumbnail, filename (flex_grow), metadata row, status icon
2. Create `history_list_panel.xml`:
   - Header bar with back button
   - Placeholder for filter/search row (Stage 4)
   - Scrollable list container (`flex_flow="column"`, `scrollable="true"`)
   - Empty state message
3. Create `HistoryListPanel` class:
   - `refresh_history()` - fetch and populate list
   - `populate_list()` - create row widgets dynamically
   - `attach_row_click_handler()` - prepare for Stage 5
4. Wire "View Full History" button in dashboard to open list panel
5. Wire back button to return to dashboard

**Success Criteria**:
- [ ] "View Full History" button opens full-screen list panel
- [ ] List shows all history items with correct data
- [ ] Each row displays: thumbnail, filename, date, duration, filament type, status icon
- [ ] List scrolls smoothly with 20+ items
- [ ] Back button returns to dashboard
- [ ] Empty state shows when no history

**Testing**:
```bash
./build/bin/helix-screen --test -p advanced

# Verify:
# 1. Navigate: Advanced → Print History → View Full History
# 2. List panel appears with items
# 3. Scroll through list
# 4. Back button returns to dashboard
# 5. Back again returns to Advanced
```

**Reference Files**:
- `ui_xml/print_file_list_row.xml` - Row component pattern
- `src/ui_panel_print_select.cpp:761-792` - Dynamic list population
- `src/ui_panel_print_select.cpp:974-983` - Row click handlers

---

### Stage 4: List Features - Search, Filter, Sort

**Goal**: Add search, status filter, and sort functionality to list

**Files to Modify**:
- `ui_xml/history_list_panel.xml` (add filter/search row)
- `src/ui_panel_history_list.cpp` (implement filtering/sorting)

**Implementation Steps**:
1. Add filter/search row to XML:
   - `lv_textarea` for search (single line, placeholder "Search...")
   - `lv_dropdown` for status: "All", "Completed", "Failed", "Cancelled"
   - `lv_dropdown` for sort: "Date", "Duration", "Filename", "Status"
2. Implement search:
   - Filter `jobs_` vector by filename substring match (case-insensitive)
   - Debounce input (300ms) to avoid excessive re-filtering
3. Implement status filter:
   - Filter by `status` field matching dropdown selection
4. Implement sort:
   - Sort by selected column, toggle ascending/descending on re-select
5. Chain filters: search → status filter → sort → display

**Success Criteria**:
- [ ] Search box filters list by filename as user types
- [ ] Status dropdown filters to selected status only
- [ ] Sort dropdown changes list order
- [ ] Filters combine correctly (search + status + sort)
- [ ] List updates reactively without full refresh
- [ ] "No matching prints" shown when filters yield empty results

**Testing**:
```bash
./build/bin/helix-screen --test -p advanced

# Verify:
# 1. Type in search box - list filters
# 2. Select "Failed" status - only failed prints show
# 3. Change sort to "Duration" - list reorders
# 4. Clear search - full list returns
# 5. Select "Completed" + search "benchy" - combined filter works
```

**Reference Files**:
- `src/ui_panel_print_select.cpp:262-285` - Sort implementation
- `ui_xml/print_select_panel.xml` - Header with sort indicators

---

### Stage 5: Print Details Overlay

**Goal**: Create detail overlay with metadata and actions

**Files to Create/Modify**:
- `ui_xml/history_detail_overlay.xml` (new)
- `src/ui_panel_history_list.cpp` (add detail overlay handling)

**Implementation Steps**:
1. Create `history_detail_overlay.xml`:
   - Header bar with "Print Details" title
   - Status section with icon and colored text
   - Stats grid (2 columns):
     - Started / Ended timestamps
     - Duration / Layers
     - Nozzle Temp / Bed Temp
     - Layer Height / Filament Used
   - Action buttons: "Reprint", "Delete"
2. Add subjects for detail overlay fields
3. Implement `show_detail_overlay()`:
   - Update subjects with selected job data
   - Call `ui_nav_push_overlay()`
4. Implement "Reprint" button:
   - Call existing `start_print()` with filename
   - Show confirmation/error notification
5. Implement "Delete" button:
   - Show confirmation dialog
   - Call `delete_history_job()` API
   - Remove from list, refresh display

**Success Criteria**:
- [ ] Clicking list row opens detail overlay
- [ ] Overlay shows all metadata fields correctly
- [ ] Status displays with appropriate icon/color
- [ ] "Reprint" starts print job (or shows error if file missing)
- [ ] "Delete" shows confirmation, removes job on confirm
- [ ] Close button returns to list
- [ ] Deleted items disappear from list

**Testing**:
```bash
./build/bin/helix-screen --test -p advanced

# Verify:
# 1. Click any list row - detail overlay appears
# 2. Verify all fields populated correctly
# 3. Click "Reprint" - confirm action initiated
# 4. Click "Delete" - confirmation appears
# 5. Confirm delete - overlay closes, item removed from list
# 6. Close button works
```

**Reference Files**:
- `ui_xml/print_file_detail.xml` - Detail overlay structure
- `ui_xml/confirmation_dialog.xml` - Confirmation pattern
- `src/ui_panel_print_select.cpp` - Reprint/delete implementation

---

### Stage 6: Dashboard Charts

**Goal**: Add sparklines and filament bar chart to dashboard

**Files to Modify**:
- `ui_xml/history_dashboard_panel.xml` (add chart containers)
- `src/ui_panel_history_dashboard.cpp` (implement charts)

**Implementation Steps**:
1. Add sparkline containers next to stat values:
   - Small `lv_chart` (80x24px) with line series
   - Data: last 7 data points for selected time period
2. Add filament bar chart container:
   - `lv_chart` with bar series
   - One bar per filament type (PLA, PETG, ABS, TPU, Other)
   - Label each bar with type name
3. Implement `update_sparklines()`:
   - Calculate trend data from job list
   - Update chart series
4. Implement `update_filament_chart()`:
   - Aggregate filament usage by type
   - Update bar chart series and labels

**Success Criteria**:
- [ ] Sparklines appear next to relevant stats
- [ ] Sparklines show trend over selected time period
- [ ] Bar chart shows filament usage by material type
- [ ] Charts update when time filter changes
- [ ] Charts handle empty data gracefully

**Testing**:
```bash
./build/bin/helix-screen --test -p advanced

# Verify:
# 1. Open Print History dashboard
# 2. Sparklines visible next to stats
# 3. Bar chart shows filament breakdown
# 4. Change time filter - charts update
# 5. Test with no history - charts show empty state
```

**Reference Files**:
- LVGL chart documentation
- Consider creating reusable `sparkline` component

---

### Stage 7: Small Screen Adaptations

**Goal**: Optimize for 480x320 displays

**Files to Modify**:
- `ui_xml/history_dashboard_panel.xml` (add responsive variants)
- `ui_xml/history_list_panel.xml` (compact mode)
- `ui_xml/history_list_row.xml` (compact variant)
- `src/ui_panel_history_dashboard.cpp` (screen size detection)
- `src/ui_panel_history_list.cpp` (compact mode)

**Implementation Steps**:
1. Dashboard adaptations:
   - Reduce to 4 stat cards (most important metrics)
   - Smaller sparklines or hide them
   - Reduce chart size or move to separate view
2. List adaptations:
   - Compact row height (reduce from ~64px to ~48px)
   - Smaller thumbnail (32x32)
   - Hide some metadata columns, show on detail only
3. Use `ui_get_screen_size()` to detect and apply adaptations
4. Ensure touch targets remain ≥44px

**Success Criteria**:
- [ ] Dashboard readable on 480x320
- [ ] List items fit more entries on screen
- [ ] All touch targets ≥44px
- [ ] No horizontal scrolling required
- [ ] Information density appropriate for screen size

**Testing**:
```bash
./build/bin/helix-screen --test -p advanced -s small

# Verify:
# 1. Dashboard fits on screen without scrolling
# 2. Stats readable
# 3. List rows compact but usable
# 4. All buttons/rows tappable
```

---

## Configuration

Add to `config/helixconfig.json.template`:

```json
{
  "print_history": {
    "max_entries": 100,
    "default_time_filter": "all_time"
  }
}
```

---

## Key Patterns Reference

| Pattern | File | Lines |
|---------|------|-------|
| Panel class structure | `include/ui_panel_print_select.h` | Full file |
| Dynamic list population | `src/ui_panel_print_select.cpp` | 761-792 |
| Row click handlers | `src/ui_panel_print_select.cpp` | 974-983 |
| Overlay navigation | `include/ui_nav.h` | `ui_nav_push_overlay()` |
| Action row XML | `ui_xml/setting_action_row.xml` | Full file |
| List row XML | `ui_xml/print_file_list_row.xml` | Full file |
| Detail overlay XML | `ui_xml/print_file_detail.xml` | Full file |
| API callback pattern | `include/moonraker_api.h` | Lines 100-120 |
| Sort implementation | `src/ui_panel_print_select.cpp` | 262-285 |

---

## Utility Functions to Create

```cpp
// In a new file or existing utils
namespace helix::format {

/**
 * @brief Format seconds as human-readable duration
 * @param seconds Duration in seconds
 * @return "2h 15m", "45m", "30s"
 */
std::string duration(double seconds);

/**
 * @brief Format filament length for display
 * @param mm Filament in millimeters
 * @return "12.5m" or "1.2kg" (assuming ~1.24g/cm³ for PLA)
 */
std::string filament(double mm);

/**
 * @brief Format timestamp as local date/time
 * @param timestamp Unix timestamp
 * @return "Dec 1, 14:30"
 */
std::string datetime(double timestamp);

/**
 * @brief Format percentage
 * @param value Decimal value (0.0-1.0)
 * @return "87%"
 */
std::string percentage(double value);

} // namespace helix::format
```

---

## Session Resume Checklist

When resuming work on this feature:

1. Check current stage status in this document
2. Run `./build/bin/helix-screen --test -p advanced` to see current state
3. Review any `// TODO:` comments in implementation files
4. Check git log for recent commits on this feature
5. Run tests: `./build/bin/helix-unit-tests`

---

## Status Tracking

Update this section as stages complete:

| Stage | Status | Notes |
|-------|--------|-------|
| 1. Data Layer | ✅ Complete | Commit 62d6a22 |
| 2. Dashboard Stats | ✅ Complete | Session 2025-12-08 |
| 3. History List | ✅ Complete | Session 2025-12-08 |
| 4. Search/Filter/Sort | ✅ Complete | Session 2025-12-08 |
| 5. Detail Overlay | ✅ Complete | Commit 2d1de9f |
| 6. Charts | ✅ Complete | Session 2025-12-10 (filament bar chart, prints trend sparkline) |
| 7. Small Screen | ⏸ Deferred | Current layout works at 800×480, optimize later if needed |

**Feature Complete:** 2025-12-10
**Merged to main:** 2025-12-10

---

## Session Notes

### Session: 2025-12-08 (Stage 1 Complete)

**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Branch:** `feature/print-history`
**Commit:** `62d6a22`

**Completed:**
- ✅ Created `include/print_history_data.h` with:
  - `PrintHistoryJob` struct with all job metadata
  - `PrintHistoryTotals` struct for aggregate statistics
  - `HistoryTimeFilter` enum (DAY, WEEK, MONTH, YEAR, ALL_TIME)
  - `PrintJobStatus` enum with helper functions

- ✅ Modified `include/moonraker_api.h`:
  - Added `#include "print_history_data.h"`
  - Added `HistoryListCallback` and `HistoryTotalsCallback` types
  - Added `get_history_list()`, `get_history_totals()`, `delete_history_job()`

- ✅ Modified `src/moonraker_api.cpp`:
  - Helper functions: `format_history_duration()`, `format_history_date()`, `format_history_filament()`
  - `parse_history_job()` for JSON parsing
  - Implemented all 3 history API methods

- ✅ Modified `src/moonraker_client_mock.cpp`:
  - 20 mock jobs with realistic data (3DBenchy, calibration_cube, phone_stand, etc.)
  - `server.history.list` handler with pagination and time filtering
  - `server.history.totals` handler
  - `server.history.delete_job` handler

- ✅ Added `--test-history` CLI flag for quick API validation
- ✅ Fixed ccache compiler detection in `scripts/check-deps.sh`
- ✅ Created `tests/unit/test_print_history_api.cpp` (pending test harness fix)

**Validation:**
```bash
./build/bin/helix-screen --test --test-history -v
# Output:
# [History Test] get_history_list SUCCESS: 10 jobs (total: 20)
# [History Test]   Job 1: 3DBenchy.gcode - 1h 35m (Dec 07, 12:30)
# [History Test] get_history_totals SUCCESS:
# [History Test]   Total jobs: 47, Total time: 513000s
```

**Known Issues:**
- Unit test harness has pre-existing linker errors (unrelated to history feature)
- `total_completed/cancelled/failed` fields not populated (by design - Moonraker doesn't provide these in totals endpoint)

---

### Session: 2025-12-08 (Stage 2 Complete)

**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Branch:** `feature/print-history`

**Files Created:**
- `ui_xml/history_dashboard_panel.xml` - Dashboard layout with 6 stat cards and time filters
- `include/ui_panel_history_dashboard.h` - Panel class header
- `src/ui_panel_history_dashboard.cpp` - Panel implementation with API integration

**Files Modified:**
- `ui_xml/advanced_panel.xml` - Added "Print History" action row
- `src/ui_panel_advanced.cpp` - Wired click handler with `show_history_dashboard` handling
- `src/main.cpp` - Registered XML component, added CLI `-p print-history` option, added `set_api()` call

**Key Fixes & Learnings:**

1. **LVGL Subject API (CRITICAL)**
   - ❌ Wrong: `lv_subject_create_int(0)` / `lv_subject_get_by_name("name")`
   - ✅ Correct: Use class member `lv_subject_t subject_` with `lv_subject_init_int(&subject_, 0)` then `lv_xml_register_subject(nullptr, "name", &subject_)`
   - Subject must persist for LVGL binding lifetime (use class member, not local variable)

2. **XML Overlay Panel Pattern (STACK OVERFLOW FIX)**
   - ❌ Wrong: `extends="overlay_panel"` causes infinite XML recursion
   - ✅ Correct: Extend `lv_obj` directly and manually include `header_bar` + `overlay_content` structure
   - Reference: `bed_mesh_panel.xml`, `calibration_zoffset_panel.xml`

3. **Error Callback Signature**
   - ❌ Wrong: `[](const std::string& error) { ... }`
   - ✅ Correct: `[](const MoonrakerError& error) { ... error.message ... }`

4. **PrintJobStatus Enum Usage**
   - ❌ Wrong: `parse_job_status(job.status)` assuming status is string
   - ✅ Correct: `job.status` is already `PrintJobStatus` enum, use directly

5. **Static Callback Visibility**
   - ❌ Wrong: Private static callbacks for LVGL XML event registration
   - ✅ Correct: Static event callbacks must be `public:` for `lv_xml_register_event_cb()`

6. **Deferred API Injection**
   - Panels created before MoonrakerAPI initialization need `set_api()` called later
   - Add to main.cpp after API construction: `get_global_history_dashboard_panel().set_api(moonraker_api.get())`

**Validation:**
```bash
./build/bin/helix-screen --test -p print-history -vv
# Shows: 20 prints, 34h 15m, 242.7m filament, 75% success, 5h longest, 5 failed
```

**Screenshot:** `/tmp/history-dashboard-final.png` - Dashboard with mock data

---

### Session: 2025-12-08 (Stage 3 Complete)

**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Branch:** `feature/print-history`

**Files Created:**
- `ui_xml/history_list_panel.xml` - Full-screen list layout with header_bar and scrollable list container
- `ui_xml/history_list_row.xml` - Row component with filename, date, duration, filament type, status
- `include/ui_panel_history_list.h` - HistoryListPanel class header
- `src/ui_panel_history_list.cpp` - Panel implementation with dynamic row creation

**Files Modified:**
- `src/ui_panel_history_dashboard.cpp` - Wired "View Full History" button, passes cached jobs to list panel
- `src/main.cpp` - Registered XML components, added panel initialization and API injection

**Key Fixes & Learnings:**

1. **PanelBase Lifecycle with Overlays**
   - ❌ Wrong: Assume `ui_nav_push_overlay()` triggers `on_activate()`
   - ✅ Correct: Manually call `panel.on_activate()` after `setup()` for programmatically created overlays
   - `ui_nav_push_overlay()` handles LVGL widget transitions, not PanelBase lifecycle

2. **Back Button Wiring**
   - ❌ Wrong: Assume header_bar back button has built-in navigation
   - ✅ Correct: Call `ui_panel_setup_back_button(panel_)` in `setup()` method
   - Uses `ui_panel_common.h` helper that wires button to `ui_nav_go_back()`

3. **Dynamic Row Creation Pattern**
   ```cpp
   const char* attrs[] = {
       "filename", job.filename.c_str(),
       "date", job.date_str.c_str(),
       // ... more attributes
       NULL  // Sentinel required!
   };
   lv_obj_t* row = static_cast<lv_obj_t*>(
       lv_xml_create(list_rows_, "history_list_row", attrs));
   ```

4. **Job Data Passing Between Panels**
   - Dashboard caches jobs from API response
   - List panel receives jobs via `set_jobs()` before `on_activate()`
   - Avoids redundant API calls when navigating

**Validation:**
```bash
./build/bin/helix-screen --test -p print-history -vv
# Navigate: Dashboard → View Full History → List with 20 rows
# Back button returns to dashboard
# All 59 unit test assertions pass
```

---

### Session: 2025-12-08 (Stage 4 Complete)

**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Branch:** `feature/print-history`

**Files Modified:**
- `ui_xml/history_list_panel.xml` - Added filter row with search, status dropdown, sort dropdown
- `ui_xml/history_dashboard_panel.xml` - UI polish: labels above values, icon fixes, button styling
- `ui_xml/advanced_panel.xml` - Fixed icon from `history` to `clock`
- `include/ui_panel_history_list.h` - Added filter/sort enums and methods
- `src/ui_panel_history_list.cpp` - Implemented filtering and sorting logic

**Key Implementation Details:**

1. **Filter/Search Row Layout**
   ```xml
   <lv_obj name="filter_row" width="100%" height="content" flex_flow="row" ...>
     <lv_textarea name="search_box" placeholder_text="Search filename..." one_line="true" flex_grow="1">
       <event_cb trigger="value_changed" callback="history_search_changed"/>
     </lv_textarea>
     <lv_dropdown name="filter_status" options="All\nCompleted\nFailed\nCancelled">
       <event_cb trigger="value_changed" callback="history_filter_status_changed"/>
     </lv_dropdown>
     <lv_dropdown name="sort_dropdown" options="Date (newest)\nDate (oldest)\nDuration\nFilename">
       <event_cb trigger="value_changed" callback="history_sort_changed"/>
     </lv_dropdown>
   </lv_obj>
   ```

2. **Filter Chain Pattern**
   - Source jobs → search filter → status filter → sort → display
   - `apply_filters_and_sort()` orchestrates the chain
   - Results stored in `filtered_jobs_` vector

3. **Debounced Search**
   - 300ms debounce using `lv_timer_create()`
   - Case-insensitive filename substring match

4. **Enums for Filter/Sort State**
   ```cpp
   enum class HistorySortColumn { DATE, DURATION, FILENAME };
   enum class HistorySortDirection { ASCENDING, DESCENDING };
   enum class HistoryStatusFilter { ALL, COMPLETED, FAILED, CANCELLED };
   ```

**UI Polish Fixes:**
- Dashboard stat card labels moved ABOVE values (was below)
- Fixed missing icons: `history`→`clock`, `format_list_bulleted`→`list`
- Filter buttons: removed flex layout (lv_button centers children by default)
- Filter buttons: removed explicit height, use `style_pad_all="#space_sm"` for natural sizing

**Validation:**
```bash
./build/bin/helix-screen --test -p print-history -vv
# Navigate: Dashboard → View Full History
# Test: Search box filters by filename
# Test: Status dropdown filters by status
# Test: Sort dropdown reorders list
# Test: Combined filters work correctly
```

---

### Session: 2025-12-08 (Stage 5 Complete)

**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Branch:** `feature/print-history`
**Commit:** `2d1de9f`

**Files Created:**
- `ui_xml/history_detail_overlay.xml` - Print detail overlay layout

**Files Modified:**
- `src/ui_panel_history_list.cpp` - Row click handlers, detail overlay management
- `include/ui_panel_history_list.h` - Detail overlay subjects and methods

**Implementation:**
- Detail overlay shows full job metadata (filename, status, times, temps, filament)
- Status displayed with colored icon (green check for completed, red X for failed)
- Reprint button starts print if file exists
- Delete button removes job from history with confirmation

---

### Session: 2025-12-10 (Stage 6 Complete + UI Polish)

**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-print-history`
**Branch:** `feature/print-history`

**Stage 6 - Dashboard Charts Completed:**

**Layout Redesign:**
- Stats grid (2×2) on left side (55% width)
- Filament by Type bar chart on right side (43% width)
- Prints Trend sparkline below (full width)
- Time filter buttons use equal width via `flex_grow="1"`

**Filament Chart Implementation:**
- Horizontal bar chart showing usage by material type (PLA, PETG, ABS, TPU)
- Material labels on left, usage amounts (meters) on right
- Color-coded bars per material type
- Dynamic sizing based on data

**Prints Trend Sparkline:**
- Gold/amber colored line chart showing print count trend
- Auto-adapts period label based on selected filter (Day/Week/Month/Year/All time)

**UI Polish - Reactive Style Bindings:**

Replaced imperative C++ filter button styling with declarative XML bindings:

```xml
<styles>
  <style name="filter_btn_active" bg_color="0xAD2724"/>
  <style name="filter_label_active" text_color="0xE6E8F0"/>
</styles>

<lv_button name="filter_day" ...>
  <bind_style name="filter_btn_active" subject="history_filter" ref_value="0"/>
  <text_small name="filter_day_label" ...>
    <bind_style name="filter_label_active" subject="history_filter" ref_value="0"/>
  </text_small>
</lv_button>
```

**Key Fix - Removed `update_filter_button_states()`:**
- Previous: C++ function manually applied styles based on filter state
- Now: XML `<bind_style>` elements auto-update when subject changes
- Benefit: Cleaner separation of concerns, less C++ code

**Typography Fix:**
- Replaced hardcoded `style_text_font="montserrat_14"` with `<text_small>` theme component
- Ensures consistent typography across the app

**Text Color Fix:**
- Fixed filament chart labels showing as black
- Root cause: `ui_theme_parse_color("#text_primary")` doesn't resolve XML constants
- Solution: Use `lv_xml_get_const(nullptr, "text_primary")` first, then parse result

**Files Modified:**
- `ui_xml/history_dashboard_panel.xml` - Reactive bindings, theme typography
- `src/ui_panel_history_dashboard.cpp` - Removed `update_filter_button_states()`, fixed color resolution
- `include/ui_panel_history_dashboard.h` - Removed function declaration
- `src/ui_panel_history_list.cpp` - Empty state visibility fix
- `ui_xml/history_list_panel.xml` - Empty state flex_grow fix

---

### Next Session: Stage 7 - Small Screen Adaptations

**Goal:** Optimize for 480×320 displays

**Quick Start:**
```bash
cd /Users/pbrown/Code/Printing/helixscreen-print-history
make -j && ./build/bin/helix-screen --test -p print-history -s small -vv
```

**Implementation Notes:**
1. Dashboard adaptations:
   - Reduce to 4 stat cards (most important metrics)
   - Smaller sparklines or hide them
   - Reduce chart size or move to separate view
2. List adaptations:
   - Compact row height (reduce from ~64px to ~48px)
   - Smaller thumbnail (32×32)
   - Hide some metadata columns, show on detail only
3. Use `ui_get_screen_size()` to detect and apply adaptations
4. Ensure touch targets remain ≥44px
