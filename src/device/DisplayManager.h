#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
  bool init();
  void update(const String& ipStatus, float voltage, float pressureBar, int pressureUnit, float maxPressureThreshold);
  bool isConnected() const;
};

// Shared OLED objects are used by ConfigPortal as well.
extern Adafruit_SSD1306 display;
extern bool isOledConnected;
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H
