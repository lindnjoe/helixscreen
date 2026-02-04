# Display Resolution Auto-Detection

## Summary

Add automatic display resolution detection for embedded backends (fbdev/DRM) with CLI override support for arbitrary resolutions.

## Goals

1. Auto-detect display resolution from hardware on fbdev/DRM backends
2. Support arbitrary `WxH` CLI override (e.g., `-s 480x400`)
3. Add `tiny_alt` preset for 480×400 displays (Creality K1 series)
4. SDL always uses presets/CLI (no auto-detect)

## Resolution Priority

1. **CLI override** (`-s 480x400` or `-s tiny_alt`) - highest priority
2. **Auto-detect** from hardware (fbdev/DRM only)
3. **Default** 800×480 (fallback)

## API Design

### New struct in `display_backend.h`

```cpp
struct DetectedResolution {
    int width = 0;
    int height = 0;
    bool valid = false;
};
```

### New virtual method

```cpp
virtual DetectedResolution detect_resolution() const {
    return {}; // Default: not supported
}
```

## Implementation Details

### Fbdev Backend

Query `FBIOGET_VSCREENINFO` for `vinfo.xres` / `vinfo.yres`:

```cpp
DetectedResolution DisplayBackendFbdev::detect_resolution() const {
    int fd = open("/dev/fb0", O_RDONLY);
    if (fd < 0) return {};

    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        close(fd);
        return {};
    }
    close(fd);

    return {static_cast<int>(vinfo.xres), static_cast<int>(vinfo.yres), true};
}
```

### DRM Backend

Query connector's preferred mode (mirror LVGL's DRM driver logic).

### SDL Backend

Returns `{0, 0, false}` - always uses presets/CLI.

### DisplayManager Init Flow

```cpp
// In DisplayManager::init()
if (config.width == 0 && config.height == 0) {
    // No CLI override - try auto-detect (non-SDL only)
    if (m_backend->type() != DisplayBackendType::SDL) {
        auto detected = m_backend->detect_resolution();
        if (detected.valid) {
            m_width = detected.width;
            m_height = detected.height;
            spdlog::info("[DisplayManager] Auto-detected resolution: {}x{}", m_width, m_height);
        } else {
            m_width = 800;
            m_height = 480;
            spdlog::warn("[DisplayManager] Could not detect resolution, using default 800x480");
        }
    } else {
        // SDL always needs explicit size
        m_width = 800;
        m_height = 480;
    }
}
```

### CLI Parsing

Extended `-s/--size` accepts:

| Input | Result |
|-------|--------|
| `tiny` | 480×320 |
| `tiny_alt` | 480×400 |
| `small` | 800×480 |
| `medium` | 1024×600 |
| `large` | 1280×720 |
| `WxH` | Arbitrary (e.g., `1920x1080`) |

Parsing order:
1. Try preset names (case-insensitive)
2. Try `WxH` format (regex: `^\d+x\d+$`)
3. Error with helpful message

### Breakpoint Mapping

No changes needed - breakpoints computed from `max(width, height)`:
- 480×400 → max=480 → TINY breakpoint (uses TINY nav, fonts, spacing)

## Files to Modify

| File | Changes |
|------|---------|
| `include/display_backend.h` | Add `DetectedResolution` struct + virtual method |
| `src/api/display_backend_fbdev.cpp` | Implement `detect_resolution()` |
| `src/api/display_backend_drm.cpp` | Implement `detect_resolution()` |
| `include/theme_manager.h` | Add `UI_SCREEN_TINY_ALT_W/H` constants |
| `src/system/cli_args.cpp` | Parse `tiny_alt` + arbitrary `WxH` |
| `src/application/display_manager.cpp` | Auto-detect flow |
| `include/display_manager.h` | Update `Config` if needed |

## Testing

- SDL: Verify presets still work, arbitrary `WxH` works
- Fbdev/DRM: Test on actual hardware (AD5M, Pi, K1 if available)
- Verify breakpoint selection for various resolutions
