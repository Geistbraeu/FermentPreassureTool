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
    bool valveOpen = manualOn;
    digitalWrite(HardwareConfig::SOLENOID_PIN, valveOpen ? HIGH : LOW);
    if (!initialized || !lastManualOverride || valveOpen != lastValveOpen) {
      DBGF("[Valve] MANUAL %s\n", valveOpen ? "OPEN" : "CLOSED");
    }
    initialized = true;
    lastManualOverride = true;
    lastValveOpen = valveOpen;
    return;
  }

  bool valveOpen = digitalRead(HardwareConfig::SOLENOID_PIN) == HIGH;
  bool desiredValveOpen = valveOpen;
  if (currentPressure > maxPressureThreshold) {
    desiredValveOpen = true;
  } else if (currentPressure < (maxPressureThreshold - hysteresis)) {
    desiredValveOpen = false;
  }

  if (desiredValveOpen != valveOpen) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, desiredValveOpen ? HIGH : LOW);
    valveOpen = desiredValveOpen;
  }

  if (!initialized || lastManualOverride || valveOpen != lastValveOpen) {
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
