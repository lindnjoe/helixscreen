# AD5M Boot Process Notes

## ForgeX Firmware Configuration

### Display Mode (variables.cfg)
- **GUPPY mode** is what we want for HelixScreen (NOT STOCK!)
- Location: `/opt/config/mod_data/variables.cfg`
- Setting: `display = 'GUPPY'`
- STOCK mode expects ffstartup-arm to handle display/backlight - doesn't work for us
- GUPPY mode handles backlight properly via ForgeX scripts
- The `reset_screen` delayed_gcode just sets backlight level, doesn't draw to FB

### Disable GuppyScreen
- Remove execute permission from init scripts:
  - `/opt/config/mod/.root/S80guppyscreen`
  - `/opt/config/mod/.root/S35tslib`
- Command: `chmod -x /opt/config/mod/.root/S80guppyscreen /opt/config/mod/.root/S35tslib`

### Disable Stock FlashForge UI
- Comment out in `/opt/auto_run.sh`:
  - `/opt/PROGRAM/ffstartup-arm -f /opt/PROGRAM/ffstartup.cfg &`

## Backlight Control

### The Bug We Fixed (2024-01-24)
The `enable_backlight()` function had a path bug:
- `FORGEX_BACKLIGHT="/root/printer_data/py/backlight.py"` - path INSIDE chroot
- `FORGEX_CHROOT="/data/.mod/.forge-x"` - chroot location
- **BUG**: Checked `[ -x "$FORGEX_BACKLIGHT" ]` which tested `/root/printer_data/py/backlight.py` on HOST
- **FIX**: Check `[ -x "${FORGEX_CHROOT}${FORGEX_BACKLIGHT}" ]` for full path

### How Backlight Works on AD5M
- Uses custom ioctl to `/dev/disp` (not sysfs)
- ForgeX provides `backlight.py` script inside chroot
- Must run via chroot: `/usr/sbin/chroot /data/.mod/.forge-x /root/printer_data/py/backlight.py 100`
- In STOCK mode, ForgeX turns OFF backlight - we must turn it ON

### Boot Sequence (STOCK mode)
1. ForgeX S99root runs, shows "Booting stock firmware..."
2. ForgeX turns OFF backlight (in STOCK mode)
3. Our init script in `/etc/init.d/S99helixscreen` starts
4. We call `enable_backlight()` to turn ON backlight
5. We start splash, then helix-screen

## Debugging

### Enable Debug Logging
Add to init script (after LOGFILE line):
```bash
export HELIX_DEBUG=1
```

### Key Log File
- `/tmp/helixscreen.log`

### Manual Backlight Test
```bash
/usr/sbin/chroot /data/.mod/.forge-x /root/printer_data/py/backlight.py 100
```

### Framebuffer Info
- Device: `/dev/fb0`
- Resolution: 800x480
- Bits per pixel: 32
- Stride: 3200 bytes

## Init Script Locations

**IMPORTANT**: There are TWO copies of the init script:
1. `/etc/init.d/S90helixscreen` - **THIS IS WHAT RUNS AT BOOT**
2. `/opt/helixscreen/config/helixscreen.init` - source copy

When deploying fixes, must update `/etc/init.d/S90helixscreen`!

## SSH/SCP Notes

- AD5M BusyBox doesn't have sftp-server
- Must use legacy SCP protocol: `scp -O` flag
- Example: `scp -O localfile root@192.168.1.67:/path/`
- AD5M IP: `192.168.1.67` (use `AD5M_HOST=192.168.1.67` with make commands)
- mDNS (ad5m.local) may not resolve - use IP directly

## Boot Sequence (Detailed)

```
S00init  - Writes ForgeX splash to fb0, mounts /dev into chroot
S55boot  - Calls boot.sh, prints "Booting stock firmware..." via `logged --send-to-screen`
S90helixscreen - OUR SCRIPT: enables backlight, starts HelixScreen
S98camera, S98zssh
S99root  - Starts chroot services (Moonraker, etc.)
```

## Current Bug (2024-01-24): Backlight Not Turning On

**Symptom**: After "Booting stock firmware..." message, backlight turns off and stays off.

**What we've confirmed**:
- Our init script (S90helixscreen) DOES run
- `enable_backlight()` IS called
- The chroot backlight command DOES execute (returns 0)
- But backlight stays OFF

**Theory**: The backlight.py ioctl command is ACCEPTED (returns 0) but IGNORED
during cold boot. Even 60 seconds of repeated attempts doesn't help.
Manual command works AFTER boot is fully complete.

**NOT a timing issue** - tried repeated attempts over 60 seconds, none worked.

**Possible causes**:
1. Display driver not fully initialized during init scripts
2. ForgeX chroot environment missing something during boot
3. Hardware state requires stock firmware to initialize first
4. ioctl is queued but not executed until something else happens

**Key discovery**: Backlight IS on when "Booting stock firmware..." is shown,
then turns OFF. Stock firmware (ffstartup-arm) normally keeps display alive.
We disabled it, so nothing maintains the backlight.

**WORKING SOLUTION**:
- Use GUPPY mode (`display = 'GUPPY'` in variables.cfg)
- Disable GuppyScreen init scripts (chmod -x S80guppyscreen and S35tslib)
- Disable stock firmware in auto_run.sh
- ForgeX handles backlight in GUPPY mode
- helixscreen_active flag prevents S99root from drawing to screen

**Files involved**:
- `/etc/init.d/S90helixscreen` - our init script (THE ONE THAT RUNS)
- `/opt/helixscreen/config/helixscreen.init` - source copy (NOT used at boot!)
- `/opt/config/mod/.shell/S55boot` - prints "Booting stock firmware..."
- `/opt/config/mod/.bin/logged` - binary that draws text to screen

**Debug log location**: `/tmp/helix_boot.log`

## GuppyScreen Approach (2026-01-24)

GuppyScreen (also LVGL-based) works perfectly with ForgeX. Key insight: it uses
**standard Linux framebuffer ioctls** to unblank the display, not the custom
backlight.py script.

### fbdev_unblank() from GuppyScreen

Located in `lv_drivers/display/fbdev.c` (via patch):

```c
void fbdev_unblank(void) {
    // 1. Unblank via standard ioctl
    ioctl(fbfd, FBIOBLANK, FB_BLANK_UNBLANK);

    // 2. Get screen info and reset pan position
    struct fb_var_screeninfo var_info;
    ioctl(fbfd, FBIOGET_VSCREENINFO, &var_info);
    var_info.yoffset = 0;
    ioctl(fbfd, FBIOPAN_DISPLAY, &var_info);
}
```

### Implemented in HelixScreen

Added `unblank_display()` and `blank_display()` methods to `DisplayBackendFbdev`:

**Unblank (startup + wake from sleep):**
- Called early in startup (both splash and main app) before `create_display()`
- Called when waking from display sleep timeout
- Uses FBIOBLANK with FB_BLANK_UNBLANK + FBIOPAN_DISPLAY to reset pan

**Blank (sleep timeout):**
- Called when display goes to sleep (brightness 0)
- Uses FBIOBLANK with FB_BLANK_NORMAL

**Files modified:**
- `include/display_backend.h` - virtual methods in base class
- `include/display_backend_fbdev.h` - override declarations
- `src/api/display_backend_fbdev.cpp` - implementation
- `src/application/display_manager.cpp` - calls on startup, sleep, wake
- `src/helix_splash.cpp` - calls on startup

This should eliminate the need for shell script backlight hacks and work
regardless of ForgeX display mode setting.

## Screen Dimming Fix (2026-01-25)

### The Problem
After HelixScreen runs for ~3 seconds, the screen dims to 10% brightness.

**Root cause**: ForgeX's `headless.cfg` has a `reset_screen` delayed_gcode that runs
3 seconds after Klipper starts:

```ini
[delayed_gcode reset_screen]
initial_duration: 3
gcode:
    RUN_SHELL_COMMAND CMD=screen PARAMS='draw_splash'
    _BACKLIGHT S={printer.mod_params.variables.backlight_eco}
```

The `_BACKLIGHT` macro calls `screen.sh backlight {value}`, which sets backlight
to `backlight_eco` (typically 10%).

**Location**: `/data/.mod/.forge-x/root/printer_data/config/mod/macros/headless.cfg`

### The Fix
Patch `/opt/config/mod/.shell/screen.sh` to skip backlight commands when
HelixScreen is active (indicated by `/tmp/helixscreen_active` flag file).

**Patch applied to screen.sh** (in `backlight)` case):
```bash
backlight)
    # Skip if HelixScreen is controlling the display
    if [ -f /tmp/helixscreen_active ]; then
        exit 0
    fi
    value=$2
    ...
```

**Installer handles this automatically**:
- `install.sh` patches screen.sh during ForgeX installation
- `install.sh --uninstall` removes the patch

### Flag File
- `/tmp/helixscreen_active` - created by init script on start, removed on stop
- Tells ForgeX scripts to leave display alone while HelixScreen is running

## Common Issues

1. **Garbled display**: Usually wrong FB format or something drawing over it
2. **Black screen**: Backlight not enabled
3. **ForgeX messages over UI**: S99root's `logged --send-to-screen` output after our init
4. **Screen dims after ~3 seconds**: ForgeX delayed_gcode - see "Screen Dimming Fix" above

## Build Notes

### Cross-Compilation (Docker)
- Uses Colima on macOS (lightweight Docker alternative)
- `make ad5m-docker` - builds using Docker container with ARM toolchain
- `SKIP_OPTIONAL_DEPS=1` passes `--minimal` to check-deps.sh (skips npm/clang-format)
- GCC 10 in Docker requires explicit type for `std::max({...})` - use `std::initializer_list<T>`

### Deployment
- `make deploy-ad5m` - deploys binaries and assets to AD5M
- Uses `scp -O` for legacy SCP protocol (BusyBox has no sftp-server)
- Init script auto-updated if different from deployed version
