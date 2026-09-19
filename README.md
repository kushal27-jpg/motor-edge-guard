# motor-edge-guard

**Edge-Based Motor Protection & IoT Diagnostic System**

Motor Edge Guard is an embedded protection and IoT diagnostic prototype designed for induction motors and agricultural borewell pumps.

The system uses an ESP32 microcontroller and a non-invasive SCT-013 current sensor to monitor motor current locally, identify abnormal operating conditions, and disconnect power through a solid-state relay.

Its edge-based fault detection operates independently of cloud availability, while Blynk IoT provides remote monitoring and control.

### Key Highlights

- Local edge-based motor fault detection.
- True-RMS current monitoring.
- Adaptive baseline tracking using EMA.
- Three-tier fault-aware protection logic.
- Automatic recovery for selected fault conditions.
- Blynk IoT remote monitoring and control.

### Achievement

🏆 **2nd Place | Hardware Edition**

Smart India Hackathon (SIH)  
College-Level Competition

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

### Step 1: Signal Acquisition
* **Hardware:** Non-invasive SCT-013 Current Transformer with active LM358 op-amp conditioning.
* **Input:** Read via 12-bit ADC on **GPIO 34**.

### Step 2: DC Midpoint Calibration (20 ms)
* Collects 200 samples at 100 µs intervals across one full 50 Hz AC cycle.
* Computes the dynamic midpoint offset to center the 1.65V virtual ground:
  $$\text{dcOffset} = \frac{\sum \text{analogRead}}{200}$$

### Step 3: Discrete True-RMS Integration (40 ms)
* Collects 400 samples at 100 µs intervals across two complete 50 Hz AC cycles.
* Subtracts the dynamic offset and calculates discrete RMS:
  $$\text{currentRMS} = \sqrt{\frac{\sum (\text{sample} - \text{dcOffset})^2}{400}}$$

### Step 4: Inrush Blanking Guard
* Checks elapsed running time since relay actuation:
  * **Elapsed < 5000 ms:** Inrush bypass active; fault evaluation skipped to allow rotor magnetization.
  * **Elapsed >= 5000 ms:** System armed; fault matrices engaged.

### Step 5: Deviation Analysis
* Calculates immediate mechanical load deviation:
  $$\text{Delta} = \text{currentRMS} - \text{baseRMS}$$

### Step 6: 3-Tier Fault Isolation Matrix
* **Tier 1: Dry Run / Cavitation (`currentRMS < 500.0`, 2 frames)**
  * *Action:* Instant relay cutoff (GPIO 13 LOW).
  * *Recovery:* Non-blocking auto-restart after 15 seconds to allow water table recovery.
* **Tier 2: Bearing Friction / Overload (`Delta +45.0 to +80.0`, 2 frames)**
  * *Action:* Instant relay cutoff (GPIO 13 LOW).
  * *Recovery:* Exactly 1 auto-restart retry after 15 seconds cooldown. Permanent lockout on repeat.
* **Tier 3: Locked Rotor Stall (`Delta >= +90.0`, Instant)**
  * *Action:* Deterministic sub-80ms power cutoff (GPIO 13 LOW).
  * *Recovery:* Hard lockout. Auto-restart strictly disabled. Manual reset required.
* **Predictive Pre-Warning (`Delta +15.0 to +45.0`)**
  * *Action:* Amber indicator sent to dashboard for preventative maintenance.

### Step 7: Dynamic Baseline Drift (EMA)
* Updates the baseline using an Exponential Moving Average to absorb line voltage fluctuations:
  $$\text{baseRMS} = (0.01 \times \text{currentRMS}) + (0.99 \times \text{baseRMS})$$

### Step 8: Telemetry Dispatch
* Non-blocking background sync via `BlynkTimer` every 350 ms.

---

## Dependencies & Toolchain

### Required Software
* **Arduino IDE:** v2.x or PlatformIO Core
* **ESP32 Board Package:** `esp32` by Espressif Systems (v2.0.x or later)

### Required Libraries
* **Blynk:** `Blynk` by Volodymyr Shymanskyy (v1.3.x or later)
* **WiFi:** Bundled natively with the ESP32 Arduino Core (`WiFi.h`, `WiFiClient.h`)

---

## Compilation & Flashing Guide

1. In Arduino IDE, navigate to **Tools > Board > esp32 > ESP32 Dev Module**.
2. Configure the upload parameters:
   * **Flash Frequency:** 80MHz
   * **Flash Mode:** QIO
   * **CPU Frequency:** 240MHz (WiFi/BT)
   * **Upload Speed:** 921600 (or 115200 for low-noise cables)
   * **Core Debug Level:** None
   * **Partition Scheme:** Default 4MB with spiffs
3. Open `src/main.ino` and configure your credentials:
   ```cpp
   char ssid[] = "YOUR_WIFI_SSID";
   char pass[] = "YOUR_WIFI_PASSWORD";
   

 

4.Connect the ESP32 via Micro-USB, select the appropriate port, and click Upload.

## Non-Blocking Telemetry Protocol
To prevent network congestion from halting deterministic motor protection:
The SSR trigger pin (GPIO 13) is actuated on line 1 of fault interrupts before any cloud sync calls.
Telemetry push events (Blynk.virtualWrite) are scheduled via non-blocking BlynkTimer callbacks running at 350 ms intervals.
All cloud transmissions are guarded by if (Blynk.connected()), preventing loop stalling during intermittent Wi-Fi conditions.
   
