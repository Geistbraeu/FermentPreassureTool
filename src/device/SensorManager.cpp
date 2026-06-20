#include "device/SensorManager.h"
#include "config.h"
#include "debug.h"
#include <Adafruit_MAX31865.h>
#include <esp_adc_cal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const int sensorPin = HardwareConfig::ADC_PRESSURE_PIN;
static Adafruit_MAX31865 tempSensor(
    HardwareConfig::TEMP_SENSOR_CS_PIN,
    HardwareConfig::TEMP_SENSOR_DI_PIN,
    HardwareConfig::TEMP_SENSOR_DO_PIN,
    HardwareConfig::TEMP_SENSOR_CLK_PIN);

SensorManager sensorManager;

void SensorManager::initAdc(esp_adc_cal_characteristics_t* adcChars) {
  analogSetAttenuation(ADC_11db);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, SensorConfig::ADC_VREF, adcChars);
}

void SensorManager::initTempSensor(bool useTempSensor) {
  if (useTempSensor) {
    tempSensor.begin(MAX31865_2WIRE);
  }
}

float SensorManager::readTemperature(bool isEnabled, float tempOffset, bool* isConnected) {
  if (!isEnabled) {
    if (isConnected != NULL) {
      *isConnected = false;
    }
    return 0.0f;
  }

  uint16_t fault = tempSensor.readFault();
  if (fault) {
    DBGF("MAX31865 Fault: %u\n", fault);
    tempSensor.clearFault();
    if (isConnected != NULL) {
      *isConnected = false;
    }
    return 0.0f;
  }

  float t = tempSensor.temperature(SensorConfig::RNOMINAL, SensorConfig::RREF);
  if (t < SensorConfig::TEMP_FAULT_THRESHOLD) {
    if (isConnected != NULL) {
      *isConnected = false;
    }
    return 0.0f;
  }

  if (isConnected != NULL) {
    *isConnected = true;
  }
  return t - tempOffset;
}

SensorReading SensorManager::readFilteredPressure(unsigned int sampleCount, unsigned long sampleDelayMs, float offsetVoltage,
                                                  const esp_adc_cal_characteristics_t* adcChars) {
  int samples[ControlConfig::MAX_MEDIAN_SAMPLES];

  if (sampleCount < ControlConfig::MIN_MEDIAN_SAMPLES) {
    sampleCount = ControlConfig::MIN_MEDIAN_SAMPLES;
  }
  if (sampleCount > ControlConfig::MAX_MEDIAN_SAMPLES) {
    sampleCount = ControlConfig::MAX_MEDIAN_SAMPLES;
  }
  if ((sampleCount % 2) == 0) {
    sampleCount++;
    if (sampleCount > ControlConfig::MAX_MEDIAN_SAMPLES) {
      sampleCount = ControlConfig::MAX_MEDIAN_SAMPLES;
    }
  }

  for (unsigned int i = 0; i < sampleCount; i++) {
    samples[i] = analogRead(sensorPin);
    vTaskDelay(pdMS_TO_TICKS(sampleDelayMs));
  }

  for (unsigned int i = 0; i < sampleCount - 1; i++) {
    for (unsigned int j = 0; j < sampleCount - i - 1; j++) {
      if (samples[j] > samples[j + 1]) {
        int temp = samples[j];
        samples[j] = samples[j + 1];
        samples[j + 1] = temp;
      }
    }
  }

  int medianRaw = samples[sampleCount / 2];
  uint32_t voltageMv = esp_adc_cal_raw_to_voltage(medianRaw, adcChars);
  float measuredVoltage = voltageMv / 1000.0f;
  float sensorVoltage = measuredVoltage * SensorConfig::ADC_VOLTAGE_DIVIDER;

  float pressure = 0.0f;
  if (sensorVoltage > offsetVoltage) {
    pressure = (sensorVoltage - offsetVoltage) * SensorConfig::PRESSURE_PSI_RANGE / SensorConfig::PRESSURE_VOLTAGE_RANGE;
  }

  SensorReading reading;
  reading.voltage = sensorVoltage;
  reading.pressure = pressure;
  return reading;
}
