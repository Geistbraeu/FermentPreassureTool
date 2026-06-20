#ifndef SOLENOID_CONTROLLER_H
#define SOLENOID_CONTROLLER_H

class SolenoidController {
public:
  void init();
  void applyState(bool manualOverride, bool manualOn, float currentPressure, float maxPressureThreshold, float hysteresis);
};

extern SolenoidController solenoidController;

#endif // SOLENOID_CONTROLLER_H
