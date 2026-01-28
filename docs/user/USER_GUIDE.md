# HelixScreen User Guide

Learn how to use HelixScreen to control your 3D printer.

---

## Table of Contents

- [Navigation Overview](#navigation-overview)
- [Home Panel](#home-panel)
- [Starting a Print](#starting-a-print)
- [During a Print](#during-a-print)
- [Controls Panel](#controls-panel)
- [AMS Panel](#ams-panel)
- [Settings](#settings)
- [Advanced Features](#advanced-features)
- [Keyboard & Input](#keyboard--input)
- [Tips & Tricks](#tips--tricks)

---

## Navigation Overview

HelixScreen uses a consistent navigation pattern with a **left sidebar** containing five navigation buttons:

| Icon | Panel | Purpose |
|------|-------|---------|
| Home | Home | Printer status at a glance |
| Tune | Controls | Motion, temperature, fan controls |
| Spool | Filament | Load, unload, filament profiles |
| Gear | Settings | Display, sound, network options |
| Dots | Advanced | Calibration, history, system tools |

> **Note:** Print Select is accessed from the **Home panel** (tap the print area), not from the navbar. Controls and Filament require a printer connection and appear disabled when disconnected.

### Navigation Tips

- **Tap** an icon to switch panels
- **Current panel** is highlighted
- **Back button** (arrow) returns from sub-panels
- **Swipe** to scroll long lists

### Connection Status

The home panel shows connection status:
- **Green checkmark:** Connected to printer
- **Red X:** Disconnected (shows reconnection toast)
- **Yellow exclamation:** Klipper not ready (firmware restart needed)

---

## Home Panel

![Home Panel](../images/user/home.png)

The Home Panel is your dashboard for printer status.

### Status Area

At the top, you'll see:
- **Printer state:** Idle, Printing, Paused, etc.
- **Print progress:** Percentage and time remaining (when printing)
- **Connection status:** Connected/Disconnected indicator

### Temperature Displays

Real-time temperature readouts:
- **Nozzle temperature:** Current / Target
- **Bed temperature:** Current / Target
- **Chamber temperature:** (if equipped)

**Tap** a temperature display to jump to its control panel.

### AMS/Filament Status

If you have a multi-material system (Happy Hare, AFC-Klipper):
- Visual display of loaded filament slots
- Current active slot highlighted
- Color-coded by filament type

### Quick Actions

Buttons for common operations:
- **LED Toggle:** Turn chamber lights on/off
- **Emergency Stop:** Halt all motion (confirmation required)

---

## Starting a Print

![Print Select Panel](../images/user/print-select.png)

### Step 1: Browse Files

1. From the **Home panel**, tap the print file area to open Print Select
2. Browse your G-code files

### Step 2: View Options

**Card View** (default): Shows thumbnails with file info
- File name
- Estimated print time
- Filament usage
- Slicer (OrcaSlicer, PrusaSlicer, etc.)

**List View**: Compact view for many files
- Toggle with the view button in top-right

### Step 3: Sort & Filter

Tap the sort button to organize files by:
- **Name** (A-Z or Z-A)
- **Modified date** (newest or oldest)
- **Size** (largest or smallest)
- **Print time** (longest or shortest)

### Step 4: Select a File

Tap a file to open the **preview panel**:
- **3D G-code preview** (rotatable with touch)
- **Layer slider** for inspection
- **File metadata** (size, time, filament)
- **Pre-print options**

### Step 5: Pre-Print Options

Before starting, you can enable/disable:
- **Auto bed level:** Run mesh before print
- **Quad gantry level:** QGL calibration
- **Z-tilt adjust:** Z-tilt calibration
- **Nozzle clean:** Run cleaning macro

These options modify the G-code on-the-fly, commenting out embedded operations if you disable them.

### Step 6: Start Print

Tap **Start Print** to begin. The UI switches to the Print Status panel.

---

## During a Print

The Print Status panel shows live print progress and provides tools to adjust print parameters on the fly.

### Progress Display

- **Circular progress indicator** with percentage
- **Time elapsed** and **time remaining**
- **Current layer** / total layers
- **Filename** being printed

### Controls During Print

**Pause/Resume:** Temporarily halt the print
- Printer parks the nozzle safely
- Tap again to resume

**Cancel:** Stop the print (confirmation required)
- Prompts "Are you sure?" to prevent accidents

### Exclude Object

If your slicer supports it (OrcaSlicer, SuperSlicer):
- Tap **Exclude Object** to see printable objects
- Select an object to skip (e.g., failed part)
- **Undo** available for 5 seconds after exclusion

### Temperature Monitoring

Live temperature graphs show:
- Nozzle and bed temperatures
- Target lines for reference
- History over time

### Print Tune Overlay

The Print Tune overlay lets you make real-time adjustments to your print without stopping it. Access it by tapping the **Tune** button on the Print Status panel.

**Available Adjustments:**

| Parameter | Range | What It Does |
|-----------|-------|--------------|
| Speed % | 10-300% | Overall print speed multiplier |
| Flow % | 75-125% | Extrusion rate multiplier |
| Fan % | 0-100% | Part cooling fan speed |

**How to use:**
1. Tap the **Tune** button during an active print
2. Adjust sliders or tap the percentage to enter an exact value
3. Changes apply immediately to the running print
4. Tap outside the overlay or the **X** button to close

**When to use each adjustment:**

- **Speed %:** Slow down for intricate details or speed up for infill. Lower to 50-70% if you see layer separation or poor adhesion on overhangs.
- **Flow %:** Increase (105-110%) if you see gaps between extrusion lines (under-extrusion). Decrease (95-98%) if you see blobs or over-packed lines (over-extrusion).
- **Fan %:** Increase for better bridging and overhang quality. Decrease or disable for ABS/ASA to prevent warping and layer separation.

**Note:** All Tune adjustments are temporary and only affect the current print. The next print starts with your slicer's original values.

### Z-Offset / Baby Steps

Fine-tune your first layer height while printing to achieve the perfect squish. This is especially useful during the first few layers when you can see if the nozzle is too close or too far from the bed.

**How to access:**
1. During a print, locate the **Z-Offset** section on the Print Status panel
2. Use the adjustment buttons to raise or lower the nozzle

**Adjustment increments:**
- **-0.05mm:** Larger step, nozzle closer to bed (more squish)
- **-0.01mm:** Fine step, nozzle slightly closer
- **+0.01mm:** Fine step, nozzle slightly higher
- **+0.05mm:** Larger step, nozzle further from bed (less squish)

**Signs you need to adjust:**

| Symptom | Problem | Solution |
|---------|---------|----------|
| Lines not sticking, curling up | Nozzle too high | Tap **-0.01** or **-0.05** |
| Rough first layer, scratching sounds | Nozzle too low | Tap **+0.01** or **+0.05** |
| Filament squished flat, see-through gaps | Nozzle too high | Tap **-0.01** to close gaps |
| Ridges, elephant foot on first layer | Nozzle too low | Tap **+0.01** or **+0.05** |

**Saving your Z-Offset:**

By default, Z-offset changes are temporary and reset after the print completes. To make your adjustment permanent:

1. Get the first layer looking good with baby step adjustments
2. Tap the **Save Z-Offset** button
3. The new offset is written to your Klipper configuration
4. Future prints will use this as the starting point

**Tip:** Make small adjustments (0.01mm) and wait for the printer to complete a few extrusion moves before judging the result. Changes take effect on the next movement command.

### Pressure Advance Tuning

If your printer has Pressure Advance configured in Klipper, you can fine-tune the PA value during a print. This is accessed through the **Tune** overlay.

**How to access:**
1. Tap the **Tune** button during a print
2. Scroll down to find the **Pressure Advance** section (only visible if PA is enabled in your Klipper config)
3. Adjust the value using the slider or tap to enter an exact number

**What Pressure Advance does:**

Pressure Advance compensates for the delay between extruder movement and actual filament flow. Higher values reduce corner bulging and improve sharp corners, but too high causes gaps at the start of lines.

**Typical values by material:**
- **PLA:** 0.02 - 0.06
- **PETG:** 0.04 - 0.10
- **ABS/ASA:** 0.03 - 0.08
- **TPU:** 0.10 - 0.20 (or disabled)

**When to adjust during a print:**

- **Bulging corners, rounded edges:** Increase PA by 0.01-0.02
- **Gaps at line starts, weak corners:** Decrease PA by 0.01-0.02
- **Switching filament brands:** Fine-tune PA since different manufacturers have varying flow characteristics

**Tip:** Run a proper PA calibration test (tower or line method) for your baseline value. Use in-print adjustments only for minor tweaks when you notice issues mid-print.

**Note:** Like other Tune adjustments, PA changes during a print are temporary. To save a PA value permanently, update your slicer's filament profile or your Klipper configuration.

---

## Controls Panel

![Controls Panel](../images/user/controls.png)

The Controls Panel provides access to all printer adjustments, organized into cards.

### Motion Controls

![Motion Controls](../images/user/controls-motion.png)

**Jog Pad:** Move print head manually
- X/Y crosshair for horizontal movement
- Z buttons for up/down
- Distance selector: 0.1mm, 1mm, 10mm, 100mm

**Home Buttons:**
- **Home All:** X, Y, and Z
- **Home XY:** Only X and Y axes
- **Home Z:** Only Z axis (probe required)

**Position Display:** Shows current X, Y, Z coordinates

### Temperature Controls

![Temperature Controls](../images/user/controls-temperature.png)

**Nozzle Panel:**
- Current temperature display
- Target temperature input
- **Presets:** Off, PLA (210°C), PETG (240°C), ABS (250°C)
- Temperature graph

**Bed Panel:**
- Current temperature display
- Target temperature input
- **Presets:** Off, PLA (60°C), PETG (80°C), ABS (100°C)
- Temperature graph

### Extrusion Panel

Control filament movement:
- **Extrude:** Push filament through nozzle
- **Retract:** Pull filament back
- **Amount selector:** 5mm, 10mm, 25mm, 50mm
- **Speed selector:** Slow, Normal, Fast

**Safety:** Requires hotend at minimum temperature

### Fan Controls

Control all configured fans:
- **Part cooling fan:** 0-100%
- **Hotend fan:** Usually automatic
- **Auxiliary fans:** If configured

Sliders for fine control, or tap percentage to type exact value.

### Bed Mesh

3D visualization of bed surface:
- **Color gradient:** Blue (low) to Red (high)
- **Touch to rotate** the view
- **Mesh profile selector:** Switch between saved meshes

Actions:
- **Calibrate:** Run new mesh probing
- **Clear:** Remove current mesh
- **Load:** Load a saved profile

### PID Tuning

Calibrate temperature controllers:
- Select **Nozzle** or **Bed**
- Enter target temperature
- Run automatic tuning
- Save results to configuration

---

## AMS Panel

![Filament Panel](../images/user/filament.png)

The AMS panel provides multi-material system control for Happy Hare, AFC-Klipper, Bambu AMS, and Valgace backends. Access via the **Spool icon** in the navigation bar.

### Slot Status

Visual display of all filament slots (typically 4-8):
- **Color indicators** show loaded filament colors
- **Active slot** highlighted with border
- **Empty slots** shown as gray
- Tap a slot to select it for loading operations

### Load/Unload Controls

- **Load:** Feed filament from selected slot to toolhead
- **Unload:** Retract filament back to buffer
- **Home:** Run homing sequence for the AMS

### Dryer Control

If your AMS has a dryer (e.g., AFC with drying box):
- Temperature display and control
- Timer settings
- Enable/disable drying cycle

### Spoolman Integration

When Spoolman is configured, the panel shows:
- Spool name and material type
- Remaining filament weight
- Tap a slot to open the **Spool Picker** and assign a different spool

---

## Settings

![Settings Panel](../images/user/settings.png)

Access settings via the **Gear icon** in the navigation bar.

### Display Settings

Tap **Display** to open the full display settings overlay:

- **Brightness:** Display backlight level (0-100%)
- **Dim timeout:** When the screen dims to save power (30s, 1m, 2m, 5m, or Never)
- **Sleep timeout:** When the screen turns off completely (1m, 2m, 5m, 10m, or Never)
- **Time format:** 12-hour or 24-hour clock display
- **Bed mesh render:** Auto, 3D, or 2D visualization mode

**Theme Settings:** Tap **Theme** to open the theme explorer:
- Browse available themes (built-in and custom)
- Preview themes with live dark/light mode toggle
- Tap **Apply** to save your selection (restart required for full effect)
- Tap **Edit** to customize colors in the theme editor

### Sound Settings

- **Enable sounds:** Toggle all sound effects
- **Test beep:** Plays a test tone (M300 command)
- **Completion alert:** Choose how to be notified when prints finish (Off, Notification, or Alert)

### Network Settings

- **WiFi:** Connect to wireless networks
- **Moonraker:** Change printer connection address

### Sensor Settings

Tap **Sensors** to manage all printer sensors:

**Filament sensors:** Enable or disable runout detection for each sensor. Choose the sensor role:
- **Runout:** Pauses print when filament runs out
- **Motion:** Detects filament movement (for clog detection)
- **Ignore:** Sensor is present but not monitored

**Other sensors:** View detected sensors including:
- Accelerometers (for Input Shaper)
- Probe sensors (BLTouch, eddy current, etc.)
- Humidity sensors
- Width sensors
- Color sensors

### Touch Calibration

> **Note:** This option only appears on touchscreen displays, not in the desktop simulator.

Tap **Touch Calibration** to recalibrate your touchscreen if taps register in the wrong location.

**How to calibrate:**
1. Tap the crosshairs that appear on screen (usually 4-5 points)
2. Tap as accurately as possible in the center of each target
3. Calibration saves automatically when complete

The settings row shows "Calibrated" or "Not calibrated" status.

### Hardware Health

Tap **Hardware Health** to see detected hardware issues:

**Issue categories:**
- **Critical:** Required hardware missing (e.g., heater, probe)
- **Warning:** Expected hardware not found
- **Info:** Newly discovered hardware
- **Session:** Hardware that changed since last session

**Actions for non-critical issues:**
- **Ignore:** Mark hardware as optional (won't warn again)
- **Save:** Add to expected hardware list (will warn if removed later)

Use this when you add/remove hardware to keep HelixScreen's expectations accurate.

### Safety Settings

- **E-Stop confirmation:** Require tap-and-hold confirmation before emergency stop

### Machine Limits

Tap **Machine Limits** to adjust motion limits:

- Maximum velocity, acceleration, and jerk for each axis
- These override your Klipper config for the current session
- Use when testing or troubleshooting motion issues

### Other Settings

- **Macro buttons:** Configure quick-access macro shortcuts
- **Plugins:** View and manage installed plugins
- **Factory reset:** Clear all settings and run wizard again
- **About:** Version information (tap 7 times for memory debug mode)

---

## Advanced Features

![Advanced Panel](../images/user/advanced.png)

Access advanced features via the **More (⋯)** icon in the navigation bar.

### Input Shaper

Tune vibration compensation for smoother, faster prints:

1. Navigate to **Advanced > Input Shaper**
2. Select axis to test (X or Y)
3. Tap **Measure** to run resonance test
4. Review recommended shaper type and frequency
5. Tap **Apply** to save to configuration

The panel shows current shaper settings and test results with frequency graphs.

### Screws Tilt Adjust

Assisted manual bed leveling using the SCREWS_TILT_CALCULATE macro:

1. Navigate to **Advanced > Screws Tilt**
2. Tap **Measure** to probe all bed screws
3. View adjustment amounts for each screw (e.g., "CW 00:15" = clockwise 15 minutes)
4. Adjust screws and re-measure until level

Color coding: green (level), yellow (minor adjustment), red (significant adjustment needed).

### Notification History

Review past system notifications:

1. Tap the **bell icon** in the status bar
2. Scroll through notification history
3. Tap **Clear All** to dismiss

Notifications are color-coded by severity: info (blue), warning (yellow), error (red).

### Macro Panel

Execute custom Klipper macros:

1. Navigate to **Controls > Macros**
2. Browse available macros (alphabetically sorted)
3. Tap a macro to execute immediately

**Notes:**
- System macros (starting with `_`) are hidden by default
- Macro names are "prettified" (CLEAN_NOZZLE → "Clean Nozzle")
- Dangerous macros (SAVE_CONFIG, etc.) require confirmation

### Console Panel

View G-code command history:

1. Navigate to **Controls > Console**
2. Scroll through recent commands and responses

**Color coding:**
- **White:** Commands you sent
- **Green:** Successful responses
- **Red:** Errors and warnings

### Power Device Control

Control Moonraker power devices:

1. Navigate to **Controls > Power**
2. Toggle devices on/off with switches

**Notes:**
- Devices locked during prints (safety feature)
- Lock icon shows which devices are protected

### Print History

View past print jobs:

1. Navigate to **Advanced > Print History**

**Dashboard view:**
- Total prints, success rate
- Print time and filament usage statistics
- Trend graphs

**List view:**
- Search by filename
- Filter by status (completed, failed, cancelled)
- Sort by date, duration, or name

**Detail view:**
- Tap any job for full details
- **Reprint:** Start the same file again
- **Delete:** Remove from history

### Timelapse Settings

Configure Moonraker-Timelapse:

1. Navigate to **Advanced > Timelapse**
2. Enable/disable timelapse recording
3. Select mode:
   - **Layer Macro:** Snapshot at each layer
   - **Hyperlapse:** Time-based snapshots
4. Set framerate (15/24/30/60 fps)
5. Enable auto-render for automatic video creation

---

## Keyboard & Input

### On-Screen Keyboard

The keyboard appears automatically when text input is needed:

- **QWERTY layout** with number row
- **Long-press** for alternate characters (e.g., hold 'a' for '@')
- **Mode buttons:** ?123 for symbols, ABC for letters
- **Shift** for uppercase

### Touch Gestures

- **Tap:** Select, button press
- **Swipe:** Scroll lists and panels
- **Long press:** Alternative characters (keyboard)
- **Pinch/spread:** Zoom in 3D views (if supported)

### Developer Shortcuts

When using SDL2 development simulator:
- **S key:** Take screenshot (saves to /tmp/)
- **Escape:** Exit application

---

## Tips & Tricks

### Temperature Shortcuts

Tap a temperature display on the Home panel to jump directly to its control panel.

### Quick Navigation

From any sub-panel, the navigation bar always takes you back to main panels.

### Pre-Print Checks

The pre-print options remember your last choices per slicer. If you always run bed mesh before PrusaSlicer prints, that preference persists.

### Disconnection Handling

If connection is lost:
1. HelixScreen shows a reconnection toast
2. Automatic reconnection attempts every few seconds
3. When reconnected, state is restored automatically
4. During disconnection, Controls and Filament panels are disabled (safety)

### AMS/Multi-Material

For Happy Hare or AFC-Klipper systems:
- Slot status shows on home panel
- Use Filament panel for load/unload operations
- Color information comes from Spoolman (if integrated)

### Memory Usage

HelixScreen is optimized for low memory:
- Typical usage: 50-80MB
- No X server or desktop environment needed
- Direct framebuffer rendering

### Log Viewing

For troubleshooting, view logs from SSH:
```bash
# Live logs
sudo journalctl -u helixscreen -f

# Last 100 lines
sudo journalctl -u helixscreen -n 100
```

---

*Next: [Troubleshooting](TROUBLESHOOTING.md) - Solutions to common problems*
