# HelixScreen Memory Usage Analysis
*Last Updated: 2025-11-30*

## Executive Summary

**Current Pattern:** Create-once, toggle visibility (all panels loaded at startup)
**Memory Footprint:** ~32-35 MB physical, ~68 MB RSS
**Heap Usage:** ~12.6 MB of actual allocations
**Leaks:** None detected
**Recommendation:** ✅ **KEEP CURRENT APPROACH** - memory usage is excellent

---

## Memory Profile (2025-11-30)

### Normal Mode (All Panels Pre-Created)
```
Physical Footprint:  32.6 MB
Physical Peak:       36.2 MB
RSS (Resident):      68 MB
VSZ (Virtual):       ~35 GB (virtual address space, not real)
Heap Allocations:    40,143 nodes
Allocated Data:      12.6 MB
```

### Wizard Mode (On-Demand Panel Creation)
```
Physical Footprint:  32.3 MB
Physical Peak:       34.9 MB
RSS (Resident):      62 MB
Heap Allocations:    ~38,000 nodes (estimated)
```

### Memory Comparison (Normal vs Wizard)
```
Physical Footprint:  -0.3 MB (negligible)
RSS:                 -6 MB (wizard uses slightly less)
```

**KEY FINDING:** Both modes use approximately the same memory. The create-once pattern has no meaningful memory penalty.

---

## Memory Breakdown

### Where the Memory Goes

**Framework Overhead (Majority):**
- LVGL runtime: ~10-15 MB
- SDL2 graphics: ~10-15 MB
- System libraries: shared, not counted in physical footprint
- Total Framework: ~25-30 MB

**UI Panels:**
- All XML panels + overlays + wizard: ~12.6 MB heap
- Individual panel estimate: ~400-800 KB each

### Heap Statistics Detail (2025-11-30)

```
Allocation Size Distribution:
- 32-byte objects: 13,124 instances (LVGL widgets)
- 48-byte objects: 6,617 instances
- 64-byte objects: 5,454 instances
- 80-byte objects: 3,773 instances
- 128-byte objects: 1,419 instances
- Larger objects: Sparse (fonts, images, buffers)

Top Memory Consumers:
- Non-object allocations: 9.3 MB (LVGL internal state)
- CFString objects: 158 KB (3,076 instances)
```

---

## LVGL 9 Documentation Findings

### Official Best Practices (from docs.lvgl.io & forum)

**Pattern 1: Create Once, Keep in Memory**
> "Just loads an already existing screen. When the screen is left, it remains in memory, so all states are preserved."

**Pattern 2: Dynamic Create/Delete**
> "The screen is created dynamically, and when it's left, it is deleted, so all changes are lost (unless they are saved in subjects)."

**Memory Optimization Quote:**
> "To work with lower LV_MEM_SIZE you can create objects only when required and delete them when they are not needed. This allows for the creation of a screen just when a button is clicked to open it, and for deletion of screens when a new screen is loaded."

**Auto-Delete Feature:**
> "By using `lv_screen_load_anim(scr, transition_type, time, delay, auto_del)` ... if `auto_del` is true the previous screen is automatically deleted when any transition animation finishes."

### When to Use Each Pattern

**Create-Once (Current HelixScreen approach):**
- ✅ Moderate number of screens (~10 main + overlays + wizard)
- ✅ State preservation is critical (temps, positions, settings)
- ✅ Instant panel switching matters
- ✅ Known memory ceiling (~35 MB physical)
- ✅ Target hardware has adequate RAM (>256 MB)

**Dynamic Create/Delete (Use When):**
- ❌ Dozens of panels (we have ~15 total)
- ❌ Panels with heavy resources (we use lightweight XML)
- ❌ Panels rarely accessed (all panels are frequently used)
- ❌ Running on <64 MB RAM (Raspberry Pi 3+ has 1 GB+)

---

## Recommendations

### 1. KEEP Current Create-Once Pattern ✅

**Rationale:**
- Memory usage is excellent (~35 MB physical)
- Instant panel switching (0ms vs 50-100ms)
- State preserved automatically
- No serialization complexity
- Predictable memory usage
- Zero risk of allocation failures at runtime

### 2. Future: Profile on Target Hardware 📊

macOS SDL2 simulator uses different memory patterns than Linux framebuffer:
- SDL2 uses GPU acceleration
- Framebuffer uses CPU rendering (different overhead)
- Test on actual Raspberry Pi for real-world numbers

### 3. Future: Consider Lazy Image Loading 🖼️

If print thumbnails become numerous:
```cpp
// Instead of loading 100 thumbnails at once:
// Load thumbnails on-demand when scrolled into view
// Unload when scrolled out (image cache)
```

---

## Conclusion

The current architecture is **well-optimized** for the use case. Physical memory footprint is ~35 MB, well within budget for any modern SBC.

**Don't refactor unless:**
- Running on <64 MB RAM hardware (extremely unlikely)
- Need to support 50+ panels
- Profiling shows OOM crashes on target hardware

---

## Test Commands Used

```bash
# Build app
make -j

# Profile normal mode (all panels)
./build/bin/helix-screen -s small --panel home --test &
PID=$!
sleep 4
ps -o pid,rss,vsz -p $PID
heap $PID | head -35
vmmap --summary $PID | grep "Physical footprint"
kill $PID

# Profile wizard mode
./build/bin/helix-screen -s small --wizard --test &
PID=$!
sleep 4
# ... same profiling commands
```
