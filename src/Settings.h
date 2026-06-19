#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

class Settings {
public:
    float maxPressureThreshold;
    int pressureUnit;
    float hysteresis;
    unsigned long sensorInterval;
    unsigned long tsIntervalSeconds;
    unsigned long bfIntervalMinutes;
    float offsetVoltage;
    bool useTempSensor;

    // Cloud credentials
    String tsApiKey;
    String bfStreamId;
    String bfDeviceName;

    // Cloud enable flags
    bool tsEnabled;
    bool bfEnabled;

    void load();
    
    void setMaxPressureThreshold(float val);
    void setPressureUnit(int val);
    void setHysteresis(float val);
    void setSensorInterval(unsigned long val);
    void setTsIntervalSeconds(unsigned long val);
    void setBfIntervalMinutes(unsigned long val);
    void setOffsetVoltage(float val);
    void setUseTempSensor(bool val);
    void setTsApiKey(const String& val);
    void setBfStreamId(const String& val);
    void setBfDeviceName(const String& val);
    void setTsEnabled(bool val);
    void setBfEnabled(bool val);
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
