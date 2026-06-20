#include "device/SolenoidController.h"
#include <Arduino.h>
#include "config.h"

SolenoidController solenoidController;

void SolenoidController::init() {
  pinMode(HardwareConfig::SOLENOID_PIN, OUTPUT);
  digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);
}

void SolenoidController::applyState(bool manualOverride, bool manualOn, float currentPressure, float maxPressureThreshold, float hysteresis) {
  if (manualOverride) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, manualOn ? HIGH : LOW);
    return;
  }

  if (currentPressure > maxPressureThreshold) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, HIGH);
  } else if (currentPressure < (maxPressureThreshold - hysteresis)) {
    digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);
  }
}
