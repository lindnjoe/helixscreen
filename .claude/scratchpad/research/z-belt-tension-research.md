# Z Belt Tension Research for Voron 2.4 / Belted Z Printers

**Date:** 2026-02-03
**Status:** Research in progress
**Context:** Investigating whether belt resonance feature can extend to Z belts

---

## The Challenge: 4 Independent Z Belts

Voron 2.4 and similar "flying gantry" printers have 4 separate Z belts:
- One at each corner (Front-Left, Front-Right, Rear-Left, Rear-Right)
- Each belt connects a Z motor to the gantry via pulleys
- Belts work together to lift/lower and level the gantry

**Key differences from A/B (XY) belts:**
| Aspect | A/B Belts (CoreXY) | Z Belts (Quad Gantry) |
|--------|-------------------|----------------------|
| Number | 2 | 4 |
| Motion | Continuous (printing) | Occasional (Z moves, QGL) |
| Load | Toolhead acceleration | Static gantry weight + gravity |
| Test method | `TEST_RESONANCES AXIS=1,1` | **Not supported** |
| Target freq | ~110 Hz | ~140 Hz |

---

## Current State of Z Belt Testing in Klipper

### Bad News: No Direct Support

From [Klipper discourse](https://klipper.discourse.group/t/test-resonances-on-z-axis/14669):
> "most of the printers will not be able to sustain the resonance test on Z axis (due to Z velocity and accel limitations)"

The `TEST_RESONANCES AXIS=Z` command doesn't exist in standard Klipper. Even the new Z-axis input shaper feature (for faster Z moves) doesn't address belt tension measurement.

### Why Z is Different

1. **Velocity limits:** Z axes typically max out at 15-25 mm/s (vs 300+ mm/s for X/Y)
2. **Acceleration limits:** Z accel often 100-500 mm/s² (vs 3000-10000 for X/Y)
3. **Can't do frequency sweep:** The resonance test needs to oscillate rapidly to excite belt resonance

### What Users Currently Do

Manual process (same as phone app):
1. Position gantry so belt span = 150mm from idler to fixed point
2. Pluck each belt with finger
3. Use smartphone app (Spectroid, guitar tuner) to measure frequency
4. Adjust top tensioner pulley
5. Move gantry up/down to settle, repeat
6. Target: ~140 Hz for all 4 belts

From [Voron Docs](https://docs.vorondesign.com/tuning/secondary_printer_tuning.html):
> "Move the gantry upwards until the fixed side of the belt is 150mm from the Z idler centers. Pluck, measure, and adjust."

---

## Potential Approaches for HelixScreen

### Approach 1: Microphone-Based Measurement 🎤

**Idea:** Use a microphone attached to the Pi to detect belt pluck frequency

**Pros:**
- Works for Z belts (no motor movement needed)
- User manually plucks - we just listen
- I2S MEMS microphones are cheap (~$5)
- Python FFT analysis is well-documented

**Cons:**
- Requires additional hardware (mic)
- Ambient noise interference (fans, etc)
- Acoustic measurement accuracy ~±10%
- Need to be close to belt being plucked

**Hardware options:**
- **I2S MEMS mic** (INMP441, SPH0645): Best quality, direct Pi connection
- **USB mic:** Easier setup but often noisy/low quality
- **Contact mic:** Attached to frame, less ambient noise

**Implementation:**
```python
# Conceptual - using pyaudio + numpy FFT
import pyaudio
import numpy as np

def detect_belt_frequency(audio_chunk, sample_rate=44100):
    # Apply FFT
    fft = np.fft.rfft(audio_chunk)
    freqs = np.fft.rfftfreq(len(audio_chunk), 1/sample_rate)

    # Find peak in belt frequency range (80-200 Hz)
    mask = (freqs > 80) & (freqs < 200)
    peak_idx = np.argmax(np.abs(fft[mask]))
    return freqs[mask][peak_idx]
```

### Approach 2: ADXL Accelerometer with Manual Pluck 📊

**Idea:** User plucks belt, ADXL on toolhead/bed picks up vibration

**Pros:**
- Uses existing hardware (many printers have ADXL)
- More accurate than acoustic (~±2-5%)
- No ambient noise issues
- Already integrated with Klipper

**Cons:**
- ADXL may be far from Z belt being plucked
- Vibration must travel through frame
- May need accelerometer repositioned near Z belt
- Unclear if signal is strong enough

**Test needed:** Does plucking a Z belt create measurable vibration at the toolhead ADXL?

### Approach 3: Motor Excitation (Limited Z Motion) ⚡

**Idea:** Small Z oscillations to excite belt resonance

**Pros:**
- Automated (no manual plucking)
- Can test each belt individually (sort of)

**Cons:**
- Z velocity/accel limits may prevent proper excitation
- All 4 belts move together - hard to isolate
- Risk of QGL getting thrown off
- May need custom Klipper code

**Feasibility:** Low. The physics don't work - you can't oscillate Z fast enough to hit 140 Hz resonance.

### Approach 4: Optical/Laser Measurement (Hardware) 🔴

**Idea:** Laser pointed at belt, measures deflection frequency

**Pros:**
- Most accurate method (~±1%)
- Industrial standard

**Cons:**
- Requires custom hardware
- Way overkill for hobbyist use
- Out of scope for HelixScreen

---

## Recommended Approach: Microphone + Guided Workflow

**Why microphone makes sense for Z belts:**

1. **Z belts MUST be plucked manually anyway** - can't use motor excitation
2. **User already does this** - just with a phone app instead
3. **We can provide better UX:**
   - Guide user through each belt (FL, FR, RL, RR)
   - Show live frequency as they pluck
   - Compare all 4 at once
   - Calculate if QGL will be affected

**Hardware requirement:**
- I2S MEMS microphone (INMP441 or similar)
- ~$5, easy to wire to Pi GPIO

**UX Flow:**
```
┌─────────────────────────────────────┐
│  Z Belt Tension Check         [?]   │
├─────────────────────────────────────┤
│                                     │
│  Position gantry at 150mm from      │
│  Z idler centers (middle height)    │
│                                     │
│  ┌─────────────────────────────┐    │
│  │   FL: ---    FR: ---        │    │
│  │                              │    │
│  │   RL: ---    RR: ---        │    │
│  └─────────────────────────────┘    │
│                                     │
│  Pluck each belt. Target: 140 Hz    │
│                                     │
│  Live: [========142 Hz=========]    │
│        ↑ Listening...               │
│                                     │
│  [Assign to FL] [FR] [RL] [RR]      │
│                                     │
└─────────────────────────────────────┘
```

**After all 4 measured:**
```
┌─────────────────────────────────────┐
│  Z Belt Results                     │
├─────────────────────────────────────┤
│                                     │
│  ┌─────────────────────────────┐    │
│  │   FL: 142✓   FR: 138⚠       │    │
│  │                              │    │
│  │   RL: 141✓   RR: 128✗       │    │
│  └─────────────────────────────┘    │
│                                     │
│  Target: 140 Hz (±5 is ideal)       │
│                                     │
│  ⚠ RR is 12 Hz low. This may cause  │
│    gantry tilt issues.              │
│                                     │
│  [Retest RR]  [Done]                │
│                                     │
└─────────────────────────────────────┘
```

---

## Hybrid Approach: Microphone + ADXL

**Best of both worlds:**

1. **For XY belts:** Use ADXL + motor excitation (existing plan)
2. **For Z belts:** Use microphone + manual pluck (new)

This gives HelixScreen complete belt health coverage for Voron 2.4 and similar printers.

**Feature detection:**
- Check if printer has `[quad_gantry_level]` config → belted Z
- Check if microphone hardware available → enable Z belt feature
- Fall back to "use your phone app" instructions if no mic

---

## Alternative: Contact Microphone on Frame

A contact/piezo microphone attached to the printer frame could:
- Pick up vibration from belt plucks
- Be less affected by ambient noise
- Potentially work as an alternative to ADXL

**Hardware:** Generic piezo pickup (~$3-10)

This is worth testing - might be better than air mic for noisy environments.

---

## Open Questions

1. **Microphone hardware selection:**
   - I2S MEMS vs USB vs contact piezo?
   - Which has best SNR for belt frequencies?

2. **Can ADXL detect Z belt plucks?**
   - Need to test on real hardware
   - Vibration transmission through frame

3. **Multiple belts at once:**
   - Can we identify which belt was plucked?
   - Probably not - user must indicate

4. **Quad Gantry Level integration:**
   - Should we suggest QGL after belt adjustment?
   - Warn if tensions are very unequal?

---

## Sources

- [Voron Secondary Printer Tuning](https://docs.vorondesign.com/tuning/secondary_printer_tuning.html) - Official Z belt procedure
- [Klipper TEST_RESONANCES on Z-Axis Discussion](https://klipper.discourse.group/t/test-resonances-on-z-axis/14669) - Why Z testing is hard
- [Z-Axis Input Shaper Feature](https://klipper.discourse.group/t/feature-testing-z-axis-input-shaper/24886) - New Klipper feature
- [Ellis Print Tuning Guide - V2 Gantry Squaring](https://ellis3dp.com/Print-Tuning-Guide/articles/voron_v2_gantry_squaring.html) - Comprehensive guide
- [Voron Forum - Z Belt Tension Issue](https://forum.vorondesign.com/threads/z-belt-tension-issue.1285/) - Common problems
- [Voron Forum - Automated Belt Tension](https://forum.vorondesign.com/threads/measuring-belt-tension-automated.1474/) - Community discussion
- [GitHub - rpi_i2s](https://github.com/makerportal/rpi_i2s) - Python I2S microphone FFT
- [Belt Tension Frequency Methods Comparison](https://forum.prusa3d.com/forum/hardware-firmware-and-software-help/belt-tension-measurements-by-audio-frequency-spectrogram/) - Accuracy discussion

---

## Next Steps

1. [ ] Test if ADXL can detect Z belt plucks (on real printer)
2. [ ] Research I2S microphone setup on Raspberry Pi
3. [ ] Prototype audio FFT frequency detection in Python
4. [ ] Design UI mockups for Z belt workflow
5. [ ] Decide: microphone vs contact mic vs ADXL-only approach
