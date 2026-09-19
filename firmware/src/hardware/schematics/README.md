# Hardware Schematics & Circuit Conditioning

This directory documents the analog signal conditioning and high-voltage power isolation circuits for the Motor Edge Guard prototype.

---

## High-Level Circuit Architecture

* **AC Mains (230V):** Feeds live line power through the motor circuit.
* **SCT-013 CT Sensor:** Clamped on the live line, outputs 0 to 50mA AC proportional to load current.
* **LM358 Active Conditioning Board:** Adds a +1.65V DC midpoint offset and scales the signal to a 0 to 3.3V range.
* **ESP32 Edge Unit (GPIO 34):** Samples the conditioned analog wave using its internal 12-bit ADC.
* **ESP32 Output (GPIO 13):** Sends an Active-HIGH trigger to the Solid-State Relay (SSR).
* **Solid-State Relay (SSR):** Acts as an inline isolation switch on the 230V live line to cut power in under 80 ms.

---

## Signal Conditioning Stage (LM358 Op-Amp)

### Problem Definition
The SCT-013 current transformer generates an alternating bipolar current waveform with positive and negative peaks relative to ground. Connecting this directly to an ESP32 ADC will:
1. Clip the negative half-cycles, destroying True-RMS calculation accuracy.
2. Exceed reverse-voltage limits on internal ADC clamping diodes, causing chip damage.

### Circuit Solution
The conditioning board built around the LM358 Dual Operational Amplifier addresses this using three sub-stages:
* **Burden Resistor:** Converts the induced secondary current from the CT into an alternating voltage signal.
* **Virtual DC Bias (+1.65V):** A precision voltage divider shifts the oscillating AC signal upward by +1.65V (half of 3.3V) so it swings safely between 0V and 3.3V for the unipolar ADC.
* **Filtering Capacitor:** Decouples residual high-frequency grid noise to ground before ADC conversion.

---

## Actuation & Mains Isolation (Solid-State Relay)

* **Component:** Zero-Cross Solid-State Relay (SSR) or Optocoupled Relay Module.
* **Control Pin:** GPIO 13 (Configured as digital OUTPUT).
* **Logic Level:** Active-HIGH (3.3V energizes the gate to run the motor; 0V de-energizes to isolate power).
* **Galvanic Isolation:** The internal optical isolator provides complete electrical separation between the low-voltage ESP32 logic rail and the 230V AC inductive motor line, eliminating reverse EMF spikes and ground loops.
* **Mains Wiring:** The SSR output terminals are wired strictly in series with the Phase (Live) line feeding the motor. Neutral runs directly to the load.

---

## Hardware Safety Guidelines

* **Single-Conductor Rule:** The SCT-013 current clamp must be clipped around only the Phase (Live) wire or only the Neutral wire. Clipping around a two-core cable cancels opposing magnetic fields, resulting in a zero reading.
* **ADC Voltage Protection:** Never feed more than 3.3V into GPIO 34. The LM358 circuit is powered from the ESP32 3.3V output rail to guarantee that the conditioned output cannot exceed the ADC ceiling.
* **Common Ground:** Tie the LM358 ground and relay control ground directly to the ESP32 GND pin to maintain a stable analog measurement reference.
  
