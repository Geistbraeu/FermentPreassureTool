#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

class Settings {
public:
    float maxPressureThreshold;
    int pressureUnit;
    float hysteresis;
    unsigned long updateIntervalMs;
    unsigned int medianSampleCount;
    unsigned long medianSampleDelayMs;
    unsigned long tsIntervalSeconds;
    unsigned long bfIntervalMinutes;
    float offsetVoltage;
    float tempOffset;
    bool useTempSensor;

    // Cloud credentials
    String tsApiKey;
    String bfStreamId;
    String bfDeviceName;

    // Cloud enable flags
    bool tsEnabled;
    bool bfEnabled;

    // HTTP custom settings
    bool httpEnabled;
    String httpServer;
    String httpPath;
    String httpBodyTemplate;
    unsigned long httpIntervalSeconds;

    void load();
    bool isValid() const;
    
    bool setMaxPressureThreshold(float val);
    bool setPressureUnit(int val);
    bool setHysteresis(float val);
    bool setUpdateIntervalMs(unsigned long val);
    bool setMedianSampleCount(unsigned int val);
    bool setMedianSampleDelayMs(unsigned long val);
    bool setTsIntervalSeconds(unsigned long val);
    bool setBfIntervalMinutes(unsigned long val);
    bool setOffsetVoltage(float val);
    bool setTempOffset(float val);
    bool setUseTempSensor(bool val);
    bool setTsApiKey(const String& val);
    bool setBfStreamId(const String& val);
    bool setBfDeviceName(const String& val);
    bool setTsEnabled(bool val);
    bool setBfEnabled(bool val);
    bool setHttpEnabled(bool val);
    bool setHttpServer(const String& val);
    bool setHttpPath(const String& val);
    bool setHttpBodyTemplate(const String& val);
    bool setHttpIntervalSeconds(unsigned long val);

private:
    bool saveFloat(const String& key, float value);
    bool saveInt(const String& key, int value);
    bool saveULong(const String& key, unsigned long value);
    bool saveBool(const String& key, bool value);
    bool saveString(const String& key, const String& value);
};

extern Settings settings;

class WifiSettings {
public:
    String ssid;
    String pass;
    String devName;

    void load();
    void save(const String& newSsid, const String& newPass, const String& newDevName);
};

extern WifiSettings wifiSettings;

#endif // SETTINGS_H
