#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <esp_adc_cal.h>

struct SensorReading {
  float voltage = 0.0f;
  float pressure = 0.0f;
};

class SensorManager {
public:
  void initAdc(esp_adc_cal_characteristics_t* adcChars);
  void initTempSensor(bool useTempSensor);
  float readTemperature(bool isEnabled, float tempOffset, bool* isConnected);
  SensorReading readFilteredPressure(unsigned int sampleCount, unsigned long sampleDelayMs, float offsetVoltage,
                                    const esp_adc_cal_characteristics_t* adcChars);
};

extern SensorManager sensorManager;

#endif // SENSOR_MANAGER_H
