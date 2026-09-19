# motor-edge-guard
Edge-based motor protection and IoT diagnostic prototype for induction motors and agricultural borewell pumps.
# Firmware Architecture & Implementation

This directory contains the production-grade Arduino/C++ firmware running bare-metal on the ESP32 (Xtensa Dual-Core 32-bit LX6). 

The firmware is designed around deterministic edge execution: all fault detection, True-RMS signal processing, and physical relay cutoffs execute locally within the main loop without depending on network callbacks or cloud connectivity.

---

## Technical Specifications

| Parameter | Specification | Engineering Reason |
| :--- | :--- | :--- |
| **ADC Resolution** | 12-Bit (0 to 4095 counts) | Provides fine-grained quantification of small bearing friction variations. |
| **Sampling Frequency** | 10 kHz (100 µs per sample) | Satisfies the Nyquist criterion for 50 Hz fundamental and harmonic distortion. |
| **Window Length** | 400 Samples (40 ms) | Exactly two complete 50 Hz AC cycles, preventing half-cycle boundary truncation. |
| **DC Midpoint Correction** | 200 Samples (20 ms) | Samples one full cycle to establish the virtual AC ground offset before squaring. |
| **Dynamic Drift Filter** | EMA (alpha = 0.01) | Tracks slow line-voltage drops and thermal drift without shifting trip thresholds. |
| **Inrush Protection** | Non-blocking 5s blanking | Allows induction motor inrush surge to clear before arming safety tiers. |

---

## Edge DSP Pipeline
Analog Current Input (GPIO 34)
│
▼
[20ms Dynamic DC Offset Acquisition]  ──> rawSum / 200 (Centers 1.65V bias)
│
▼
[40ms True-RMS Discrete Integration]  ──> sqrt( Σ(sample - offset)^2 / 400 )
│
▼
[Dynamic Baseline Comparison]        ──> delta = currentRMS - baseRMS
│
├──> If Delta in [+15, +45]  ──> Amber Drag Warning
├──> If Delta in [+45, +80]  ──> Tier 2 Trip (1-strike Cooldown Auto-Restart)
├──> If Delta >= +90         ──> Tier 3 Trip (Sub-80ms Relay Cutoff, Hard Lockout)
└──> If RMS < 500            ──> Tier 1 Trip (Dry Run, 15s Auto-Restart)
│
▼
[Exponential Moving Average Update]  ──> baseRMS = (0.01 * currentRMS) + (0.99 * baseRMS)


---

## Dependencies & Toolchain

### Required Software
- **Arduino IDE:** v2.x or PlatformIO Core
- **ESP32 Board Package:** `esp32` by Espressif Systems (v2.0.x or later)

### Required Libraries
- **Blynk:** `Blynk` by Volodymyr Shymanskyy (v1.3.x or later)
- **WiFi:** Bundled natively with the ESP32 Arduino Core (`WiFi.h`, `WiFiClient.h`)

---

## Compilation & Flashing Guide

1. In Arduino IDE, navigate to **Tools > Board > esp32 > ESP32 Dev Module**.
2. Configure the upload parameters:
   - **Flash Frequency:** 80MHz
   - **Flash Mode:** QIO
   - **CPU Frequency:** 240MHz (WiFi/BT)
   - **Upload Speed:** 921600 (or 115200 for low-noise serial cables)
   - **Core Debug Level:** None
   - **Partition Scheme:** Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
3. Open `src/main.ino` and configure your credentials:
   ```cpp
   char ssid[] = "YOUR_WIFI_SSID";
   char pass[] = "YOUR_WIFI_PASSWORD";

Connect the ESP32 via Micro-USB, select the appropriate port, and click Upload.

##Non-Blocking Telemetry Protocol
To prevent network congestion from halting deterministic motor protection:
The SSR trigger pin (GPIO 13) is actuated on line 1 of fault interrupts before any cloud sync calls.
Telemetry push events (Blynk.virtualWrite) are scheduled via non-blocking BlynkTimer callbacks running at 350 ms intervals.
All cloud transmissions are guarded by if (Blynk.connected()), preventing loop stalling during intermittent Wi-Fi conditions.
   
