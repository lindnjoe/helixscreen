# Microphone Audio Input Feasibility on Raspberry Pi / ARM SBCs

**Date:** 2026-02-03
**Status:** Research complete
**Context:** Investigating audio input options for belt frequency detection

---

## Executive Summary

**Verdict: Feasible, but with caveats.**

| Approach | Feasibility | Complexity | Best For |
|----------|-------------|------------|----------|
| I2S MEMS Mic (INMP441) | ✅ Good | Medium | Pi 3/4, clean signal |
| USB Microphone | ⚠️ Okay | Low | Quick prototyping |
| Raspberry Pi Codec Zero | ✅ Good | Low | Official Pi solution |
| Other SBCs (Orange Pi) | ⚠️ Variable | High | Not recommended |

**Recommendation:** Target Raspberry Pi 3/4/5 with I2S MEMS microphone or the official Codec Zero HAT. Don't try to support arbitrary SBCs initially.

---

## Hardware Options

### Option 1: I2S MEMS Microphone (INMP441)

**What it is:** Digital microphone that connects directly to Pi GPIO pins via I2S protocol.

**Hardware cost:** ~$5-8 for INMP441 breakout board

**Wiring (5 wires):**
```
INMP441 → Raspberry Pi
3V      → Pin 1 (3.3V)
GND     → Pin 6 (GND)
SEL     → Pin 6 (GND) for left channel
BCLK    → Pin 12 (BCM 18)
DOUT    → Pin 38 (BCM 20)
LRCL    → Pin 35 (BCM 19)
```

**Software setup (Pi OS Bookworm):**
```bash
# Add to /boot/firmware/config.txt:
dtoverlay=googlevoicehat-soundcard

# Reboot, then test:
arecord -D plughw:1 -c1 -r 48000 -f S32_LE -t wav -V mono -v test.wav
```

**Pros:**
- High quality digital signal (24-bit)
- No analog noise issues
- Well-documented for Raspberry Pi
- Low latency possible

**Cons:**
- Requires GPIO wiring
- Pi 5 needs different device tree overlay (extra steps)
- Default volume is very low (needs software amplification)
- DC offset needs filtering before amplification

**Pi 5 compatibility:** Works, but requires custom device tree overlay. See [RPi forums](https://forums.raspberrypi.com/viewtopic.php?t=359494).

### Option 2: Raspberry Pi Codec Zero (~$20)

**What it is:** Official Pi Foundation audio HAT with built-in MEMS mic.

**Hardware cost:** ~$20

**Features:**
- Built-in MEMS microphone
- 3.5mm stereo input jack (external mic option)
- Audio output (speaker/headphone)
- Pi Zero form factor (also works on full-size Pi)
- Programmable LEDs + button

**Software setup:**
```bash
# Add to /boot/firmware/config.txt:
dtoverlay=iqaudio-codec

# Comment out default audio:
#dtparam=audio=on

# Load microphone config:
sudo alsactl restore -f IQaudIO_Codec_OnboardMIC_record_and_SPK_playback.state

# Test:
arecord --device=hw:1,0 --format S16_LE --rate 44100 -c2 test.wav
```

**Pros:**
- Official Raspberry Pi product
- Clean integration
- Multiple input options (built-in or external mic)
- Good documentation

**Cons:**
- More expensive than bare MEMS mic
- Takes up HAT slot
- Some Bookworm OS compatibility issues reported (use Bullseye if issues)
- Built-in mic quality is "okay" not great

### Option 3: USB Microphone

**What it is:** Any USB microphone plugged into Pi USB port.

**Hardware cost:** $5-50+ depending on quality

**Software setup:**
```bash
# Usually plug-and-play
arecord -l  # Find device
arecord -D plughw:1,0 -f cd test.wav
```

**Pros:**
- Zero wiring
- Works on any SBC with USB
- Easy to try different mics

**Cons:**
- Cheap USB mics are VERY noisy
- Often have 50/60 Hz hum
- "USB microphones under ~30 EUR are extremely bad"
- Latency can be higher
- Less deterministic than I2S

**Verdict:** Fine for prototyping, not recommended for production feature.

### Option 4: Contact/Piezo Microphone

**What it is:** Piezoelectric pickup that attaches to printer frame.

**Hardware cost:** $3-10

**How it works:** Picks up vibration through physical contact, not air.

**Pros:**
- Less affected by ambient noise (fans, etc.)
- Can be permanently attached to frame
- Cheap

**Cons:**
- Needs ADC (analog-to-digital converter)
- Signal conditioning required
- Less common setup
- May pick up motor noise instead of belt

**Verdict:** Interesting alternative worth testing, but more complex.

---

## Other SBCs (Orange Pi, Rock Pi, etc.)

### Orange Pi (H3, H5, H618 based)

**I2S support:** Requires custom kernel modules and device tree overlays. Armbian community has some support, but it's not plug-and-play.

**Issues:**
- No standard I2S overlays for many boards
- Each SoC (H3, H5, H618) needs different config
- Orange Pi Zero 2W notably lacks I2S overlay support

**Verdict:** Too fragmented to support. Users would need to figure it out themselves.

### Rock Pi S

**I2S support:** Has I2S input/output, users have gotten 2-channel mics working.

**Issues:**
- Requires custom dtbo overlay files for some configs
- Smaller user community = less help

**Verdict:** Possible but not worth targeting initially.

### Generic ARM SBC

**Verdict:** Don't try to support. I2S is too hardware-specific. Recommend USB mic as fallback, with warning about quality.

---

## Signal Processing Requirements

### Frequency Range

Belt frequencies we care about:
- **XY belts:** ~110 Hz target
- **Z belts:** ~140 Hz target
- **Detection range:** 80-200 Hz should cover everything

### Sample Rate Requirements

Per Nyquist theorem: sample rate must be ≥ 2× highest frequency.

| Frequency Range | Min Sample Rate | Recommended |
|-----------------|-----------------|-------------|
| 80-200 Hz | 400 Hz | 1000-4000 Hz |

**Good news:** Even the lowest common audio sample rate (8000 Hz) is MORE than enough. We don't need high-fidelity audio.

### FFT Configuration

For good frequency resolution in the 80-200 Hz range:

```python
# Example config
SAMPLE_RATE = 4000  # Hz (low = less CPU, more than enough for belt freq)
FFT_SIZE = 1024     # Power of 2
FREQ_RESOLUTION = SAMPLE_RATE / FFT_SIZE  # = 3.9 Hz per bin

# Belt frequency bins
# 110 Hz → bin 28
# 140 Hz → bin 36
```

**Frequency resolution:** With 4000 Hz sample rate and 1024-sample FFT:
- Resolution = 4000/1024 = ~3.9 Hz per bin
- Good enough to distinguish 110 Hz from 140 Hz easily

---

## Python Libraries

### PyAudio (Recommended)

```python
import pyaudio
import numpy as np

CHUNK = 1024
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 4000  # Low sample rate is fine for belt frequencies

p = pyaudio.PyAudio()
stream = p.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)

# Read and FFT
data = np.frombuffer(stream.read(CHUNK), dtype=np.int16)
fft = np.abs(np.fft.rfft(data))
freqs = np.fft.rfftfreq(len(data), 1/RATE)

# Find peak in belt frequency range
mask = (freqs > 80) & (freqs < 200)
peak_freq = freqs[mask][np.argmax(fft[mask])]
print(f"Detected frequency: {peak_freq:.1f} Hz")
```

**Pros:** Simple, lightweight, well-tested on Pi
**Cons:** Needs PortAudio installed

### sounddevice (Alternative)

```python
import sounddevice as sd
import numpy as np

RATE = 4000
DURATION = 0.5  # seconds

recording = sd.rec(int(DURATION * RATE), samplerate=RATE, channels=1)
sd.wait()

fft = np.abs(np.fft.rfft(recording.flatten()))
# ... same FFT processing
```

**Pros:** Cleaner API, good NumPy integration
**Cons:** Slightly more overhead than PyAudio

### python-rtmixer (For lowest latency)

Uses C callbacks to avoid Python GIL. Probably overkill for our use case since we're not doing real-time synthesis.

---

## CPU Usage Concerns

### Will this interfere with Klipper?

**Short answer:** No, if done correctly.

**Belt frequency detection requirements:**
- Sample rate: 4000 Hz (very low)
- FFT size: 1024 samples
- Update rate: ~4 Hz (every 0.25 seconds)
- CPU usage: Negligible (<1% on Pi 3+)

**Klipper's requirements:**
- Needs consistent timing for stepper control
- Runs mostly on MCU, not host CPU
- Host CPU handles G-code parsing, web UI

**Mitigation strategies:**
1. Only run audio capture during belt tuning (not during printing!)
2. Use separate thread for audio processing
3. On Pi 4+, could isolate CPU core if needed (overkill)

**Real-world:** People run cameras, web UIs, and complex processing alongside Klipper on Pi 4 without issues. Simple FFT analysis will be fine.

---

## Recommended Implementation Path

### Phase 1: Prototype on Pi 4 with USB mic

1. Use any cheap USB mic for initial testing
2. Get FFT frequency detection working in Python
3. Test with actual belt plucks
4. Evaluate signal quality / noise floor

### Phase 2: Test I2S MEMS (INMP441)

1. Wire up INMP441 to Pi GPIO
2. Configure device tree overlay
3. Compare signal quality to USB mic
4. Determine if I2S is worth the extra setup

### Phase 3: Integration with HelixScreen

1. Add optional microphone feature
2. Auto-detect microphone hardware
3. Implement UI workflow
4. Document supported hardware

### Phase 4: Test on Pi 5

1. Get I2S working with Pi 5 device tree
2. Verify Codec Zero compatibility
3. Update documentation

---

## Hardware Recommendations

### For Users

| Priority | Hardware | Cost | Notes |
|----------|----------|------|-------|
| 1st | Raspberry Pi Codec Zero | ~$20 | Official, just works |
| 2nd | INMP441 breakout | ~$5 | Cheap, requires wiring |
| 3rd | USB mic (decent quality) | ~$30+ | Avoid cheap ones |

### For HelixScreen Documentation

"Belt frequency detection requires a microphone. Supported options:
- **Recommended:** Raspberry Pi Codec Zero HAT
- **Budget:** INMP441 I2S MEMS microphone (wiring required)
- **Fallback:** USB microphone (quality varies, not recommended for noisy environments)

Not supported: Orange Pi, Rock Pi, other SBCs (use USB mic at your own risk)"

---

## Open Questions

1. **Does ambient fan noise interfere?** Need real-world testing
2. **Contact mic vs air mic?** Worth testing piezo on frame
3. **Pi 5 device tree?** Need to verify current status
4. **Integration with accelerometer?** Can we detect plucks via ADXL too?

---

## Sources

- [Adafruit I2S MEMS Microphone Setup Guide](https://learn.adafruit.com/adafruit-i2s-mems-microphone-breakout/raspberry-pi-wiring-test)
- [Medium: Setting up MEMS I2S Microphone on Pi](https://medium.com/@martin.hodges/setting-up-a-mems-i2s-microphone-on-a-raspberry-pi-306248961043)
- [Raspberry Pi Codec Zero Documentation](https://www.raspberrypi.com/documentation/accessories/audio.html)
- [RPi Forums: Pi 5 I2S Details](https://forums.raspberrypi.com/viewtopic.php?t=359494)
- [Maker Portal: I2S MEMS Microphone FFT Analysis](https://github.com/makerportal/rpi_i2s)
- [Maker Portal: Audio Processing in Python (Nyquist, FFT)](https://makersportal.com/blog/2018/9/13/audio-processing-in-python-part-i-sampling-and-the-fast-fourier-transform)
- [Klipper FAQ: Hardware Requirements](https://www.klipper3d.org/FAQ.html)
- [Armbian Forums: I2S on Orange Pi](https://forum.armbian.com/topic/759-i2s-on-orange-pi-h3/)
