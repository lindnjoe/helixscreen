# Log Standardization Handoff

**Created**: 2025-12-13
**Purpose**: Continue logging standardization work in a fresh session

---

## Context

We're standardizing all `spdlog` calls across the HelixScreen codebase to follow consistent patterns. This improves log readability at all verbosity levels (`-v`, `-vv`, `-vvv`).

## What's Been Done

### Phase 1-3: Log Level Reorganization ✅
- **DEBUG → TRACE**: Per-item loop noise (theme colors, wire protocol, observer registration)
- **INFO → DEBUG**: Internal details (backend selection, subject initialization)
- **DEBUG → INFO**: Important milestones (subscription complete, XML UI created)
- Files: 23 modified, documented in `docs/LOG_LEVEL_REFACTOR_PLAN.md`

### Phase 4: Prefix Standardization (In Progress)
- Fixed 44 logs across 5 files
- **Remaining: ~391 logs** need `[ComponentName]` prefix

## What Remains

### Priority Files (by non-standard count)
```
main.cpp              76 logs
ui_gcode_viewer.cpp   40 logs
gcode_parser.cpp      37 logs
gcode_tinygl_renderer 35 logs
display_backend_drm   27 logs
bed_mesh_renderer     21 logs
display_backend_fbdev 13 logs
ui_notification.cpp   16 logs
app_globals.cpp       16 logs
```

### Standard Format Required

All logs MUST follow: `spdlog::level("[ComponentName] Message", args...)`

**Examples:**
```cpp
// ✅ Correct
spdlog::info("[PrinterState] Subjects initialized");
spdlog::debug("[DisplayBackend] Auto-detected: SDL");
spdlog::trace("[Theme] Registering color {}: {}", name, value);

// ❌ Wrong - no brackets
spdlog::info("PrinterState: Subjects initialized");
spdlog::debug("Auto-detected display backend: SDL");

// ❌ Wrong - colon instead of brackets
spdlog::info("AmsBackendMock: Started");
```

**Component naming:**
- PascalCase for single words: `[PrinterState]`, `[Theme]`
- Space-separated for multi-word: `[AMS State]`, `[Moonraker Client]`
- Match existing class/module name when possible

## Commands

### Find remaining non-standard prefixes
```bash
grep -rn 'spdlog::\(debug\|info\|trace\|warn\|error\)("[A-Za-z]' src/ | grep -v '\[' | wc -l
```

### Find by file (sorted by count)
```bash
grep -rn 'spdlog::\(debug\|info\|trace\|warn\|error\)("[A-Za-z]' src/ | grep -v '\[' | cut -d: -f1 | sort | uniq -c | sort -rn | head -20
```

### Test after changes
```bash
make -j && ./build/bin/helix-screen --test -vv 2>&1 | head -60
```

## Reference Docs

- `docs/LOGGING.md` - Full logging guidelines (levels, patterns, examples)
- `docs/LOG_LEVEL_REFACTOR_PLAN.md` - Detailed tracking of all phases

## Suggested Continuation Prompt

Copy this to start the next session:

---

**Continue logging standardization work from `docs/LOG_STANDARDIZATION_HANDOFF.md`.**

The goal is to add `[ComponentName]` prefixes to all ~391 remaining spdlog calls that don't have them.

Priority order:
1. `main.cpp` (76 logs) - highest impact on startup output
2. `gcode_*.cpp` files (~100 logs combined)
3. `display_backend_*.cpp` files (~40 logs combined)
4. Remaining files

Use `replace_all` when a file has a consistent pattern like `"ComponentName:"` that can become `"[ComponentName]"`.

After each batch, run `make -j` to verify compilation, then commit with message format:
```
refactor(logging): standardize prefixes in [files changed]
```

---
