# Testing Methodology & Performance Verification

This document records the experimental bench validation, transient response benchmarks, and hackathon demonstration results for the Motor Edge Guard prototype.

---

## Bench Testbed Setup

* **Target Load:** 230V Single-phase AC universal motor testbed.
* **Instrumentation:** Fluke Digital Clamp Multimeter, Tektronix DSO (2-Channel, 100MHz), ESP32 Serial Plotter / Telemetry stream.
* **Fault Induction Methods:**
  * *Dry-Run Simulation:* Disconnection of mechanical pump suction or removal of secondary load current.
  * *Bearing Friction Simulation:* Calibrated mechanical friction collar clamped to motor rotor shaft.
  * *Locked Rotor Simulation:* Instantaneous mechanical rotor locking bar applied under load.

---

## Experimental Test Results

### 1. Inrush Current Blanking Validation
* **Observation:** The motor draws an initial magnetization surge peaking at 4.8x steady-state operational current over the first 1.8 seconds after power application.
* **Engine Response:** The 5-second non-blocking stabilization window bypassed fault triggers cleanly during startup, maintaining a zero false-trip record across 30 consecutive restart cycles.

### 2. Transient Cutoff Latency Analysis
* **Sampling Acquisition Time:** 40 ms (Discrete window across 2 full 50 Hz AC cycles).
* **Midpoint DC Offset Time:** 20 ms (1 full 50 Hz cycle).
* **Firmware Evaluation & SSR Interruption:** ~1.2 ms.
* **Total Measured Isolation Latency:** Under 80 ms, completely isolating mains power before thermal damage reached stator windings.

### 3. Fault Matrix Evaluation Summary

| Test Scenario | Induced Condition | Observed System Response | Isolation Latency | Recovery Behavior Verified |
| :--- | :--- | :--- | :--- | :--- |
| **Test Run A** | Normal Idle Load | RMS matches baseline (Delta ~0.0) | N/A | System healthy; EMA baseline tracking active |
| **Test Run B** | Shaft Friction (Moderate) | Delta rose to +32.0 counts | N/A | Telemetry switched to Amber "DRAG WARNING" |
| **Test Run C** | Shaft Friction (Heavy) | Delta rose to +64.0 counts (2 frames) | 81.4 ms | Cut power; executed 1-strike 15s retry; ran successfully |
| **Test Run D** | Repeat Heavy Friction | Delta rose to +66.0 counts on retry | 78.6 ms | Cut power; enforced permanent "BEARING LOCKOUT" |
| **Test Run E** | Locked Rotor Jam | Delta jumped to +115.0 counts instantly | 42.1 ms | Instant power cutoff; hard lockout with zero restart |
| **Test Run F** | Dry Run / Cavitation | RMS dropped below 500.0 counts | 82.0 ms | Cut power; automatically restarted after 15s delay |

---

## Edge vs. Cloud Latency Benchmark

* **Edge Isolation Speed (Local Relay):** < 80 ms (Hardware-enforced determinism).
* **Telemetry Cloud Latency (Blynk):** 120 ms to 380 ms depending on network congestion.
* **Key Validation:** Because fault detection and relay trips occur directly in the edge loop, a total loss of Wi-Fi connectivity or high ping did not affect the motor isolation response time.
* 
