#include "device/DisplayManager.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>

Adafruit_SSD1306 display(DisplayConfig::SCREEN_WIDTH, DisplayConfig::SCREEN_HEIGHT, &Wire, DisplayConfig::OLED_RESET_PIN);
bool isOledConnected = false;
DisplayManager displayManager;

bool DisplayManager::init() {
  if (display.begin(SSD1306_SWITCHCAPVCC, DisplayConfig::OLED_I2C_ADDRESS)) {
    isOledConnected = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Starting...");
    display.display();
    return true;
  }

  Serial.println(F("SSD1306 allocation failed (no OLED connected)"));
  return false;
}

void DisplayManager::update(const String& ipStatus, float voltage, float pressureBar, int pressureUnit, float maxPressureThreshold) {
  if (!isOledConnected) {
    return;
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(DisplayConfig::LAYOUT_X_LEFT, DisplayConfig::LAYOUT_Y_HOSTNAME);
  display.print(NetworkConfig::HOSTNAME);

  float pDisplay = (pressureUnit == 0) ? (pressureBar / SensorConfig::PSI_TO_BAR) : pressureBar;
  String unitStr = (pressureUnit == 0) ? "PSI" : "Bar";
  float maxPVal = (pressureUnit == 0) ? maxPressureThreshold : (maxPressureThreshold * SensorConfig::PSI_TO_BAR);
  String maxPStr = String(maxPVal, 1) + " " + unitStr;

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(maxPStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(DisplayConfig::SCREEN_WIDTH - w, 0);
  display.print(maxPStr);
  display.drawLine(0, 15, DisplayConfig::SCREEN_WIDTH - 1, 15, SSD1306_WHITE);

  display.setTextSize(3);
  display.setCursor(5, 20);
  if (pDisplay < 10.0f) {
    display.print(pDisplay, 2);
  } else if (pDisplay < 100.0f) {
    display.print(pDisplay, 1);
  } else {
    display.print((int)pDisplay);
  }

  display.setTextSize(2);
  display.setCursor(DisplayConfig::LAYOUT_X_CENTER, DisplayConfig::LAYOUT_Y_PRESSURE);
  display.print(unitStr);

  display.setTextSize(1);
  display.setCursor(0, DisplayConfig::LAYOUT_Y_MANUAL);
  display.print(ipStatus);

  String voltStr = String(voltage, 2) + " V";
  display.getTextBounds(voltStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(DisplayConfig::SCREEN_WIDTH - w, DisplayConfig::LAYOUT_Y_MANUAL);
  display.print(voltStr);

  display.display();
}

bool DisplayManager::isConnected() const {
  return isOledConnected;
}
