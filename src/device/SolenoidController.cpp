#include "device/SolenoidController.h"
#include <Arduino.h>
#include "config.h"
#include "debug.h"

SolenoidController solenoidController;

void SolenoidController::init() {
  pinMode(HardwareConfig::SOLENOID_PIN, OUTPUT);
  digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);
}

void SolenoidController::applyState(bool manualOverride, bool manualOn, float currentPressure, float maxPressureThreshold, float hysteresis) {
  static bool initialized = false;
  static bool lastManualOverride = false;
  static bool lastValveOpen = false;

  if (manualOverride) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, manualOn ? HIGH : LOW);
    bool valveOpen = (manualOn ? HIGH : LOW) == HIGH;
    if (!initialized || !lastManualOverride || valveOpen != lastValveOpen) {
      DBGF("[Valve] MANUAL %s\n", valveOpen ? "OPEN" : "CLOSED");
    }
    initialized = true;
    lastManualOverride = true;
    lastValveOpen = valveOpen;
    return;
  }

  bool changed = false;
  if (currentPressure > maxPressureThreshold) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, HIGH);
    changed = true;
  } else if (currentPressure < (maxPressureThreshold - hysteresis)) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);
    changed = true;
  }

  bool valveOpen = digitalRead(HardwareConfig::SOLENOID_PIN) == HIGH;
  if (!initialized || lastManualOverride || changed || valveOpen != lastValveOpen) {
    DBGF("[Valve] AUTO %s (P=%.2f, threshold=%.2f, hysteresis=%.2f)\n",
         valveOpen ? "OPEN" : "CLOSED",
         currentPressure,
         maxPressureThreshold,
         hysteresis);
  }

  initialized = true;
  lastManualOverride = false;
  lastValveOpen = valveOpen;
}
