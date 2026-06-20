#ifndef WEB_VIEW_MODELS_H
#define WEB_VIEW_MODELS_H

#include <Arduino.h>
#include "config.h"

struct RuntimeSnapshot {
    float pressure = 0.0f;
    float voltage = 0.0f;
    float temperature = 0.0f;
    bool manualOverride = false;
    bool manualOn = false;
    unsigned long manualStartTime = 0;
    bool isTempSensorConnected = false;
};

struct SettingsSnapshot {
    float maxPressureThreshold = 0.0f;
    int pressureUnit = 0;
    float hysteresis = 0.0f;
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    unsigned int medianSampleCount = ControlConfig::DEFAULT_MEDIAN_SAMPLE_COUNT;
    unsigned long medianSampleDelayMs = ControlConfig::DEFAULT_MEDIAN_SAMPLE_DELAY_MS;
    unsigned long tsIntervalSeconds = CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC;
    unsigned long bfIntervalMinutes = CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN;
    float offsetVoltage = SensorConfig::PRESSURE_OFFSET_DEFAULT;
    float tempOffset = 0.0f;
    bool useTempSensor = true;
    String tsApiKey;
    String bfStreamId;
    String bfDeviceName;
    bool tsEnabled = false;
    bool bfEnabled = false;
    bool httpEnabled = false;
    String httpServer;
    String httpPath = "/";
    String httpBodyTemplate;
    unsigned long httpIntervalSeconds = CloudConfig::CUSTOM_HTTP_DEFAULT_INTERVAL_SEC;
    String devName;
};

#endif // WEB_VIEW_MODELS_H
