#include "Settings.h"
#include <Preferences.h>

Settings settings;
WifiSettings wifiSettings;

void Settings::saveFloat(const String& key, float value) {
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putFloat(key, value);
    prefs.end();
}

void Settings::saveInt(const String& key, int value) {
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putInt(key, value);
    prefs.end();
}

void Settings::saveULong(const String& key, unsigned long value) {
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putULong(key, value);
    prefs.end();
}

void Settings::saveBool(const String& key, bool value) {
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putBool(key, value);
    prefs.end();
}

void Settings::saveString(const String& key, const String& value) {
    Preferences prefs;
    prefs.begin("config", false);
    prefs.putString(key, value);
    prefs.end();
}

void Settings::load() {
    Preferences prefs;
    prefs.begin("config", true);
    maxPressureThreshold = prefs.getFloat("maxPressure", 14.0);
    pressureUnit = prefs.getInt("pUnit", 0);
    hysteresis = prefs.getFloat("hysteresis", 0.5);
    sensorInterval = prefs.getULong("sInterval", 200);
    tsIntervalSeconds = prefs.getULong("tsInterval", 120);
    bfIntervalMinutes = prefs.getULong("bfInterval", 15);
    offsetVoltage = prefs.getFloat("offsetVoltage", 0.515);
    useTempSensor = prefs.getBool("useTemp", true);
    tsApiKey = prefs.getString("tsApiKey", "DEXTOPQCD39G16GW");
    bfStreamId = prefs.getString("bfStreamId", "b8wwXJ3xdW3B8h");
    bfDeviceName = prefs.getString("bfDevName", "Pressure_Sensor");
    tsEnabled = prefs.getBool("tsEnabled", true);
    bfEnabled = prefs.getBool("bfEnabled", true);
    httpEnabled = prefs.getBool("httpEnabled", false);
    httpServer = prefs.getString("httpServer", "");
    httpPath = prefs.getString("httpPath", "/");
    httpBodyTemplate = prefs.getString("httpBody", "");
    httpIntervalSeconds = prefs.getULong("httpInterval", 60);
    prefs.end();
}

void Settings::setMaxPressureThreshold(float val) {
    maxPressureThreshold = val;
    saveFloat("maxPressure", val);
}

void Settings::setPressureUnit(int val) {
    pressureUnit = val;
    saveInt("pUnit", val);
}

void Settings::setHysteresis(float val) {
    hysteresis = val;
    saveFloat("hysteresis", val);
}

void Settings::setSensorInterval(unsigned long val) {
    sensorInterval = val;
    saveULong("sInterval", val);
}

void Settings::setTsIntervalSeconds(unsigned long val) {
    tsIntervalSeconds = val;
    saveULong("tsInterval", val);
}

void Settings::setBfIntervalMinutes(unsigned long val) {
    bfIntervalMinutes = val;
    saveULong("bfInterval", val);
}

void Settings::setOffsetVoltage(float val) {
    offsetVoltage = val;
    saveFloat("offsetVoltage", val);
}

void Settings::setUseTempSensor(bool val) {
    useTempSensor = val;
    saveBool("useTemp", val);
}

void Settings::setTsApiKey(const String& val) {
    tsApiKey = val;
    saveString("tsApiKey", val);
}

void Settings::setBfStreamId(const String& val) {
    bfStreamId = val;
    saveString("bfStreamId", val);
}

void Settings::setBfDeviceName(const String& val) {
    bfDeviceName = val;
    saveString("bfDevName", val);
}

void Settings::setTsEnabled(bool val) {
    tsEnabled = val;
    saveBool("tsEnabled", val);
}

void Settings::setBfEnabled(bool val) {
    bfEnabled = val;
    saveBool("bfEnabled", val);
}

void Settings::setHttpEnabled(bool val) {
    httpEnabled = val;
    saveBool("httpEnabled", val);
}

void Settings::setHttpServer(const String& val) {
    httpServer = val;
    saveString("httpServer", val);
}

void Settings::setHttpPath(const String& val) {
    httpPath = val;
    saveString("httpPath", val);
}

void Settings::setHttpBodyTemplate(const String& val) {
    httpBodyTemplate = val;
    saveString("httpBody", val);
}

void Settings::setHttpIntervalSeconds(unsigned long val) {
    httpIntervalSeconds = val;
    saveULong("httpInterval", val);
}

void WifiSettings::load() {
    Preferences prefs;
    prefs.begin("wifi_conf", true);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    devName = prefs.getString("devName", "ferment01");
    prefs.end();
}

void WifiSettings::save(const String& newSsid, const String& newPass, const String& newDevName) {
    ssid = newSsid;
    pass = newPass;
    devName = newDevName;
    Preferences prefs;
    prefs.begin("wifi_conf", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("devName", devName);
    prefs.end();
}
