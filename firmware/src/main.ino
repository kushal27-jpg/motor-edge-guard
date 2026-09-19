#define BLYNK_TEMPLATE_ID "TMPL3-c0jieKV"
#define BLYNK_TEMPLATE_NAME "Motor Edge Guard"
#define BLYNK_AUTH_TOKEN "plhnVF4XvapE--SMnFlfzIn4Qw1AX6m1"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// --- WI-FI CREDENTIALS ---
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// --- PIN DEFINITIONS ---
const int SENSOR_PIN = 34;   // LM358 Signal / SCT-013 -> ESP32 ADC Pin 34
const int RELAY_PIN  = 13;   // SSR Control Pin (Active-HIGH)
const int RESET_BTN  = 4;    // Push Button Reset (Active-LOW)

// --- SYSTEM STATE & FLAGS ---
bool motorCommandedOn = false; // Starts in standby until toggled
bool isStartingUp     = false;
bool faultTripped     = false;
bool systemArmed      = false;
float baseRMS         = 0.0;
String systemStatus   = "STANDBY";
unsigned long motorTurnOnTime = 0; // Inrush protection marker

// Fault Verification Debounce Counters
int frictionConfirmCount = 0;
int dryRunConfirmCount   = 0;

// Selective Auto-Restart Timing (Fault 1 & Fault 2 only)
const unsigned long AUTO_RESTART_DELAY_MS = 15000; // 15s demonstration window
int fault2AutoRetryCount = 0;                      // Exactly 1 retry for bearing friction

BlynkTimer timer;

// Forward declarations
void calibrateBaseline();
void resetSystem();
void finishStartup();
void setStatusLedColor(String hexColor);

// --- CURRENT SAMPLING ENGINE (FULL 50Hz AC CYCLES TO PREVENT PEAK SPIKES) ---
float getSmoothedRMS() {
  long sumSquare = 0;
  const int totalSamples = 400; // 400 * 100us = 40ms = 2 full cycles at 50Hz
  
  // 200 samples * 100us = 20ms (Exactly 1 full 50Hz AC cycle for true DC midpoint)
  long rawSum = 0;
  for (int i = 0; i < 200; i++) {
    rawSum += analogRead(SENSOR_PIN);
    delayMicroseconds(100);
  }
  int dcOffset = rawSum / 200;

  for (int i = 0; i < totalSamples; i++) {
    long sample = analogRead(SENSOR_PIN) - dcOffset;
    sumSquare += (sample * sample);
    delayMicroseconds(100);
  }

  return sqrt((float)sumSquare / totalSamples);
}

float readFilteredCurrent() {
  return getSmoothedRMS();
}

void calibrateBaseline() {
  float total = 0;
  for (int i = 0; i < 5; i++) {
    total += readFilteredCurrent();
    delay(10);
  }
  baseRMS = total / 5.0;
  Serial.print("--> BASELINE LOCKED: ");
  Serial.println(baseRMS, 1);
  if (Blynk.connected()) {
    Blynk.virtualWrite(V1, baseRMS);
  }
}

void setStatusLedColor(String hexColor) {
  if (Blynk.connected()) {
    Blynk.setProperty(V6, "color", hexColor);
    Blynk.virtualWrite(V6, 255);
  }
}

// Non-blocking completion of motor inrush stabilization
void finishStartup() {
  if (!motorCommandedOn || faultTripped) return;

  calibrateBaseline();
  systemArmed = true;
  isStartingUp = false;
  systemStatus = "HEALTHY";
  
  if (Blynk.connected()) {
    Blynk.virtualWrite(V3, systemStatus);
  }
  setStatusLedColor("#00FF00"); // Green
  Serial.println("--> MOTOR STABILIZED & HEALTHY <--\n");
}

void resetSystem() {
  // Physical relay actuated FIRST for zero lag
  digitalWrite(RELAY_PIN, HIGH);

  faultTripped = false;
  systemArmed  = false;
  frictionConfirmCount = 0;
  dryRunConfirmCount   = 0;

  motorCommandedOn = true;
  isStartingUp = true;
  motorTurnOnTime = millis();
  
  if (Blynk.connected()) {
    Blynk.virtualWrite(V5, 1); // Keep UI switch ON
    systemStatus = "STABILIZING";
    Blynk.virtualWrite(V3, systemStatus);
  }
  setStatusLedColor("#FFFF00"); // Yellow

  Serial.println("\n[RESET] Re-engaging motor... Stabilizing (5s)");
  timer.setTimeout(5000L, finishStartup);
}

// Remote Reset Button (V4)
BLYNK_WRITE(V4) {
  int val = param.asInt();
  if (val == 1 && faultTripped) {
    fault2AutoRetryCount = 0; // Manual reset clears retry lockouts
    resetSystem();
  }
}

// Motor ON/OFF Toggle Switch (V5)
BLYNK_WRITE(V5) {
  int val = param.asInt();
  
  if (val == 1) {
    if (!faultTripped && !motorCommandedOn) {
      digitalWrite(RELAY_PIN, HIGH); // Actuate immediately
      motorCommandedOn = true;
      isStartingUp = true;
      systemArmed = false;
      fault2AutoRetryCount = 0;
      motorTurnOnTime = millis();

      systemStatus = "STABILIZING";
      if (Blynk.connected()) {
        Blynk.virtualWrite(V3, systemStatus);
      }
      setStatusLedColor("#FFFF00"); // Yellow
      Serial.println("\n[MANUAL] Motor Commanded ON... Soft Inrush (5s)");

      timer.setTimeout(5000L, finishStartup);
    }
  } else {
    digitalWrite(RELAY_PIN, LOW); // Cut motor immediately
    motorCommandedOn = false;
    systemArmed = false;
    isStartingUp = false;

    systemStatus = "MANUAL OFF";
    if (Blynk.connected()) {
      Blynk.virtualWrite(V3, systemStatus);
      Blynk.virtualWrite(V6, 0); // Turn off Status LED
    }
    Serial.println("\n[MANUAL] Motor Powered OFF");
  }
}

// Telemetry Push Every 350ms
void sendTelemetry() {
  if (motorCommandedOn && systemArmed && !faultTripped && Blynk.connected()) {
    Blynk.virtualWrite(V0, readFilteredCurrent());
    Blynk.virtualWrite(V1, baseRMS);
    Blynk.virtualWrite(V3, systemStatus);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RESET_BTN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, LOW); // Motor starts OFF in safe mode

  Serial.println("\nConnecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  systemStatus = "MANUAL OFF";
  if (Blynk.connected()) {
    Blynk.virtualWrite(V3, systemStatus);
    Blynk.virtualWrite(V5, 0); // Ensure switch starts OFF
    Blynk.virtualWrite(V6, 0); // LED off
  }

  timer.setInterval(350L, sendTelemetry);
}

void loop() {
  Blynk.run();
  timer.run();

  // Reset Trigger: Hardware Button (Pin 4) or Serial ('r')
  if ((digitalRead(RESET_BTN) == LOW || (Serial.available() > 0 && (Serial.peek() == 'r' || Serial.peek() == 'R'))) && faultTripped) {
    if (Serial.available() > 0) Serial.read();
    fault2AutoRetryCount = 0; // Manual button clears lockout
    resetSystem();
  }

  // Safety Cutoff Latch or Standby
  if (!motorCommandedOn || isStartingUp || faultTripped) {
    if (faultTripped || !motorCommandedOn) {
      digitalWrite(RELAY_PIN, LOW);
    }
    return;
  }

  // Inrush Guard: Ignore trips during the initial 5-second stabilization window
  if (!systemArmed || isStartingUp || (millis() - motorTurnOnTime < 5000)) return;

  float currentRMS = readFilteredCurrent();
  float delta = currentRMS - baseRMS;

  // Serial Diagnostics
  Serial.print("RMS: ");     Serial.print(currentRMS, 1);
  Serial.print(" | Base: "); Serial.print(baseRMS, 1);
  Serial.print(" | Delta: ");
  if (delta >= 0) Serial.print("+");
  Serial.print(delta, 1);

  // ========================================================
  //              3-TIER FAULT ISOLATION MATRIX
  // ========================================================

  // FAULT 1: Dry-Run / Broken Belt / Unclipped CT (< 500.0) -> AUTO-RESTART ENABLED
  if (currentRMS < 500.0) {
    dryRunConfirmCount++;
    frictionConfirmCount = 0;
    Serial.print(" | [WARN: LOW CURRENT] Frame ");
    Serial.print(dryRunConfirmCount);
    Serial.println("/2");

    if (dryRunConfirmCount >= 2) {
      digitalWrite(RELAY_PIN, LOW);
      systemStatus = "TRIP: DRY RUN";
      faultTripped = true;

      if (Blynk.connected()) {
        Blynk.virtualWrite(V0, currentRMS);
        Blynk.virtualWrite(V2, delta);
        Blynk.virtualWrite(V3, systemStatus);
      }
      setStatusLedColor("#FF0000"); // Red
      Blynk.logEvent("fault_alert", "CRITICAL: Dry Run Detected! Auto-restarting in 15s...");
      Serial.println(" | [TRIP: FAULT 1 - DRY RUN] Power Isolated! Scheduled Auto-Restart in 15s...");

      // Non-blocking auto-restart for Fault 1
      timer.setTimeout(AUTO_RESTART_DELAY_MS, []() {
        if (faultTripped && systemStatus == "TRIP: DRY RUN") {
          Serial.println("\n[AUTO-RESTART] Recovering from Fault 1 (Dry Run)...");
          resetSystem();
        }
      });
    }
    return;
  } else {
    dryRunConfirmCount = 0;
  }

  // FAULT 3: Instant Locked Rotor Stall (delta >= 90.0) -> HARD LOCKOUT ONLY (NO AUTO-RESTART)
  if (delta >= 90.0) {
    digitalWrite(RELAY_PIN, LOW);
    systemStatus = "TRIP: LOCKED ROTOR";
    faultTripped = true;

    if (Blynk.connected()) {
      Blynk.virtualWrite(V0, currentRMS);
      Blynk.virtualWrite(V2, delta);
      Blynk.virtualWrite(V3, systemStatus);
    }
    setStatusLedColor("#FF0000"); // Red
    Blynk.logEvent("fault_alert", "CRITICAL: Locked Rotor Jam! HARD LOCKOUT. Manual Reset Required.");
    Serial.println(" | [TRIP: FAULT 3 - LOCKED ROTOR] Power Isolated! HARD LOCKOUT (No Auto-Restart)");
    return;
  }

  // FAULT 2: Bearing Friction / Mechanical Overload (delta 45.0 to 80.0) -> SINGLE RETRY AUTO-RESTART
  if (delta >= 45.0 && delta < 80.0) {
    frictionConfirmCount++;
    Serial.print(" | [WARN: OVERLOAD DRAG] Frame ");
    Serial.print(frictionConfirmCount);
    Serial.println("/2");

    if (frictionConfirmCount >= 2) {
      digitalWrite(RELAY_PIN, LOW);
      faultTripped = true;

      if (fault2AutoRetryCount < 1) {
        fault2AutoRetryCount++;
        systemStatus = "TRIP: DRAG RETRY";

        if (Blynk.connected()) {
          Blynk.virtualWrite(V0, currentRMS);
          Blynk.virtualWrite(V2, delta);
          Blynk.virtualWrite(V3, systemStatus);
        }
        setStatusLedColor("#FF0000"); // Red
        Blynk.logEvent("fault_alert", "WARNING: Bearing Friction Detected! Cooling down, 1 retry in 15s.");
        Serial.println(" | [TRIP: FAULT 2 - BEARING FRICTION] Power Isolated! 1 Auto-Restart armed in 15s...");

        // Single Auto-Restart for Fault 2
        timer.setTimeout(AUTO_RESTART_DELAY_MS, []() {
          if (faultTripped && systemStatus == "TRIP: DRAG RETRY") {
            Serial.println("\n[AUTO-RESTART] Executing 1-time Bearing Drag Recovery...");
            resetSystem();
          }
        });
      } else {
        // Second strike: Hard Lockout
        systemStatus = "TRIP: BEARING LOCKOUT";

        if (Blynk.connected()) {
          Blynk.virtualWrite(V0, currentRMS);
          Blynk.virtualWrite(V2, delta);
          Blynk.virtualWrite(V3, systemStatus);
        }
        setStatusLedColor("#FF0000"); // Red
        Blynk.logEvent("fault_alert", "CRITICAL: Persistent Bearing Friction! Hard Lockout Enforced.");
        Serial.println(" | [TRIP: FAULT 2 - HARD LOCKOUT] Repeated drag. Service required!");
      }
    }
    return;
  } else {
    frictionConfirmCount = 0;
  }

  // Gradual Friction Pre-Warning (+15.0 to +45.0 Delta)
  if (delta >= 15.0 && delta < 45.0) {
    systemStatus = "DRAG WARNING";
    setStatusLedColor("#FFA500"); // Amber / Orange
    Serial.println(" | [LOAD WARNING: MODERATE FRICTION]");
  } else {
    systemStatus = "HEALTHY";
    setStatusLedColor("#00FF00"); // Green
    Serial.println(" | [STATUS: HEALTHY]");
  }

  if (Blynk.connected()) {
    Blynk.virtualWrite(V2, delta);
  }

  // Dynamic baseline drift tracking (EMA)
  baseRMS = (0.01 * currentRMS) + (0.99 * baseRMS);
}

