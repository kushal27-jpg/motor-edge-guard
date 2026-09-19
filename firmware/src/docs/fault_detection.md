# 3-Tier Fault Detection Engine & Recovery Matrix

This document provides the mathematical criteria, debouncing logic, and state transitions that govern the Motor Edge Guard protection engine.

---

## State Transition Model

### 1. STANDBY / MANUAL OFF
* Motor relay output (`GPIO 13`) held LOW.
* Sensor sampling loop idles; no fault timers active.

### 2. STABILIZING (Inrush Blanking Window)
* Motor relay output toggled HIGH (`GPIO 13`).
* 5000 ms non-blocking timer armed to allow rotor magnetization inrush current to clear.
* Fault detection logic is bypassed during this phase to eliminate startup false trips.
* Status LED glows Yellow.

### 3. HEALTHY (Operational Steady State)
* Initial baseline current locked via a 5-frame moving average.
* System evaluates running True-RMS current against dynamic baseline thresholds.
* Dynamic drift tracking (EMA) runs continuously.
* Status LED glows Green.

### 4. FAULT TRIPPED & ISOLATED
* Mains relay instantly cut (LOW) in sub-80 ms.
* Status LED turns Red; critical push event logged to Blynk cloud.
* System branches into recovery based on fault severity.

---

## 3-Tier Fault Classification Matrix

### Tier 1: Dry Run / Broken Belt / Fluid Loss
* **Electrical Characteristic:** Extreme reduction in motor load current below the pump's minimum no-load operational ceiling.
* **Mathematical Condition:**
  $$\text{currentRMS} < 500.0$$
* **Debounce Window:** 2 consecutive confirmation frames (~80 ms).
* **System Action:** Instant power cutoff via SSR (`GPIO 13` LOW).
* **Recovery Policy (Auto-Restart Enabled):** Non-blocking 15-second cooldown delay. After 15 seconds, the system automatically triggers `resetSystem()` to allow borewell groundwater recovery without human intervention.

### Tier 2: Bearing Friction & Mechanical Overload
* **Electrical Characteristic:** Progressive mechanical resistance, shaft misalignment, or bearing degradation causing moderate current rise above the calibrated running baseline.
* **Mathematical Condition:**
  $$\Delta = \text{currentRMS} - \text{baseRMS} \in [+45.0, +80.0]$$
* **Debounce Window:** 2 consecutive confirmation frames (~80 ms).
* **System Action:** Instant power cutoff via SSR (`GPIO 13` LOW).
* **Recovery Policy (1-Strike Cooldown Retry):** 
  * *Strike 1:* System waits 15 seconds for stator winding cooling, then executes exactly one auto-restart attempt (`TRIP: DRAG RETRY`).
  * *Strike 2:* If the overload reoccurs immediately upon stabilization, auto-restart is disabled and the engine enters permanent lockout (`TRIP: BEARING LOCKOUT`).

### Tier 3: Instant Locked Rotor Stall
* **Electrical Characteristic:** Severe mechanical blockage or foreign object jam causing the motor rotor to stall instantly, driving currents to winding breakdown levels.
* **Mathematical Condition:**
  $$\Delta = \text{currentRMS} - \text{baseRMS} \ge +90.0$$
* **Debounce Window:** Instantaneous (0 debouncing frames).
* **System Action:** Immediate deterministic sub-80 ms power cutoff.
* **Recovery Policy (Hard Lockout Only):** Zero automated restarts. Requires physical on-site push button reset (`GPIO 4`) or explicit manual remote app restart (`V4`) following inspection.

---

## Predictive Pre-Warning Stage

* **Condition:** Current deviation $\Delta$ between $+15.0$ and $+45.0$.
* **State Label:** `DRAG WARNING`
* **Indicator:** Dashboard Status LED glows Amber / Orange.
* **Operational Intent:** Enables condition-based predictive maintenance. Alerts operators to clean pump impellers or lubricate mechanical bearings before thermal overload occurs.

---

## Fault Debounce & Inrush Blanking Flowchart

```text
[Current Sample Acquired]
           │
     Is Inrush Active? (<5s) ──YES──> [Bypass Fault Checks]
           │ NO
   Compute Delta (RMS - Base)
           │
           ├─── Delta >= 90.0 ──────────────> [TRIP: LOCKED ROTOR (Hard Lockout)]
           │
           ├─── Delta in [+45.0, +80.0]
           │         │
           │         ├── Frame Count < 2 ───> [Increment Drag Counter]
           │         └── Frame Count >= 2 ──> [TRIP: BEARING OVERLOAD]
           │                                        │
           │                                 Retry Count < 1?
           │                                   ├── YES ──> [15s Delay -> Retry]
           │                                   └── NO  ──> [BEARING LOCKOUT]
           │
           ├─── currentRMS < 500.0
           │         │
           │         ├── Frame Count < 2 ───> [Increment Dry Counter]
           │         └── Frame Count >= 2 ──> [TRIP: DRY RUN (15s Auto-Restart)]
           │
           ├─── Delta in [+15.0, +45.0] ────> [AMBER WARNING (Predictive Drag)]
           │
           └─── Normal Envelope ────────────> [HEALTHY (Update EMA Baseline)]
           
