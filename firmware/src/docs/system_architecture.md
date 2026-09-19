# System Architecture & Technical Design

The Motor Edge Guard system is structured around an edge-first, decoupled architecture. 

All signal conditioning, high-speed discrete numerical integration, fault categorization, and hardware isolation routines are executed entirely on bare-metal firmware. Network synchronization functions run asynchronously in background time slices without blocking critical safety loops.

---

## Architecture Overview

### Physical Layer
* **Load:** 230V AC Single-Phase Induction / Universal Motor
* **Power Gate:** Zero-cross Solid-State Relay (SSR) wired in series with the AC mains live line
* **Current Sensing:** Split-core SCT-013 current transformer clamped around the live line

### Signal Conditioning Stage (Analog Front-End)
* **Burden Resistor:** Converts the CT secondary induced current into an alternating voltage
* **Active Biasing:** LM358 operational amplifier shifts the AC waveform midpoint by +1.65V
* **Scaling:** Maps the signal safely within the 0V to 3.3V dynamic window required by the ESP32 ADC

### Edge DSP & Processing (ESP32)
* **Core Loop:** High-speed sampling, 20 ms dynamic DC bias extraction, 40 ms True-RMS integration
* **Dynamic Drift Filter:** Real-time Exponential Moving Average (EMA) to compensate for line voltage drops
* **Fault Isolation Engine:** Evaluates running current against a 3-tier threshold matrix to trigger sub-80ms power cutoff
* **Background Process:** Non-blocking asynchronous telemetry dispatch to Blynk IoT via BlynkTimer (350 ms interval)

### Cloud & Telemetry Layer
* **Platform:** Blynk IoT Cloud
* **Interfaces:** Mobile app dashboard and web console for live current gauges, historical baseline plots, and critical alerts

---

## Core Subsystems

### 1. Analog Front-End (AFE)
* **Sensing Principle:** Current Transformer (CT) magnetic induction (SCT-013)
* **Signal Scaling:** Burden resistor selected to map peak current to a safe operational envelope
* **Virtual Ground Biasing:** The LM358 operational amplifier shifts the AC waveform midpoint by +1.65V, mapping the full waveform within the 0V to 3.3V span required by the ESP32 ADC

### 2. Deterministic Edge DSP
* **Sample Rate:** 10 kHz (100 µs intervals per discrete sample)
* **Sampling Depth:** Exactly 400 continuous samples across two full 50 Hz power cycles (40 ms window), eliminating cycle-boundary truncation errors
* **Offset Compensation:** 20 ms dynamic calibration samples an entire AC wave to extract the instantaneous DC offset before applying the RMS square-summation algorithm
* **Dynamic Drift Tracking:** An Exponential Moving Average (EMA) baseline filter adjusts dynamically to rural line voltage variations:
  $$\text{baseRMS} = (0.01 \times \text{currentRMS}) + (0.99 \times \text{baseRMS})$$

### 3. Isolation & Actuation
* **Solid-State Relay:** Driven via GPIO 13
* **Actuation Latency:** Hardware cutoff executes in under 80 ms from the detection of persistent overcurrent or locked rotor stall
* **Fail-Safe Startup:** In the event of a microcontroller crash or power loss, GPIO 13 defaults LOW, keeping the relay open and the motor isolated

### 4. Non-Blocking Cloud Communication
* Telemetry pushes are decoupled from loop processing via `BlynkTimer` running every 350 ms
* Network timeouts or disconnected Wi-Fi do not halt the local sampling and fault trip loops

---

## Latency & Timing Budget

* **DC Midpoint Offset Capture:** 20 ms (1 full AC cycle) — Critical Priority
* **True-RMS Discrete Integration:** 40 ms (2 full AC cycles) — Critical Priority
* **Fault Evaluation & Relay Cutoff:** Under 1 ms (Direct Register / Digital I/O) — Highest Priority
* **Blynk Cloud Telemetry Push:** 15 to 30 ms (Asynchronous non-blocking slice) — Background Priority
  
